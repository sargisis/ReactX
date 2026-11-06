# reactx::icons

Иконки для приложений на [ReactX](https://github.com/sargisis/ReactX): линейный SVG в браузере, который берёт цвет и размер текста, и символ в одну клетку в терминале. 51 иконка, нарисованная для ReactX (линии 2 px на сетке 24×24), и свои через `add()`.

In English: [README.md](README.md).

```cpp
#include "rx/icons.hpp"

rx::icons::icon({.name = "search"})
rx::icons::icon({.name = "trash", .size = 20, .color = "red", .label = "Удалить"})
rx::icons::icon({.name = "loader", .spin = true})
```

## Подключение

Библиотека идёт вместе с ReactX (с версии 1.4). Подключите её в `CMakeLists.txt` проекта:

```cmake
target_link_libraries(my_app PRIVATE reactx::icons)
```

## Иконки

| | |
|---|---|
| действия | `check` `x` `plus` `minus` `search` `edit` `trash` `copy` `download` `upload` `send` `refresh` `filter` `settings` `more` `menu` `external` `link` `loader` |
| стрелки | `arrow-left` `arrow-right` `arrow-up` `arrow-down` `chevron-left` `chevron-right` `chevron-up` `chevron-down` |
| статусы | `info` `warning` `error` `success` `bell` `lock` `unlock` `eye` `eye-off` `star` `heart` |
| предметы | `home` `user` `mail` `calendar` `clock` `file` `folder` `image` `sun` `moon` |
| медиа | `play` `pause` `stop` |

`rx::icons::names()` возвращает все имена. Неизвестное имя показывает `?` (в браузере — пунктирный квадрат), так что опечатку легко заметить.

| Поле | |
|---|---|
| `name` | иконка |
| `size` | браузер: px; `0` (по умолчанию) — размер шрифта, так что иконка в заголовке размером с заголовок |
| `label` | для экранных дикторов, когда рядом нет текста с тем же смыслом (`"Удалить"`); без него иконка от них скрыта |
| `color` | `""` — цвет текста; имя из палитры (`red`, `green`, `blue`, `yellow`, `magenta`, `cyan`, `gray`) — те же оттенки, что у `rx::ui` в светлой и тёмной теме — или, в браузере, любой CSS-цвет |
| `spin` | браузер: вращается (`loader`, `refresh`); медленнее при «уменьшении движения» |
| `key` | для списков, последнее поле |

## Свои иконки

```cpp
rx::icons::add("bolt", "M13 3L5 13h6l-1 8 8-10h-6z", "ϟ");   // SVG-путь на сетке 24×24, символ для терминала
rx::icons::icon({.name = "bolt"})
```

Путь рисуется линией 2 px с круглыми концами и без заливки, как встроенные. Вызывайте `add` до того, как приложение покажет иконку (например, в `main()`); то же имя заменяет встроенную иконку.

## Лицензия

MIT или Apache-2.0, как ReactX. Иконки нарисованы для ReactX и идут под той же лицензией.
