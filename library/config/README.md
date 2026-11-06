# reactx::config

Settings for a [ReactX](https://github.com/sargisis/ReactX) server or tool from, in this order: command-line arguments, environment variables, a JSON file, your defaults. Errors are values (`std::expected`), not exceptions. Native builds.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/config.hpp"

int main(int argc, char** argv) {
    auto config = rx::config::load({.args = {argc, argv}, .envPrefix = "TODO_", .file = "todo.json"});
    if (!config) { std::println(stderr, "{}", config.error()); return 2; }   // a broken file

    int port = config->number("port", 8080);              // --port 9000 | TODO_PORT=9000 | {"port": 9000}
    std::string db = config->text("db.path", "todo.db");  // --db.path x | TODO_DB_PATH=x | {"db": {"path": "x"}}
    bool verbose = config->flag("verbose");                // --verbose
    auto secret = config->require("session.secret");
    if (!secret) { std::println(stderr, "{}", secret.error()); return 2; }
    // missing setting session.secret (--session.secret, TODO_SESSION_SECRET, todo.json)
}
```

## Add it to a project

It comes with ReactX (since 1.5): `target_link_libraries(my_server PRIVATE reactx::config)`.

## Sources

- **Arguments:** `--name value`, `--name=value`, `--flag` (true). Anything else (and everything after `--`) is in `positional()`.
- **Environment:** the key with the prefix, upper case, `.` and `-` as `_`: `db.path` → `TODO_DB_PATH` (`envName(prefix, key)` shows it).
- **JSON file:** nested objects give dotted keys (`{"db": {"path": "x"}}` → `db.path`). A missing file is skipped unless `.fileRequired = true`; an invalid one is an error.

## Reading

| | |
|---|---|
| `text(key, fallback)`, `number<T>(key, fallback)`, `flag(key, fallback)` | with a default (`flag` knows true/yes/on/1 and false/no/off/0) |
| `require(key)`, `requireNumber<T>(key)` | `std::expected` with a message that names where it looked |
| `find(key)` | `std::optional<std::string>` |
| `source(key)` | `"argument"`, `"environment"`, `"file"` or `""` — e.g. to print where the port came from |

## License

MIT or Apache-2.0, like ReactX.
