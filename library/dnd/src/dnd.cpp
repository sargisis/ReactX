// reactx::dnd: the same pointer events in both backends (as in the kanban
// example): an item's onPointerDown lifts it, onMouseEnter marks where it would
// land, the list's onPointerUp drops it; leaving the list cancels.
#include "rx/dnd.hpp"

#include <algorithm>
#include <string>
#include <utility>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "reactx/dom.hpp"
#endif

namespace rx::dnd {

namespace {

#ifdef __EMSCRIPTEN__
constexpr const char* kBox = "div";
constexpr const char* kCss = R"css(
.rx-dnd { display: flex; flex-direction: column; }
.rx-dnd-item { cursor: grab; user-select: none; border-radius: 8px; outline-offset: 2px; position: relative; }
.rx-dnd--dragging, .rx-dnd--dragging .rx-dnd-item { cursor: grabbing; }
.rx-dnd-item--lifted { opacity: .45; }
.rx-dnd-item--over::before { content: ""; position: absolute; left: 0; right: 0; top: -3px; height: 2px;
  border-radius: 2px; background: var(--rx-accent, #2563eb); }
.rx-dnd-item--over-after::before { top: auto; bottom: -3px; }
)css";

// clang-format off  (JavaScript: a formatter would split === into == =)
EM_JS(int, rx_dnd_install_styles, (const char* css), {
    if (typeof document === "undefined") return 0;
    if (document.getElementById("rx-dnd-styles")) return 1;
    const style = document.createElement("style");
    style.id = "rx-dnd-styles";
    style.textContent = UTF8ToString(css);
    document.head.prepend(style);
    return 1;
});
// clang-format on

void ensureStyles() {
    static bool done = false;
    if (done) return;
    done = true;
    if (!rx_dnd_install_styles(kCss)) rx::detail::addPrerenderHead(std::string("<style id=\"rx-dnd-styles\">") + kCss + "</style>");
}
#else
constexpr const char* kBox = "box";
#endif

Element Sortable(const SortableProps& p) {
    auto [dragging, setDragging] = useState(std::string());  // the lifted id, "" = none
    auto [over, setOver] = useState(-1);                     // the index under the pointer
#ifdef __EMSCRIPTEN__
    ensureStyles();
#endif
    const int from = [&] {
        auto it = std::ranges::find(p.ids, dragging);
        return it == p.ids.end() ? -1 : static_cast<int>(it - p.ids.begin());
    }();
    auto onChange = p.onChange;
    auto ids = p.ids;
    auto drop = [ids, from, over, onChange, setDragging, setOver] {
        if (from >= 0 && over >= 0 && over != from && onChange)
            onChange(moved(ids, static_cast<std::size_t>(from), static_cast<std::size_t>(over)));
        setDragging(std::string());
        setOver(-1);
    };

    std::vector<Element> items;
    for (std::size_t i = 0; i < p.ids.size(); ++i) {
        const std::string& id = p.ids[i];
        const int index = static_cast<int>(i);
        const bool lifted = id == dragging;
        const bool target = from >= 0 && index == over && over != from;
        Element content = p.item ? p.item(id, lifted) : Element(id);
        Element e = h(kBox, {
                {"onPointerDown", [setDragging, setOver, id, index](const PointerEvent& ev) {
                     if (ev.button != 0) return;
                     setDragging(id);
                     setOver(index);
                 }},
                {"onMouseEnter", [setOver, index] { setOver(index); }},
                {"onKey", [ids, index, onChange](const KeyEvent& k) {
                     if (!k.alt || !onChange) return;
                     if (k.key == "Up" && index > 0) onChange(moved(ids, static_cast<std::size_t>(index), static_cast<std::size_t>(index - 1)));
                     if (k.key == "Down" && index + 1 < static_cast<int>(ids.size()))
                         onChange(moved(ids, static_cast<std::size_t>(index), static_cast<std::size_t>(index + 1)));
                 }},
                {"focusable", true},
                {"tabindex", 0},
            },
            {});
#ifdef __EMSCRIPTEN__
        std::string cls = "rx-dnd-item";
        if (lifted) cls += " rx-dnd-item--lifted";
        if (target) cls += over > from ? " rx-dnd-item--over rx-dnd-item--over-after" : " rx-dnd-item--over";
        e.props.set("className", cls);
        e.props.set("aria-roledescription", std::string("sortable item"));
        e.props.setChildren({std::move(content)});
#else
        // A terminal marks the lifted item and where it would go in the first column.
        const char* mark = lifted ? "» " : target ? (over > from ? "↓ " : "↑ ") : "  ";
        e.props.set("direction", std::string("row"));
        if (lifted) e.props.set("color", std::string("cyan"));
        if (target) e.props.set("color", std::string("yellow"));
        e.props.setChildren({h("text", {{"wrap", false}}, {mark}), h("box", {{"flexGrow", 1}, {"flexShrink", 1}}, {std::move(content)})});
#endif
        e.key = id;
        items.push_back(std::move(e));
    }

    Element list = h(kBox, {
            {"onPointerUp", [drop](const PointerEvent&) { drop(); }},
            {"onMouseLeave", [setDragging, setOver] {
                 setDragging(std::string());
                 setOver(-1);
             }},
        },
        std::move(items));
#ifdef __EMSCRIPTEN__
    list.props.set("className", std::string(dragging.empty() ? "rx-dnd" : "rx-dnd rx-dnd--dragging"));
    list.props.set("role", std::string("list"));
    if (!p.label.empty()) list.props.set("aria-label", p.label);
    if (p.gap > 0) list.props.set("style", "gap:" + std::to_string(4 * p.gap) + "px");
#else
    if (p.gap > 0) list.props.set("gap", static_cast<long>(p.gap));
#endif
    return list;
}

}  // namespace

std::vector<std::string> moved(std::vector<std::string> ids, std::size_t from, std::size_t to) {
    if (from >= ids.size() || to >= ids.size() || from == to) return ids;
    std::string item = std::move(ids[from]);
    ids.erase(ids.begin() + static_cast<std::ptrdiff_t>(from));
    ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(to), std::move(item));
    return ids;
}

Element sortable(SortableProps props) {
    std::string key = std::move(props.key);
    Element e = h(Sortable, std::move(props));
    if (!key.empty()) e.key = std::move(key);
    return e;
}

}  // namespace rx::dnd
