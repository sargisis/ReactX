# reactx::icons

Icons for [ReactX](https://github.com/sargisis/ReactX) apps: a line-drawn SVG in the browser that takes the text color and size, a one-cell symbol in a terminal. 51 icons drawn for ReactX (2 px lines on a 24×24 grid), and your own with `add()`.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/icons.hpp"

rx::icons::icon({.name = "search"})
rx::icons::icon({.name = "trash", .size = 20, .color = "red", .label = "Delete"})
rx::icons::icon({.name = "loader", .spin = true})
```

## Add it to a project

It comes with ReactX (since 1.4). Link it in the project's `CMakeLists.txt`:

```cmake
target_link_libraries(my_app PRIVATE reactx::icons)
```

## Icons

| | |
|---|---|
| actions | `check` `x` `plus` `minus` `search` `edit` `trash` `copy` `download` `upload` `send` `refresh` `filter` `settings` `more` `menu` `external` `link` `loader` |
| arrows | `arrow-left` `arrow-right` `arrow-up` `arrow-down` `chevron-left` `chevron-right` `chevron-up` `chevron-down` |
| status | `info` `warning` `error` `success` `bell` `lock` `unlock` `eye` `eye-off` `star` `heart` |
| things | `home` `user` `mail` `calendar` `clock` `file` `folder` `image` `sun` `moon` |
| media | `play` `pause` `stop` |

`rx::icons::names()` lists them all. An unknown name shows `?` (a dashed box in the browser), so a typo is easy to see.

| Field | |
|---|---|
| `name` | the icon |
| `size` | browser: px; `0` (default) = the font size, so an icon in a heading is as big as the heading |
| `label` | for screen readers when no text says the same next to it (`"Delete"`); without it the icon is hidden from them |
| `color` | `""` = the text color; a palette name (`red`, `green`, `blue`, `yellow`, `magenta`, `cyan`, `gray`) — the same shades as `rx::ui` in light and dark themes — or, in the browser, any CSS color |
| `spin` | browser: turns forever (`loader`, `refresh`); slower with "reduce motion" |
| `key` | for lists, the last field |

## Your own icons

```cpp
rx::icons::add("bolt", "M13 3L5 13h6l-1 8 8-10h-6z", "ϟ");   // SVG path on a 24×24 grid, terminal symbol
rx::icons::icon({.name = "bolt"})
```

The path is drawn as a 2 px line with round ends and no fill, like the built-in ones. Call `add` before the app shows the icon (e.g. in `main()`); the same name replaces a built-in icon.

## License

MIT or Apache-2.0, like ReactX. The icons are drawn for ReactX and come under the same license.
