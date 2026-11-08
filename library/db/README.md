# reactx::db

An embedded store of JSON documents for [ReactX](https://github.com/sargisis/ReactX) servers: one file, nothing to install or run next to the app. Collections of documents with ids; reads come from memory, every change is written to the file (and synced to disk) before the call returns.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/db.hpp"
using namespace rx;

auto db = db::Database::open("data/app.db");           // creates it the first time
if (!db) { std::println(stderr, "{}", db.error()); return 1; }
auto todos = db->collection("todos");

auto id = todos.insert(json::object({{"title", "Buy milk"}, {"done", false}}));   // "1"
todos.update(*id, [](json::Value& t) { t["done"] = true; });
todos.get(*id);                                        // std::optional<json::Value>
todos.index("done");                                   // makes where("done", ...) fast
todos.where("done", false);                            // std::vector<json::Value>
todos.find([](const json::Value& t) { return t.get<std::string>("title").starts_with("Buy"); });
todos.remove(*id);

db->batch([](db::Batch& b) {                           // all or nothing
    b.put("accounts", "alice", json::object({{"cents", 900}}));
    b.put("accounts", "bob", json::object({{"cents", 1100}}));
});
```

With `reactx::api`: `todos.all()` for `GET /api/todos`, `todos.insert(body)` for `POST`, and so on — the documents are already JSON.

## Add it to a project

It comes with ReactX (since 1.7), for native builds (a server). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_server PRIVATE reactx::db)
```

## Collections

| Call | |
|---|---|
| `insert(doc)` | a new document; its id is the next free number unless `doc` has an `"id"`; an id in use is an error |
| `put(id, doc)` | insert or replace |
| `update(id, change)` | change it in place; `false` if there is none |
| `remove(id)` | `false` if there was none |
| `get(id)` | `std::optional<json::Value>` |
| `all()`, `find(match)` | in id order (`"2"` before `"10"`) |
| `where(field, value)` | documents whose field equals the value |
| `index(field)` | keeps an index in memory for `where` |
| `size()` | |

A document is a JSON object, and its id is in it as `"id"`. Writes return `std::expected` (a full disk is an error, not an exception); reads can't fail. All of it is thread-safe.

## The file

One JSON line per change, appended and synced (`fsync`). If the process dies in the middle of a write, the next `open` drops the half-written line and goes on. When the file holds many more changes than documents (`Options::compactAfter`, 1000), it is rewritten with only the current documents — into a new file that then replaces the old one, so a crash during that loses nothing. `db->compact()` does it now.

`Options{.sync = false}` skips the `fsync`: much faster writes, but a power cut may lose the last changes (a crash of the program alone doesn't).

Everything lives in memory: it suits the data of one app (thousands to a few million small documents). For more, or for many servers sharing data, use a database server.
