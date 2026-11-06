# reactx::query

Данные с сервера с общим кэшем для приложений на [ReactX](https://github.com/sargisis/ReactX), как React Query или SWR: один запрос на ключ, сколько бы компонентов его ни просили; данные из кэша показываются сразу и тихо обновляются; повторы при ошибке, перезагрузка после изменения, оптимистичные обновления. В браузере и в терминале.

In English: [README.md](README.md).

```cpp
#include "rx/query.hpp"
using namespace rx;

Element Todos(const TodosProps&) {   // struct TodosProps { std::string key{}; };
    auto todos = query::useQuery({.key = "todos", .url = "/api/todos"});  // json::Value
    if (todos.loading) return ui::text({.text = "Загрузка..."});
    if (!todos.error.empty()) return ui::text({.text = todos.error});
    std::vector<Element> rows;
    for (auto& t : todos.data->asArray()) rows.push_back(ui::text({.text = t["title"].asString()}));
    return ui::stack({.children = std::move(rows)});
}
```

## Подключение

Библиотека идёт вместе с ReactX (с версии 1.3). Подключите её в `CMakeLists.txt` проекта:

```cmake
target_link_libraries(my_app PRIVATE reactx::query)
```

## Запросы

```cpp
auto q = query::useQuery<std::vector<Todo>>({
    .key = "todos",                 // ключ в кэше
    .url = "/api/todos",            // GET; или .request = FetchRequest{...}; или .load = свой загрузчик
    .parse = parseTodos,            // json::Value -> T; для json::Value и std::string не нужен
    .staleTime = std::chrono::seconds(30),
});
q.data      // std::shared_ptr<const T>: nullptr до загрузки; последнее хорошее значение во время обновления и после ошибки
q.error     // "" или почему последняя загрузка не удалась (после повторов): "HTTP 500", "network error: ..."
q.loading   // первая загрузка, данных ещё нет: покажите индикатор
q.fetching  // идёт любая загрузка, фоновые обновления тоже
q.refetch() // загрузить заново сейчас
q.ok()      // данные есть, ошибки нет
```

| Параметр | |
|---|---|
| `key` | ключ в кэше; компоненты с одним ключом делят данные и запрос. Части — через `/`: `"todos/5"` |
| `url` / `request` / `load` | как загружать: GET по адресу, любой `FetchRequest` или `Loader<T>` — функция, получающая `resolve(Result<T>)` и возвращающая отмену |
| `parse` | JSON в ваш тип; может бросить `json::Error` (не то поле) — это станет `error` |
| `staleTime` | настолько свежие данные новый компонент показывает без повторной загрузки (минимум 2 с, чтобы компоненты, появившиеся вместе, делили одну загрузку). По умолчанию 0: показываются сразу и обновляются в фоне |
| `cacheTime` | столько данные хранятся после того, как исчез последний компонент с ними (5 мин) |
| `refetchInterval` | загружать заново каждые ..., пока данные на экране (опрос); 0 — выключено |
| `retry` | повторы после ошибки с паузами 0,5 с, 1 с, 2 с, ... (2) |
| `enabled` | `false`: пока не загружать (поиск без текста, страница без id) |

## Изменения

```cpp
auto add = query::useMutation<std::string>({
    .request = [](const std::string& title) {
        return FetchRequest{.method = "POST", .url = "/api/todos",
                            .body = json::object({{"title", title}}).dump()};
    },
    .invalidates = {"todos"},  // после него списки загрузятся заново
});
ui::button({.label = add.pending ? "Сохраняю..." : "Добавить", .onClick = [add] { add.mutate("Купить молоко"); }})
// add.error, add.data (ответ), add.reset()
```

`useMutation<Vars, R = json::Value>` принимает `.request` (или `.run` — свой способ), `.parse`, `.onSuccess(result, vars)`, `.onError(error, vars)` и `.invalidates`. `pending`, `error` и `data` пишет только последний вызов `mutate()`.

### Оптимистичные обновления

Поменяйте кэш до ответа сервера и откатите, если запрос не удался:

```cpp
auto toggle = query::useMutation<int>({
    .request = [](const int& id) { return FetchRequest{.method = "POST", .url = "/api/todos/" + std::to_string(id) + "/toggle"}; },
    .onMutate = [](const int& id) -> Cleanup {
        auto before = query::getData<std::vector<Todo>>("todos");
        query::updateData<std::vector<Todo>>("todos", [id](std::vector<Todo> list) {
            for (auto& t : list) if (t.id == id) t.done = !t.done;
            return list;
        });
        return [before] { if (before) query::setData("todos", *before); };  // откат
    },
    .invalidates = {"todos"},
});
```

## Кэш откуда угодно

```cpp
query::invalidate("todos");   // "todos" и "todos/..." загрузятся заново (те, что на экране, — сейчас, остальные — когда появятся)
query::setData<User>("me", user);                 // например, из ответа на вход
query::getData<User>("me");                       // std::shared_ptr<const User> или nullptr
query::updateData<User>("me", [](User u) { u.name = "Аня"; return u; });
query::remove("todos");       // забыть
query::clear();               // забыть всё (после выхода из аккаунта)
```

Всё работает в UI-потоке, как состояние компонентов. Библиотека не использует `std::format`, поэтому мало добавляет к `.wasm`.

## Лицензия

MIT или Apache-2.0, как ReactX.
