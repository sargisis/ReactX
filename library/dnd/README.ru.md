# reactx::dnd

Перетаскивание для изменения порядка списка в приложениях на [ReactX](https://github.com/sargisis/ReactX): мышью в браузере и в терминале, клавиатурой везде (фокус на элементе, Alt+↑ / Alt+↓). Порядок храните вы; список сообщает новый.

In English: [README.md](README.md).

```cpp
#include "rx/dnd.hpp"
using namespace rx;

auto [order, setOrder] = useState(std::vector<std::string>{"design", "build", "ship"});
dnd::sortable({
    .ids = order,
    .item = [&](const std::string& id, bool lifted) { return ui::card({.title = titles.at(id)}); },
    .onChange = [setOrder](std::vector<std::string> next) { setOrder(std::move(next)); },
    .label = "Задачи, перетащите, чтобы поменять порядок",
})
```

## Подключение

Идёт вместе с ReactX (с версии 1.5): `target_link_libraries(my_app PRIVATE reactx::dnd)`.

## API

| `SortableProps` | |
|---|---|
| `ids` | элементы по порядку, по id |
| `item(id, lifted)` | как выглядит элемент; `lifted` — пока его тащат |
| `onChange(ids)` | новый порядок после броска или Alt+↑ / Alt+↓ |
| `label` | для экранных дикторов |
| `gap` | между элементами (строки в терминале; по 4 px в браузере) |

Браузер показывает поднятый элемент бледным и линию, куда он встанет; терминал помечает его `»`, а место — `↑` / `↓`. Отпускание вне списка или уход с него отменяет. `dnd::moved(ids, from, to)` переставляет по индексу (для своих кнопок). Сенсорные экраны: пока — клавиатура или кнопки.

## Лицензия

MIT или Apache-2.0, как ReactX.
