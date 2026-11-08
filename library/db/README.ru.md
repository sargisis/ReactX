# reactx::db

Встроенное хранилище JSON-документов для серверов на [ReactX](https://github.com/sargisis/ReactX): один файл, ничего не нужно ставить и запускать рядом с приложением. Коллекции документов с id; чтение — из памяти, каждое изменение записывается в файл (и сбрасывается на диск) до возврата из вызова.

In English: [README.md](README.md).

```cpp
#include "rx/db.hpp"
using namespace rx;

auto db = db::Database::open("data/app.db");           // в первый раз создаёт файл
if (!db) { std::println(stderr, "{}", db.error()); return 1; }
auto todos = db->collection("todos");

auto id = todos.insert(json::object({{"title", "Купить молоко"}, {"done", false}}));   // "1"
todos.update(*id, [](json::Value& t) { t["done"] = true; });
todos.get(*id);                                        // std::optional<json::Value>
todos.index("done");                                   // where("done", ...) станет быстрым
todos.where("done", false);                            // std::vector<json::Value>
todos.find([](const json::Value& t) { return t.get<std::string>("title").starts_with("Куп"); });
todos.remove(*id);

db->batch([](db::Batch& b) {                           // всё или ничего
    b.put("accounts", "alice", json::object({{"cents", 900}}));
    b.put("accounts", "bob", json::object({{"cents", 1100}}));
});
```

С `reactx::api`: `todos.all()` для `GET /api/todos`, `todos.insert(body)` для `POST` и так далее — документы уже в JSON.

## Подключение

Идёт вместе с ReactX (с 1.7), для нативной сборки (сервер). В `CMakeLists.txt` проекта:

```cmake
target_link_libraries(my_server PRIVATE reactx::db)
```

## Коллекции

| Вызов | |
|---|---|
| `insert(doc)` | новый документ; id — следующий свободный номер, если в `doc` нет `"id"`; занятый id — ошибка |
| `put(id, doc)` | вставить или заменить |
| `update(id, change)` | изменить на месте; `false`, если такого нет |
| `remove(id)` | `false`, если такого не было |
| `get(id)` | `std::optional<json::Value>` |
| `all()`, `find(match)` | в порядке id (`"2"` раньше `"10"`) |
| `where(field, value)` | документы, у которых поле равно значению |
| `index(field)` | держит индекс в памяти для `where` |
| `size()` | |

Документ — JSON-объект, его id лежит в нём же как `"id"`. Запись возвращает `std::expected` (полный диск — ошибка, а не исключение); чтение не может упасть. Всё потокобезопасно.

## Файл

Одна JSON-строка на изменение, дописывается и сбрасывается на диск (`fsync`). Если процесс упал посреди записи, следующий `open` отбросит недописанную строку и продолжит. Когда изменений в файле намного больше, чем документов (`Options::compactAfter`, 1000), файл переписывается только с текущими документами — в новый файл, который затем заменяет старый, так что падение в это время ничего не теряет. `db->compact()` делает это сразу.

`Options{.sync = false}` пропускает `fsync`: запись намного быстрее, но при отключении питания последние изменения могут пропасть (падение самой программы — нет).

Всё лежит в памяти: подходит для данных одного приложения (от тысяч до нескольких миллионов небольших документов). Для большего или для нескольких серверов с общими данными нужен сервер баз данных.
