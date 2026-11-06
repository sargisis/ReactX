# reactx::config

Настройки для сервера или утилиты на [ReactX](https://github.com/sargisis/ReactX) в таком порядке: аргументы командной строки, переменные окружения, JSON-файл, ваши значения по умолчанию. Ошибки — значения (`std::expected`), а не исключения. Нативные сборки.

In English: [README.md](README.md).

```cpp
#include "rx/config.hpp"

int main(int argc, char** argv) {
    auto config = rx::config::load({.args = {argc, argv}, .envPrefix = "TODO_", .file = "todo.json"});
    if (!config) { std::println(stderr, "{}", config.error()); return 2; }   // сломанный файл

    int port = config->number("port", 8080);              // --port 9000 | TODO_PORT=9000 | {"port": 9000}
    std::string db = config->text("db.path", "todo.db");  // --db.path x | TODO_DB_PATH=x | {"db": {"path": "x"}}
    bool verbose = config->flag("verbose");                // --verbose
    auto secret = config->require("session.secret");
    if (!secret) { std::println(stderr, "{}", secret.error()); return 2; }
    // missing setting session.secret (--session.secret, TODO_SESSION_SECRET, todo.json)
}
```

## Подключение

Идёт вместе с ReactX (с версии 1.5): `target_link_libraries(my_server PRIVATE reactx::config)`.

## Источники

- **Аргументы:** `--name value`, `--name=value`, `--flag` (true). Всё остальное (и всё после `--`) — в `positional()`.
- **Окружение:** ключ с префиксом, в верхнем регистре, `.` и `-` как `_`: `db.path` → `TODO_DB_PATH` (`envName(prefix, key)` покажет).
- **JSON-файл:** вложенные объекты дают ключи через точку (`{"db": {"path": "x"}}` → `db.path`). Отсутствующий файл пропускается, если не `.fileRequired = true`; сломанный — ошибка.

## Чтение

| | |
|---|---|
| `text(key, fallback)`, `number<T>(key, fallback)`, `flag(key, fallback)` | со значением по умолчанию (`flag` понимает true/yes/on/1 и false/no/off/0) |
| `require(key)`, `requireNumber<T>(key)` | `std::expected` с сообщением, где искали |
| `find(key)` | `std::optional<std::string>` |
| `source(key)` | `"argument"`, `"environment"`, `"file"` или `""` — например, чтобы напечатать, откуда взялся порт |

## Лицензия

MIT или Apache-2.0, как ReactX.
