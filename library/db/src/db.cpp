#include "rx/db.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

#include "reactx/parse.hpp"

namespace rx::db {

namespace {
constexpr std::string_view kHeader = R"({"reactx-db":1})";

bool allDigits(std::string_view s) {
    return !s.empty() && std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
}

std::string errnoText(const std::string& what) { return what + ": " + std::strerror(errno); }

// Writes all of `text` to `fd` (a short write is continued).
bool writeAll(int fd, std::string_view text) {
    while (!text.empty()) {
        ssize_t n = ::write(fd, text.data(), text.size());
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        text.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

struct IdOrder {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const { return idLess(a, b); }
};
}  // namespace

bool idLess(std::string_view a, std::string_view b) {
    const bool na = allDigits(a), nb = allDigits(b);
    if (na && nb) {
        // Numbers: without leading zeros the longer one is bigger.
        auto strip = [](std::string_view s) {
            while (s.size() > 1 && s.front() == '0') s.remove_prefix(1);
            return s;
        };
        a = strip(a);
        b = strip(b);
        if (a.size() != b.size()) return a.size() < b.size();
        return a < b;
    }
    if (na != nb) return na;  // numbers first
    return a < b;
}

struct Coll {
    std::map<std::string, json::Value, IdOrder> docs;
    std::map<std::string, std::map<std::string, std::set<std::string>>> indexes;  // field -> value (as JSON) -> ids
    std::uint64_t nextId = 1;

    void unindex(const std::string& id, const json::Value& doc) {
        for (auto& [field, byValue] : indexes) {
            auto it = byValue.find(doc[field].dump());
            if (it == byValue.end()) continue;
            it->second.erase(id);
            if (it->second.empty()) byValue.erase(it);
        }
    }
    void addToIndexes(const std::string& id, const json::Value& doc) {
        for (auto& [field, byValue] : indexes) byValue[doc[field].dump()].insert(id);
    }
    void put(const std::string& id, json::Value doc) {
        if (auto it = docs.find(id); it != docs.end()) unindex(id, it->second);
        addToIndexes(id, doc);
        docs.insert_or_assign(id, std::move(doc));
        if (allDigits(id) && id.size() < 19)
            if (auto n = parseNumber<std::uint64_t>(id)) nextId = std::max(nextId, *n + 1);
    }
    bool remove(const std::string& id) {
        auto it = docs.find(id);
        if (it == docs.end()) return false;
        unindex(id, it->second);
        docs.erase(it);
        return true;
    }
};

struct State {
    std::mutex mutex;
    std::filesystem::path file;
    Options options;
    int fd = -1;
    std::size_t records = 0;  // changes in the file
    std::map<std::string, Coll> colls;

    ~State() {
        if (fd >= 0) ::close(fd);
    }

    std::size_t documents() const {
        std::size_t n = 0;
        for (const auto& [name, c] : colls) n += c.docs.size();
        return n;
    }

    // One op of a change: {"c":..,"op":"put"|"del","id":..,"doc":..}
    void apply(const json::Value& op) {
        Coll& c = colls[op.get<std::string>("c")];
        const std::string id = op.get<std::string>("id");
        if (op.get<std::string>("op") == "put") c.put(id, op["doc"]);
        else c.remove(id);
    }

    // Under the lock: writes one change (several ops) to the file, then applies it.
    std::expected<void, std::string> commit(json::Value ops) {
        json::Value line = json::object({{"ops", std::move(ops)}});
        if (!writeAll(fd, line.dump() + "\n")) return std::unexpected(errnoText("db: can't write " + file.string()));
        if (options.sync && ::fsync(fd) != 0) return std::unexpected(errnoText("db: can't sync " + file.string()));
        for (const auto& op : line["ops"].asArray()) apply(op);
        ++records;
        if (records > documents() + options.compactAfter) (void)compactLocked();  // a failure leaves the long file
        return {};
    }

    std::expected<void, std::string> compactLocked() {
        const std::filesystem::path tmp = file.string() + ".tmp";
        int out = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (out < 0) return std::unexpected(errnoText("db: can't create " + tmp.string()));
        bool ok = writeAll(out, std::string(kHeader) + "\n");
        for (const auto& [name, c] : colls)
            for (const auto& [id, doc] : c.docs) {
                if (!ok) break;
                json::Value op = json::object({{"c", name}, {"op", "put"}, {"id", id}, {"doc", doc}});
                json::Value line = json::object({{"ops", json::Array{std::move(op)}}});
                ok = writeAll(out, line.dump() + "\n");
            }
        ok = ok && ::fsync(out) == 0;
        ::close(out);
        if (!ok || ::rename(tmp.c_str(), file.c_str()) != 0) {
            ::unlink(tmp.c_str());
            return std::unexpected(errnoText("db: can't compact " + file.string()));
        }
        // The rename itself survives a crash once the folder is synced.
        if (int dir = ::open(file.parent_path().empty() ? "." : file.parent_path().c_str(), O_RDONLY | O_CLOEXEC); dir >= 0) {
            ::fsync(dir);
            ::close(dir);
        }
        int fresh = ::open(file.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
        if (fresh < 0) return std::unexpected(errnoText("db: can't reopen " + file.string()));
        ::close(fd);
        fd = fresh;
        records = documents();
        return {};
    }
};

// ---- Database ----

std::expected<Database, std::string> Database::open(std::filesystem::path file, Options options) {
    auto state = std::make_shared<State>();
    state->file = file;
    state->options = options;
    std::error_code ec;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ec);

    // Read what is there.
    std::string text;
    if (std::ifstream in(file, std::ios::binary); in) {
        std::ostringstream all;
        all << in.rdbuf();
        text = std::move(all).str();
    }
    std::size_t good = 0;  // bytes up to the last whole change
    if (!text.empty()) {
        std::size_t at = 0, lineNo = 0;
        while (at < text.size()) {
            std::size_t nl = text.find('\n', at);
            if (nl == std::string::npos) break;  // a change cut off by a crash: dropped below
            std::string_view line(text.data() + at, nl - at);
            ++lineNo;
            if (lineNo == 1) {
                if (line != kHeader) return std::unexpected("db: " + file.string() + " is not a ReactX db file");
            } else {
                auto v = json::tryParse(line);
                const bool last = nl + 1 >= text.size();
                if (!v || !v->isObject() || !(*v)["ops"].isArray()) {
                    if (last) break;  // half-written last line
                    return std::unexpected("db: " + file.string() + ": line " + std::to_string(lineNo) + " is damaged");
                }
                for (const auto& op : (*v)["ops"].asArray()) state->apply(op);
                ++state->records;
            }
            at = nl + 1;
            good = at;
        }
    }

    state->fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (state->fd < 0) return std::unexpected(errnoText("db: can't open " + file.string()));
    if (text.empty()) {
        if (!writeAll(state->fd, std::string(kHeader) + "\n") || ::fsync(state->fd) != 0)
            return std::unexpected(errnoText("db: can't write " + file.string()));
    } else if (good < text.size() && ::ftruncate(state->fd, static_cast<off_t>(good)) != 0) {
        return std::unexpected(errnoText("db: can't repair " + file.string()));
    }
    return Database(std::move(state));
}

Collection Database::collection(std::string name) {
    return Collection(state_, std::move(name));
}

std::vector<std::string> Database::collections() const {
    std::lock_guard lock(state_->mutex);
    std::vector<std::string> out;
    for (const auto& [name, c] : state_->colls)
        if (!c.docs.empty()) out.push_back(name);
    return out;
}

void Batch::put(std::string collection, std::string id, json::Value doc) {
    if (doc.isObject()) doc["id"] = id;
    ops_.push_back(json::object({{"c", std::move(collection)}, {"op", "put"}, {"id", std::move(id)}, {"doc", std::move(doc)}}));
}

void Batch::remove(std::string collection, std::string id) {
    ops_.push_back(json::object({{"c", std::move(collection)}, {"op", "del"}, {"id", std::move(id)}}));
}

std::expected<void, std::string> Database::batch(const std::function<void(Batch&)>& fill) {
    Batch b;
    fill(b);
    if (b.ops_.empty()) return {};
    for (const auto& op : b.ops_)
        if (op.get<std::string>("op") == "put" && !op["doc"].isObject())
            return std::unexpected("db: a document must be a JSON object");
    std::lock_guard lock(state_->mutex);
    return state_->commit(json::Value(std::move(b.ops_)));
}

std::expected<void, std::string> Database::compact() {
    std::lock_guard lock(state_->mutex);
    return state_->compactLocked();
}

// ---- Collection ----

std::expected<std::string, std::string> Collection::insert(json::Value doc) {
    if (!doc.isObject()) return std::unexpected("db: a document must be a JSON object");
    std::lock_guard lock(state_->mutex);
    Coll& c = state_->colls[name_];
    std::string id = doc["id"].isString() ? doc["id"].asString()
                     : doc["id"].isNumber() ? std::to_string(doc["id"].asInt())
                                            : std::to_string(c.nextId);
    if (c.docs.contains(id)) return std::unexpected("db: " + name_ + " already has id " + id);
    doc["id"] = id;
    auto done = state_->commit(json::Array{json::object({{"c", name_}, {"op", "put"}, {"id", id}, {"doc", std::move(doc)}})});
    if (!done) return std::unexpected(done.error());
    return id;
}

std::expected<void, std::string> Collection::put(const std::string& id, json::Value doc) {
    if (!doc.isObject()) return std::unexpected("db: a document must be a JSON object");
    doc["id"] = id;
    std::lock_guard lock(state_->mutex);
    return state_->commit(json::Array{json::object({{"c", name_}, {"op", "put"}, {"id", id}, {"doc", std::move(doc)}})});
}

std::expected<bool, std::string> Collection::update(const std::string& id, const std::function<void(json::Value&)>& change) {
    std::lock_guard lock(state_->mutex);
    Coll& c = state_->colls[name_];
    auto it = c.docs.find(id);
    if (it == c.docs.end()) return false;
    json::Value doc = it->second;
    change(doc);
    if (!doc.isObject()) return std::unexpected("db: a document must stay a JSON object");
    doc["id"] = id;
    auto done = state_->commit(json::Array{json::object({{"c", name_}, {"op", "put"}, {"id", id}, {"doc", std::move(doc)}})});
    if (!done) return std::unexpected(done.error());
    return true;
}

std::expected<bool, std::string> Collection::remove(const std::string& id) {
    std::lock_guard lock(state_->mutex);
    Coll& c = state_->colls[name_];
    if (!c.docs.contains(id)) return false;
    auto done = state_->commit(json::Array{json::object({{"c", name_}, {"op", "del"}, {"id", id}})});
    if (!done) return std::unexpected(done.error());
    return true;
}

std::optional<json::Value> Collection::get(const std::string& id) const {
    std::lock_guard lock(state_->mutex);
    auto c = state_->colls.find(name_);
    if (c == state_->colls.end()) return std::nullopt;
    auto it = c->second.docs.find(id);
    if (it == c->second.docs.end()) return std::nullopt;
    return it->second;
}

std::vector<json::Value> Collection::all() const {
    return find([](const json::Value&) { return true; });
}

std::vector<json::Value> Collection::find(const std::function<bool(const json::Value&)>& match) const {
    std::lock_guard lock(state_->mutex);
    std::vector<json::Value> out;
    if (auto c = state_->colls.find(name_); c != state_->colls.end())
        for (const auto& [id, doc] : c->second.docs)
            if (match(doc)) out.push_back(doc);
    return out;
}

std::vector<json::Value> Collection::where(const std::string& field, const json::Value& value) const {
    std::lock_guard lock(state_->mutex);
    std::vector<json::Value> out;
    auto c = state_->colls.find(name_);
    if (c == state_->colls.end()) return out;
    if (auto ix = c->second.indexes.find(field); ix != c->second.indexes.end()) {
        if (auto ids = ix->second.find(value.dump()); ids != ix->second.end()) {
            std::vector<std::string> sorted(ids->second.begin(), ids->second.end());
            std::ranges::sort(sorted, idLess);
            for (const auto& id : sorted) out.push_back(c->second.docs.at(id));
        }
        return out;
    }
    for (const auto& [id, doc] : c->second.docs)
        if (doc[field] == value) out.push_back(doc);
    return out;
}

void Collection::index(const std::string& field) {
    std::lock_guard lock(state_->mutex);
    Coll& c = state_->colls[name_];
    if (c.indexes.contains(field)) return;
    auto& byValue = c.indexes[field];
    for (const auto& [id, doc] : c.docs) byValue[doc[field].dump()].insert(id);
}

std::size_t Collection::size() const {
    std::lock_guard lock(state_->mutex);
    auto c = state_->colls.find(name_);
    return c == state_->colls.end() ? 0 : c->second.docs.size();
}

}  // namespace rx::db
