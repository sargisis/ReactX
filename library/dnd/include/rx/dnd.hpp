// reactx::dnd — drag and drop to reorder a list, with the mouse in the
// browser and in a terminal, and with the keyboard everywhere (focus an item,
// Alt+Up / Alt+Down). You keep the order; the list tells you the new one.
//
//   auto [order, setOrder] = useState(std::vector<std::string>{"a", "b", "c"});
//   rx::dnd::sortable({
//       .ids = order,
//       .item = [&](const std::string& id, bool lifted) { return ui::text({.text = titles.at(id)}); },
//       .onChange = [setOrder](std::vector<std::string> next) { setOrder(std::move(next)); },
//   })
//
// Comes with ReactX (library/dnd); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::dnd)
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "reactx/reactx.hpp"

namespace rx::dnd {

struct SortableProps {
    std::vector<std::string> ids{};  // the items, in order (each id once)
    // How an item looks; `lifted` while it is being dragged.
    std::function<Element(const std::string& id, bool lifted)> item{};
    // The new order after a drop or Alt+Up / Alt+Down.
    std::function<void(std::vector<std::string> ids)> onChange{};
    std::string label{};  // for screen readers: "Tasks, drag to reorder"
    int gap = 0;          // between items (terminal: rows; browser: 4 px each)
    std::string key{};
};
Element sortable(SortableProps props);

// `ids` with the item at `from` moved to `to` (indexes; out of range: unchanged).
std::vector<std::string> moved(std::vector<std::string> ids, std::size_t from, std::size_t to);

}  // namespace rx::dnd
