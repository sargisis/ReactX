# reactx::query

Data from a server with a shared cache for [ReactX](https://github.com/sargisis/ReactX) apps, like React Query or SWR: one request per key however many components ask, cached data shown at once and refreshed in the background, retries, invalidation after a change, optimistic updates. In the browser and in a terminal.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/query.hpp"
using namespace rx;

Element Todos(const TodosProps&) {   // struct TodosProps { std::string key{}; };
    auto todos = query::useQuery({.key = "todos", .url = "/api/todos"});  // a json::Value
    if (todos.loading) return ui::text({.text = "Loading..."});
    if (!todos.error.empty()) return ui::text({.text = todos.error});
    std::vector<Element> rows;
    for (auto& t : todos.data->asArray()) rows.push_back(ui::text({.text = t["title"].asString()}));
    return ui::stack({.children = std::move(rows)});
}
```

## Add it to a project

It comes with ReactX (since 1.3). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_app PRIVATE reactx::query)
```

## Queries

```cpp
auto q = query::useQuery<std::vector<Todo>>({
    .key = "todos",                 // the cache key
    .url = "/api/todos",            // GET; or .request = FetchRequest{...}; or .load = your loader
    .parse = parseTodos,            // json::Value -> T; json::Value and std::string need none
    .staleTime = std::chrono::seconds(30),
});
q.data      // std::shared_ptr<const T>: nullptr until loaded; the last good value while refreshing or after an error
q.error     // "" or why the last load failed (after the retries): "HTTP 500", "network error: ..."
q.loading   // the first load, no data yet: show a spinner
q.fetching  // any load in flight, background refreshes too
q.refetch() // load again now
q.ok()      // data and no error
```

| Option | |
|---|---|
| `key` | the cache key; components with the same key share the data and the request. Use `/` for parts: `"todos/5"` |
| `url` / `request` / `load` | how to load: GET a URL, any `FetchRequest`, or `Loader<T>` — a function that gets `resolve(Result<T>)` and returns how to cancel |
| `parse` | the JSON as your type; it may throw `json::Error` (a wrong field), which becomes `error` |
| `staleTime` | data this fresh is shown to a new component without loading again (at least 2 s, so components that mount together share one load). Default 0: shown at once and refreshed in the background |
| `cacheTime` | kept this long after the last component using it unmounts (5 min) |
| `refetchInterval` | load again every ... while shown (polling); 0 = off |
| `retry` | tries again after an error, waiting 0.5 s, 1 s, 2 s, ... (2) |
| `enabled` | `false`: don't load yet (a search with no text, a detail page with no id) |

## Changes

```cpp
auto add = query::useMutation<std::string>({
    .request = [](const std::string& title) {
        return FetchRequest{.method = "POST", .url = "/api/todos",
                            .body = json::object({{"title", title}}).dump()};
    },
    .invalidates = {"todos"},  // load the lists again after it
});
ui::button({.label = add.pending ? "Saving..." : "Add", .onClick = [add] { add.mutate("Buy milk"); }})
// add.error, add.data (the answer), add.reset()
```

`useMutation<Vars, R = json::Value>` takes `.request` (or `.run` for your own way), `.parse`, `.onSuccess(result, vars)`, `.onError(error, vars)` and `.invalidates`. Only the latest `mutate()` writes `pending`, `error` and `data`.

### Optimistic updates

Change the cache before the server answers and undo it if the request fails:

```cpp
auto toggle = query::useMutation<int>({
    .request = [](const int& id) { return FetchRequest{.method = "POST", .url = "/api/todos/" + std::to_string(id) + "/toggle"}; },
    .onMutate = [](const int& id) -> Cleanup {
        auto before = query::getData<std::vector<Todo>>("todos");
        query::updateData<std::vector<Todo>>("todos", [id](std::vector<Todo> list) {
            for (auto& t : list) if (t.id == id) t.done = !t.done;
            return list;
        });
        return [before] { if (before) query::setData("todos", *before); };  // the undo
    },
    .invalidates = {"todos"},
});
```

## The cache from anywhere

```cpp
query::invalidate("todos");   // "todos" and "todos/..." load again (the ones on screen now, the others when shown)
query::setData<User>("me", user);                 // e.g. from a login answer
query::getData<User>("me");                       // std::shared_ptr<const User> or nullptr
query::updateData<User>("me", [](User u) { u.name = "Ann"; return u; });
query::remove("todos");       // forget it
query::clear();               // forget everything (after signing out)
```

Everything runs on the UI thread, like component state. The library uses no `std::format`, so it adds little to a `.wasm`.

## License

MIT or Apache-2.0, like ReactX.
