// Stress tests of reactx::db: thousands of random changes checked against a
// std::map, reopening, compaction, threads writing at once, the file cut at
// random places (a crash), batches and indexes. `ctest -L stress`.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <thread>

#include <unistd.h>
#include <vector>

#include "reactx/testing.hpp"
#include "rx/db.hpp"

using namespace rx;
namespace fs = std::filesystem;

namespace {

std::mt19937 rng(42);

// Not a static: the Cleanup below runs at exit, after function statics may be gone.
fs::path dir() { return fs::temp_directory_path() / ("reactx-db-stress-" + std::to_string(::getpid())); }

fs::path fresh(const std::string& name) {
    fs::create_directories(dir());
    fs::path f = dir() / name;
    fs::remove(f);
    return f;
}

std::string read(const fs::path& f) {
    std::ifstream in(f, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return std::move(s).str();
}

void write(const fs::path& f, std::string_view text) { std::ofstream(f, std::ios::binary | std::ios::trunc) << text; }

// The documents of a collection as id -> JSON text.
std::map<std::string, std::string> snapshot(db::Collection c) {
    std::map<std::string, std::string> out;
    for (const auto& doc : c.all()) out[doc.get<std::string>("id")] = doc.dump();
    return out;
}

struct Cleanup {
    ~Cleanup() {
        std::error_code ec;
        fs::remove_all(dir(), ec);
    }
} cleanup;

}  // namespace

RX_TEST(random_changes_match_a_map_across_reopens_and_compactions) {
    const fs::path f = fresh("random.db");
    std::map<std::string, std::string> model;  // id -> doc JSON
    std::uniform_int_distribution<int> op(0, 9), idPick(1, 400), val(0, 1'000'000);
    for (int round = 0; round < 5; ++round) {
        auto db = db::Database::open(f, {.sync = false, .compactAfter = 300});
        CHECK(db.has_value());
        if (!db) return;
        auto items = db->collection("items");
        CHECK(snapshot(items) == model);  // what the file gave back
        for (int i = 0; i < 4000; ++i) {
            const std::string id = std::to_string(idPick(rng));
            const int o = op(rng);
            if (o < 4) {  // put
                json::Value doc = json::object({{"v", val(rng)}, {"tag", o % 2 ? "odd" : "even"}});
                CHECK(items.put(id, doc).has_value());
                doc["id"] = id;
                model[id] = doc.dump();
            } else if (o < 7) {  // update
                auto r = items.update(id, [](json::Value& d) { d["v"] = d.get<double>("v") + 1; });
                CHECK(r.has_value());
                CHECK(*r == model.contains(id));
                if (model.contains(id)) {
                    auto d = json::parse(model[id]);
                    d["v"] = d.get<double>("v") + 1;
                    model[id] = d.dump();
                }
            } else if (o < 9) {  // remove
                auto r = items.remove(id);
                CHECK(r.has_value() && *r == model.contains(id));
                model.erase(id);
            } else {  // insert with a new number
                auto r = items.insert(json::object({{"v", -1}}));
                CHECK(r.has_value());
                if (r) {
                    CHECK(!model.contains(*r));
                    model[*r] = json::object({{"v", -1}, {"id", *r}}).dump();
                }
            }
        }
        CHECK(items.size() == model.size());
        CHECK(snapshot(items) == model);
    }
    // Compaction kept the file near the size of the data, not of the history (20,000 changes).
    std::size_t lines = 0;
    for (char c : read(f)) lines += c == '\n';
    std::printf("  %zu documents, %zu lines in the file after 20,000 changes\n", model.size(), lines);
    CHECK(lines < model.size() + 400);
}

RX_TEST(threads_writing_at_once) {
    const fs::path f = fresh("threads.db");
    {
        auto db = db::Database::open(f, {.sync = false, .compactAfter = 500});
        CHECK(db.has_value());
        if (!db) return;
        auto c = db->collection("c");
        CHECK(c.put("counter", json::object({{"n", 0}})).has_value());
        std::atomic<int> errors{0};
        std::vector<std::jthread> threads;
        for (int t = 0; t < 8; ++t)
            threads.emplace_back([&, t] {
                auto mine = db->collection("c");
                for (int i = 0; i < 500; ++i) {
                    if (!mine.put("t" + std::to_string(t) + "-" + std::to_string(i), json::object({{"t", t}, {"i", i}})))
                        ++errors;
                    // A read-modify-write: under the store's lock, so no update is lost.
                    if (!mine.update("counter", [](json::Value& d) { d["n"] = d.get<double>("n") + 1; })) ++errors;
                    if (i % 50 == 0) (void)mine.where("t", t);
                }
            });
        threads.clear();  // join
        CHECK(errors == 0);
        CHECK(c.size() == 1 + 8 * 500);
        CHECK(c.get("counter")->get<int>("n") == 8 * 500);
    }
    auto again = db::Database::open(f);
    CHECK(again && again->collection("c").size() == 1 + 8 * 500);
    CHECK(again && again->collection("c").get("counter")->get<int>("n") == 8 * 500);
}

RX_TEST(a_cut_file_opens_with_the_changes_before_the_cut) {
    const fs::path f = fresh("cut.db");
    {
        auto db = db::Database::open(f, {.sync = false, .compactAfter = 1'000'000});
        auto c = db->collection("c");
        for (int i = 0; i < 300; ++i) (void)c.put(std::to_string(i % 50), json::object({{"i", i}, {"pad", std::string(i % 40, 'x')}}));
        (void)db->batch([](db::Batch& b) {
            for (int i = 0; i < 20; ++i) b.put("c", "batch" + std::to_string(i), json::object({{"in", "batch"}}));
        });
    }
    const std::string whole = read(f);
    std::uniform_int_distribution<std::size_t> at(16, whole.size() - 1);  // past the header line
    for (int k = 0; k < 120; ++k) {
        const std::size_t cut = k == 0 ? whole.size() - 2 : at(rng);  // k == 0: inside the batch line
        const fs::path g = fresh("cut-copy.db");
        write(g, std::string_view(whole).substr(0, cut));
        auto db = db::Database::open(g);
        CHECK(db.has_value());
        if (!db) continue;
        // It must equal the file cut at the last whole line before `cut`.
        const std::size_t lineEnd = whole.rfind('\n', cut - 1) + 1;
        const fs::path h = fresh("cut-line.db");
        write(h, std::string_view(whole).substr(0, lineEnd));
        auto ref = db::Database::open(h);
        CHECK(ref.has_value());
        if (ref) CHECK(snapshot(db->collection("c")) == snapshot(ref->collection("c")));
        // A batch is one line: all of it or none.
        const auto n = db->collection("c").find([](const json::Value& d) { return d.get<std::string>("in") == "batch"; }).size();
        CHECK(n == 0 || n == 20);
        // And the repaired file takes new writes.
        CHECK(db->collection("c").put("after", json::object({{"ok", true}})).has_value());
        auto reopened = db::Database::open(g);
        CHECK(reopened && reopened->collection("c").get("after").has_value());
    }
}

RX_TEST(an_index_always_agrees_with_a_full_scan) {
    const fs::path f = fresh("index.db");
    auto db = db::Database::open(f, {.sync = false, .compactAfter = 200});
    auto c = db->collection("c");
    c.index("k");
    std::uniform_int_distribution<int> idPick(1, 300), key(0, 6), op(0, 3);
    for (int i = 0; i < 6000; ++i) {
        const std::string id = std::to_string(idPick(rng));
        switch (op(rng)) {
            case 0: (void)c.put(id, json::object({{"k", key(rng)}})); break;
            case 1: (void)c.update(id, [&](json::Value& d) { d["k"] = key(rng); }); break;
            case 2: (void)c.remove(id); break;
            default: (void)c.put(id, json::object({{"other", 1}})); break;  // no "k" at all
        }
        if (i % 250 == 0)
            for (int k = 0; k < 7; ++k) {
                auto fast = c.where("k", k);
                auto slow = c.find([k](const json::Value& d) { return d["k"] == json::Value(k); });
                CHECK(fast == slow);
            }
    }
    auto missing = c.where("k", json::Value());  // documents without "k" (null)
    CHECK(missing == c.find([](const json::Value& d) { return d["k"].isNull(); }));
}

RX_TEST(many_collections_and_big_documents) {
    const fs::path f = fresh("big.db");
    {
        auto db = db::Database::open(f, {.sync = false});
        for (int i = 0; i < 200; ++i)
            CHECK(db->collection("col" + std::to_string(i)).insert(json::object({{"blob", std::string(10'000, static_cast<char>('a' + i % 26))}})).has_value());
        CHECK(db->collections().size() == 200);
        CHECK(db->compact().has_value());
    }
    auto db = db::Database::open(f);
    CHECK(db && db->collections().size() == 200);
    CHECK(db && db->collection("col27").get("1")->get<std::string>("blob") == std::string(10'000, 'b'));
}
