# reactx::dnd

Drag and drop to reorder a list in [ReactX](https://github.com/sargisis/ReactX) apps: with the mouse in the browser and in a terminal, and with the keyboard everywhere (focus an item, Alt+Up / Alt+Down). You keep the order; the list tells you the new one.

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/dnd.hpp"
using namespace rx;

auto [order, setOrder] = useState(std::vector<std::string>{"design", "build", "ship"});
dnd::sortable({
    .ids = order,
    .item = [&](const std::string& id, bool lifted) { return ui::card({.title = titles.at(id)}); },
    .onChange = [setOrder](std::vector<std::string> next) { setOrder(std::move(next)); },
    .label = "Tasks, drag to reorder",
})
```

## Add it to a project

It comes with ReactX (since 1.5): `target_link_libraries(my_app PRIVATE reactx::dnd)`.

## API

| `SortableProps` | |
|---|---|
| `ids` | the items in order, by id |
| `item(id, lifted)` | how an item looks; `lifted` while it is dragged |
| `onChange(ids)` | the new order after a drop or Alt+Up / Alt+Down |
| `label` | for screen readers |
| `gap` | between items (terminal rows; 4 px each in the browser) |

The browser shows the lifted item faded and a line where it would land; a terminal marks it `»` and the place `↑` / `↓`. Releasing outside the list or leaving it cancels. `dnd::moved(ids, from, to)` moves an item by index (for your own buttons). Touch screens: use the keyboard path or buttons for now.

## License

MIT or Apache-2.0, like ReactX.
