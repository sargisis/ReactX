# reactx::grid

A data table for [ReactX](https://github.com/sargisis/ReactX) apps: your rows with the parts every data screen writes by hand — sorting by a header click the way people expect, a search box, your own filters, pages and selected rows. Drawn with `ui::table`: the same code in the browser and in a terminal.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/grid.hpp"
using namespace rx;

struct Incident { std::string id, title, severity; int age = 0; };

Element Incidents(const Props&) {
    auto [incidents, setIncidents] = useState([] { return loadIncidents(); });
    auto [onlyOpen, setOnlyOpen] = useState(false);
    auto grid = grid::useGrid<Incident>(incidents, {
        .columns = {
            {.title = "ID", .text = [](const Incident& i) { return i.id; }, .width = 10},
            {.title = "Title", .text = [](const Incident& i) { return i.title; }},
            {.title = "Severity", .text = [](const Incident& i) { return i.severity; },
             .order = {"Critical", "High", "Medium", "Low"}},
            {.title = "Age", .text = [](const Incident& i) { return std::to_string(i.age); },
             .width = 5, .align = ui::Align::End},
        },
        .id = [](const Incident& i) { return i.id; },
        .filter = [onlyOpen](const Incident& i) { return !onlyOpen || i.severity != "Low"; },
        .pageSize = 20,
        .select = true,
    });
    return grid.view();  // search box, table, "1–20 of 312" and the page buttons
}
```

## Add it to a project

It comes with ReactX (since 1.7). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_app PRIVATE reactx::grid)
```

## Columns

| Field | |
|---|---|
| `title` | the header |
| `text` | `(const T&) -> std::string`: what the cell shows; search and sorting use it too |
| `cell` | `(const T&) -> Element`: draw the cell yourself (a badge, a link); `text` still sorts and searches |
| `order` | ranks values for sorting: `{"Critical", "High", "Medium", "Low"}` (values not in it go last) |
| `compare` | `(a, b) -> -1 / 0 / 1`: your own sorting (dates, money) |
| `width`, `align` | characters (0 = share the room left); `ui::Align::End` for numbers |
| `sortable`, `searchable` | `true` by default |

Without `order` or `compare` a column sorts the way people read: case doesn't matter and numbers inside text compare as numbers (`INC-2` before `INC-10`). Sorting is stable: equal rows keep your order. A click on the same header flips the direction; another header starts ascending.

## Options

| Field | |
|---|---|
| `id` | a row's stable id; keeps selected rows across sorting and pages (needed for `select`) |
| `filter` | your own filter, together with the search |
| `pageSize` | rows per page; `0` = all |
| `search` | show the search box in `view()` (`true`) |
| `select` | a checkbox per row; the ids are in `grid.selected` |
| `onRowClick` | a click or Enter on a row (otherwise a click selects it when `select` is on) |
| `sortColumn`, `sortDescending` | the first sort |
| `searchPlaceholder`, `emptyText` | texts |

## What `useGrid` gives back

`grid.rows` — the rows on this page, filtered and sorted (pointers into your vector); `matching`, `total`, `page`, `pages`, `search`, `sortColumn`, `sortDescending`, `selected`; and `setSearch`, `sortBy(column)`, `setPage`, `toggle(id)`, `setSelected(ids)`. A new search or sort goes back to the first page.

Draw it all with `grid.view()`, or the parts: `grid.searchBox()`, `grid.table()`, `grid.pager()`.

`rx::grid::compareNatural(a, b)` and `rx::grid::containsIgnoringCase(text, part)` are there for your own code too.

Every render filters and sorts the rows again; for 100,000 rows put the heavy part on a server (`useQuery`) or show one page from it.
