#pragma once

// reactx::db — an embedded store of JSON documents for a server (native): one
// file, nothing to install. Collections of documents with ids; reads come from
// memory, every change is appended to the file (one JSON line, synced to disk)
// before the call returns. Link reactx::db.
//
//   auto db = rx::db::Database::open("data/app.db");      // std::expected
//   if (!db) return fail(db.error());
//   auto todos = db->collection("todos");
//   auto id = todos.insert(rx::json::object({{"title", "Buy milk"}, {"done", false}}));  // "1"
//   todos.update(*id, [](rx::json::Value& t) { t["done"] = true; });
//   todos.where("done", false);                             // documents whose "done" is false
//   todos.find([](const rx::json::Value& t) { return t.get<std::string>("title").starts_with("Buy"); });
//   db->batch([](rx::db::Batch& b) {                         // all or nothing, one write
//       b.put("accounts", "a", rx::json::object({{"cents", 900}}));
//       b.put("accounts", "b", rx::json::object({{"cents", 1100}}));
//   });
//
// A document is a JSON object; its id is also in it as "id". Thread-safe.
// After a crash the file is read back up to the last whole change. The file
// is rewritten without old versions (compact) when it holds many more
// changes than documents. Sized for what fits in memory (thousands to a few
// million small documents), like the data of one app.

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/json.hpp"

namespace rx::db {

struct Options {
    bool sync = true;               // fsync after each change (off: faster, a power cut may lose the last ones)
    std::size_t compactAfter = 1000;  // compact when the file holds this many changes more than documents
};

struct State;  // inside

// Changes applied together: in memory and in the file as one line.
class Batch {
public:
    void put(std::string collection, std::string id, json::Value doc);
    void remove(std::string collection, std::string id);

private:
    friend class Database;
    std::vector<json::Value> ops_;
};

class Collection {
public:
    // A new document; it gets the next free number as its id unless it has an "id".
    std::expected<std::string, std::string> insert(json::Value doc);
    // Insert or replace the document `id`.
    std::expected<void, std::string> put(const std::string& id, json::Value doc);
    // Changes the document in place; false if there is none. The id stays.
    std::expected<bool, std::string> update(const std::string& id, const std::function<void(json::Value&)>& change);
    std::expected<bool, std::string> remove(const std::string& id);  // false if there was none

    std::optional<json::Value> get(const std::string& id) const;
    std::vector<json::Value> all() const;  // in id order (numbers as numbers)
    std::vector<json::Value> find(const std::function<bool(const json::Value&)>& match) const;
    // Documents whose `field` equals `value` (fast after index(field)).
    std::vector<json::Value> where(const std::string& field, const json::Value& value) const;
    void index(const std::string& field);  // kept in memory, built now and kept up to date
    std::size_t size() const;
    const std::string& name() const { return name_; }

private:
    friend class Database;
    Collection(std::shared_ptr<State> state, std::string name) : state_(std::move(state)), name_(std::move(name)) {}
    std::shared_ptr<State> state_;
    std::string name_;
};

class Database {
public:
    // Opens (or creates) the file and reads it. Fails on a file it can't read
    // or write, or one that isn't a ReactX db.
    static std::expected<Database, std::string> open(std::filesystem::path file, Options options = {});

    Collection collection(std::string name);
    std::vector<std::string> collections() const;
    std::expected<void, std::string> batch(const std::function<void(Batch&)>& fill);
    // Rewrites the file with only the current documents (it happens by itself too).
    std::expected<void, std::string> compact();

private:
    explicit Database(std::shared_ptr<State> state) : state_(std::move(state)) {}
    std::shared_ptr<State> state_;
};

// "2" before "10": the order all() uses for ids.
bool idLess(std::string_view a, std::string_view b);

}  // namespace rx::db
