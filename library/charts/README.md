# reactx::charts

Charts for [ReactX](https://github.com/sargisis/ReactX) apps. One call draws SVG in the browser and block and braille characters in a terminal: bars, lines and pies.

```cpp
#include "rx/charts.hpp"

rx::charts::bar({.labels = {"Mon", "Tue", "Wed"},
                 .series = {{.name = "Steps", .values = {6200, 8100, 4300}}},
                 .title = "This week"})
```

```
This week
Mon ██████████████████████████████████▋          6200
Tue █████████████████████████████████████████████ 8100
Wed ███████████████████████▉                      4300
```

In the browser the same call is an SVG that fills its box, follows the light and dark theme of `rx::ui` and shows a value when you hover a bar or a point.

## Add it to a project

It comes with ReactX (since 1.2). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_app PRIVATE reactx::charts)
```

Only linked libraries are compiled, and an unused one adds nothing to a `.wasm`.

## Charts

```cpp
using namespace rx;

// Bars: columns in the browser, rows of bars in a terminal. Several series stand side by side.
charts::bar({.labels = {"Q1", "Q2", "Q3"},
             .series = {{.name = "2025", .values = {12, 18, 9}}, {.name = "2026", .values = {15, 21, 14}}},
             .title = "Sales", .unit = "k"})

// Lines through the values (braille in a terminal).
charts::line({.labels = {"Jan", "Feb", "Mar", "Apr"},
              .series = {{.name = "Visits", .values = {10, 40, 25, 80}}},
              .unit = "k", .rows = 8})

// Shares of a whole: a pie or a ring in the browser, a split bar and a legend in a terminal.
charts::pie({.labels = {"Rent", "Food", "Fun"}, .values = {1200, 450, 200}, .unit = "$", .donut = true})
```

| Field | |
|---|---|
| `labels` | one per value, under the bars or the line |
| `series` | `{.name, .values, .color}`; `color` is a palette name (`blue`, `green`, `magenta`, `cyan`, `yellow`, `red`), in the browser any CSS color; empty: the next palette color |
| `title`, `unit` | a caption; text after every value (`"k"`, `" %"`) |
| `height` | browser: px (240) |
| `rows` (`line`) | terminal: rows (10) |
| `width` | terminal: cells (`bar`, `pie`: the width of their box; `line`: the window's width); the browser fills the box |
| `dots` (`line`) | a dot on every value in the browser |
| `donut` (`pie`) | a ring with the total in the middle |
| `key` | for lists, the last field |

`rx::charts::shortNumber(12345)` is `"12.3k"`: the text the charts use for numbers.

No `std::format` inside, so the library adds only its own code to a `.wasm` (std::format would add ~100 KB).

## Develop

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-16   # fetches ReactX v1.1.0
cmake --build build && ctest --test-dir build                # terminal tests (reactx/testing.hpp)
# against a local ReactX: -DFETCHCONTENT_SOURCE_DIR_REACTX=/path/to/ReactX
```

## License

MIT or Apache-2.0, like ReactX.
