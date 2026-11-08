# reactx::grid

Таблица данных для приложений на [ReactX](https://github.com/sargisis/ReactX): ваши строки и то, что каждый экран с данными пишет руками, — сортировка по клику на заголовок так, как ждут люди, поиск, свои фильтры, страницы и выбранные строки. Рисуется через `ui::table`: один код в браузере и в терминале.

In English: [README.md](README.md).

```cpp
#include "rx/grid.hpp"
using namespace rx;

struct Incident { std::string id, title, severity; int age = 0; };

Element Incidents(const Props&) {
    auto [incidents, setIncidents] = useState([] { return loadIncidents(); });
    auto grid = grid::useGrid<Incident>(incidents, {
        .columns = {
            {.title = "ID", .text = [](const Incident& i) { return i.id; }, .width = 10},
            {.title = "Название", .text = [](const Incident& i) { return i.title; }},
            {.title = "Важность", .text = [](const Incident& i) { return i.severity; },
             .order = {"Critical", "High", "Medium", "Low"}},
            {.title = "Возраст", .text = [](const Incident& i) { return std::to_string(i.age); },
             .width = 7, .align = ui::Align::End},
        },
        .id = [](const Incident& i) { return i.id; },
        .pageSize = 20,
        .select = true,
        .searchPlaceholder = "Поиск...",
        .emptyText = "Ничего не найдено",
    });
    return grid.view();  // поиск, таблица, «1–20 of 312» и кнопки страниц
}
```

## Подключение

Идёт вместе с ReactX (с 1.7). В `CMakeLists.txt` проекта:

```cmake
target_link_libraries(my_app PRIVATE reactx::grid)
```

## Колонки

| Поле | |
|---|---|
| `title` | заголовок |
| `text` | `(const T&) -> std::string`: что показывает ячейка; по нему же поиск и сортировка |
| `cell` | `(const T&) -> Element`: своя ячейка (значок, ссылка); сортирует и ищет по-прежнему `text` |
| `order` | порядок значений при сортировке: `{"Critical", "High", "Medium", "Low"}` (остальные — в конце) |
| `compare` | `(a, b) -> -1 / 0 / 1`: своя сортировка (даты, деньги) |
| `width`, `align` | символы (0 — делить оставшееся место); `ui::Align::End` для чисел |
| `sortable`, `searchable` | по умолчанию `true` |

Без `order` и `compare` колонка сортируется так, как читают люди: регистр не важен, числа внутри текста сравниваются как числа (`INC-2` раньше `INC-10`). Сортировка устойчивая: равные строки остаются в вашем порядке. Повторный клик по тому же заголовку меняет направление, клик по другому начинает по возрастанию.

## Опции

| Поле | |
|---|---|
| `id` | постоянный id строки; сохраняет выбор при сортировке и листании (нужен для `select`) |
| `filter` | свой фильтр, вместе с поиском |
| `pageSize` | строк на странице; `0` — все |
| `search` | поле поиска в `view()` (`true`) |
| `select` | флажок у каждой строки; id выбранных — в `grid.selected` |
| `onRowClick` | клик или Enter по строке (иначе при `select` клик выбирает её) |
| `sortColumn`, `sortDescending` | начальная сортировка |
| `searchPlaceholder`, `emptyText` | тексты |

## Что возвращает `useGrid`

`grid.rows` — строки этой страницы, отфильтрованные и отсортированные (указатели в ваш вектор); `matching`, `total`, `page`, `pages`, `search`, `sortColumn`, `sortDescending`, `selected`; и `setSearch`, `sortBy(column)`, `setPage`, `toggle(id)`, `setSelected(ids)`. Новый поиск или сортировка возвращают на первую страницу.

Всё сразу — `grid.view()`, или по частям: `grid.searchBox()`, `grid.table()`, `grid.pager()`.

`rx::grid::compareNatural(a, b)` и `rx::grid::containsIgnoringCase(text, part)` можно звать и из своего кода.

Каждая отрисовка заново фильтрует и сортирует строки; для 100 000 строк тяжёлую часть делайте на сервере (`useQuery`) и показывайте страницу оттуда.
