// Typed helpers of rx::ui (ui::button({.label = ...}) etc.). Shared by the
// terminal (ui_terminal.cpp) and browser (ui_web.cpp) implementations: they
// only turn the structs into props for the components.
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "reactx/hooks.hpp"
#include "reactx/parse.hpp"
#include "reactx/ui.hpp"

namespace rx::ui {

namespace {

const char* variantName(Variant v) {
    switch (v) {
        case Variant::Primary: return "primary";
        case Variant::Danger: return "danger";
        case Variant::Success: return "success";
        default: return "default";
    }
}

Element withKey(Element e, const std::string& key) {
    e.key = key;
    return e;
}

const char* alignName(Align a) {
    switch (a) {
        case Align::Start: return "start";
        case Align::Center: return "center";
        case Align::End: return "end";
        default: return "stretch";
    }
}

const char* justifyName(Justify j) {
    switch (j) {
        case Justify::Center: return "center";
        case Justify::End: return "end";
        case Justify::Between: return "between";
        default: return "start";
    }
}

}  // namespace

Element button(ButtonProps p) {
    return withKey(h(Button, {{"label", std::move(p.label)},
                              {"onClick", std::move(p.onClick)},
                              {"variant", variantName(p.variant)},
                              {"disabled", p.disabled}}),
                   p.key);
}

Element input(InputProps p) {
    return withKey(h(Input, {{"value", std::move(p.value)},
                             {"onChange", std::move(p.onChange)},
                             {"onSubmit", std::move(p.onSubmit)},
                             {"placeholder", std::move(p.placeholder)},
                             {"label", std::move(p.label)},
                             {"labelAbove", p.labelAbove},
                             {"disabled", p.disabled},
                             {"error", std::move(p.error)}}),
                   p.key);
}

Element checkbox(CheckboxProps p) {
    Callback toggle = std::move(p.onToggle);
    if (p.onChange)
        toggle = [toggle = std::move(toggle), onChange = std::move(p.onChange), next = !p.checked] {
            if (toggle) toggle();
            onChange(next);
        };
    return withKey(h(Checkbox, {{"label", std::move(p.label)}, {"checked", p.checked}, {"onToggle", std::move(toggle)},
                                {"disabled", p.disabled}, {"error", std::move(p.error)}}),
                   p.key);
}

Element tabs(TabsProps p) {
    std::vector<Element> children;
    for (auto& tab : p.tabs) children.push_back(h("ui-tab", {{"label", tab.label}}, std::move(tab.content)));
    return withKey(h(Tabs, {{"initial", p.initial}}, std::move(children)), p.key);
}

Element modal(ModalProps p) {
    return withKey(h(Modal, {{"title", std::move(p.title)},
                             {"open", p.open},
                             {"onClose", std::move(p.onClose)},
                             {"width", p.width}},
                     std::move(p.children)),
                   p.key);
}

Element listItem(ListItemProps p) {
    return withKey(h(ListItem, {{"label", std::move(p.label)},
                                {"detail", std::move(p.detail)},
                                {"selected", p.selected},
                                {"onSelect", std::move(p.onSelect)},
                                {"onKey", std::move(p.onKey)}}),
                   p.key);
}

Element list(ListProps p) {
    // .items is the old name of .children: both work, the items come first.
    std::vector<Element> items = std::move(p.items);
    for (auto& e : p.children) items.push_back(std::move(e));
    return withKey(h(List, {{"title", std::move(p.title)}, {"emptyText", std::move(p.emptyText)}, {"scroll", p.scroll}},
                     std::move(items)),
                   p.key);
}

Element text(TextProps p) {
    return withKey(h(Text, {{"text", std::move(p.text)}, {"muted", p.muted}}), p.key);
}

Element card(CardProps p) {
    return withKey(h(Card, {{"title", std::move(p.title)}, {"level", std::clamp(p.level, 1, 6)}}, std::move(p.children)), p.key);
}

Element stack(StackProps p) {
    return withKey(h(Stack, {{"direction", p.direction == Direction::Row ? "row" : "column"},
                             {"gap", p.gap},
                             {"align", alignName(p.align)},
                             {"justify", justifyName(p.justify)},
                             {"wrap", p.wrap}},
                     std::move(p.children)),
                   p.key);
}

namespace {
std::vector<Element> optionElements(std::vector<Option>& list) {
    std::vector<Element> options;
    for (auto& o : list) {
        std::string label = o.label.empty() ? o.value : o.label;
        options.push_back(h("ui-option", {{"key", o.value}, {"value", std::move(o.value)}, {"label", std::move(label)}}));
    }
    return options;
}
}  // namespace

Element select(SelectProps p) {
    auto options = optionElements(p.options);
    return withKey(h(Select, {{"label", std::move(p.label)}, {"value", std::move(p.value)}, {"onChange", std::move(p.onChange)},
                              {"labelAbove", p.labelAbove}, {"disabled", p.disabled}, {"error", std::move(p.error)}},
                     std::move(options)),
                   p.key);
}

Element textarea(TextareaProps p) {
    return withKey(h(Textarea, {{"value", std::move(p.value)},
                                {"onChange", std::move(p.onChange)},
                                {"placeholder", std::move(p.placeholder)},
                                {"label", std::move(p.label)},
                                {"rows", std::max(p.rows, 1)},
                                {"disabled", p.disabled},
                                {"error", std::move(p.error)}}),
                   p.key);
}

Element radioGroup(RadioGroupProps p) {
    auto options = optionElements(p.options);
    return withKey(h(RadioGroup, {{"label", std::move(p.label)}, {"value", std::move(p.value)}, {"onChange", std::move(p.onChange)},
                                  {"row", p.row || p.direction == Direction::Row}, {"disabled", p.disabled},
                                  {"error", std::move(p.error)}},
                     std::move(options)),
                   p.key);
}

Element slider(SliderProps p) {
    int lo = std::min(p.min, p.max), hi = std::max(p.min, p.max);
    // The props of a component hold std::function<void(int)> as an index handler.
    std::function<void(int)> onChange = std::move(p.onChange);
    return withKey(h(Slider, {{"label", std::move(p.label)},
                              {"value", std::clamp(p.value, lo, hi)},
                              {"min", lo},
                              {"max", hi},
                              {"step", std::max(p.step, 1)},
                              {"onChange", InputHandler([onChange](const std::string& text) {
                                   if (auto n = parseNumber<int>(text); n && onChange) onChange(*n);
                               })},
                              {"showValue", p.showValue},
                              {"disabled", p.disabled}}),
                   p.key);
}

Element progress(ProgressProps p) {
    int max = std::max(p.max, 1);
    return withKey(h(Progress, {{"value", std::clamp(p.value, 0, max)}, {"max", max}, {"label", std::move(p.label)},
                                {"variant", variantName(p.variant)}}),
                   p.key);
}

Element tooltip(TooltipProps p) {
    return withKey(h(Tooltip, {{"text", std::move(p.text)}}, std::move(p.children)), p.key);
}

Element confirm(ConfirmProps p) {
    return withKey(h(Confirm, {{"title", std::move(p.title)},
                               {"message", std::move(p.message)},
                               {"open", p.open},
                               {"confirmLabel", std::move(p.confirmLabel)},
                               {"cancelLabel", std::move(p.cancelLabel)},
                               {"danger", p.danger},
                               {"onConfirm", std::move(p.onConfirm)},
                               {"onCancel", std::move(p.onCancel)}}),
                   p.key);
}

// The same in both backends: a Modal holding a Text and a row of Buttons.
Element Confirm(const Props& p) {
    auto onCancel = p.get<Callback>("onCancel");
    std::string message = p.get<std::string>("message");
    std::vector<Element> body;
    if (!message.empty()) body.push_back(text({.text = message}));
    body.push_back(stack({
        .direction = Direction::Row,
        .justify = Justify::End,
        .children = {
            button({.label = p.get<std::string>("cancelLabel", "Cancel"), .onClick = onCancel, .key = "cancel"}),
            button({.label = p.get<std::string>("confirmLabel", "OK"),
                    .onClick = p.get<Callback>("onConfirm"),
                    .variant = p.get<bool>("danger") ? Variant::Danger : Variant::Primary,
                    .key = "confirm"}),
        },
    }));
    return modal({.title = p.get<std::string>("title"), .open = p.get<bool>("open"), .onClose = onCancel,
                  .children = std::move(body)});
}

// ---- toasts ----

namespace detail {

const Store<std::vector<ToastItem>>& toasts() {
    static const auto store = createStore(std::vector<ToastItem>{});
    return store;
}

void dismissToast(int id) {
    toasts().update([id](std::vector<ToastItem> list) {
        std::erase_if(list, [id](const ToastItem& t) { return t.id == id; });
        return list;
    });
}

}  // namespace detail

void toast(ToastProps p) {
    static int nextId = 0;
    detail::ToastItem item{.id = ++nextId, .text = std::move(p.text), .variant = variantName(p.variant), .duration = p.duration};
    detail::toasts().update([&item](std::vector<detail::ToastItem> list) {
        list.push_back(std::move(item));
        if (list.size() > 5) list.erase(list.begin());  // the oldest makes room
        return list;
    });
}

Element toaster() { return h(Toaster); }

Element table(TableProps p) { return h(detail::TableView, std::move(p)); }

// ---- virtualList ----

namespace {
#ifdef __EMSCRIPTEN__
constexpr int kGuess = 800, kOverscan = 8;  // px, rows
int unitOf(const VirtualListOptions& o) { return std::max(o.rowHeight, 1); }
#else
constexpr int kGuess = 60, kOverscan = 4;  // terminal rows (plus a screen: PageUp/PageDown land on drawn rows)
int unitOf(const VirtualListOptions& o) { return std::max(o.rowLines, 1); }
#endif
}  // namespace

VirtualList useVirtualList(VirtualListOptions o) {
    const int unit = unitOf(o);
    auto box = useNodeRef();
    auto [viewport, setViewport] = useState(std::pair<int, int>{0, o.height > 0 ? o.height : kGuess});
    const int count = std::max(o.count, 0);
    const int total = count * unit;

    // The real size and position, once laid out (and after every change: a
    // resized window, rows added while following).
    useEffect([box, setViewport, follow = o.follow, total] {
        if (follow) box.scrollTo(total);
        if (auto m = box.scrollMetrics()) setViewport(std::pair{m->top, std::max(m->clientHeight, 1)});
    });

    const int height = std::max(viewport.second, 1);
    const int top = o.follow ? std::max(total - height, 0) : std::clamp(viewport.first, 0, std::max(total - height, 0));
    VirtualList list;
#ifdef __EMSCRIPTEN__
    const int overscan = kOverscan;
#else
    const int overscan = kOverscan + height / unit;
#endif
    list.first = std::clamp(top / unit - overscan, 0, count);
    list.last = std::clamp((top + height) / unit + 1 + overscan, list.first, count);
    list.options = std::move(o);
    list.box = box;
    list.setViewport = setViewport;
    return list;
}

Element VirtualList::view(std::vector<Element> rows) const {
    const int unit = unitOf(options);
    const int count = std::max(options.count, 0);
    const int drawnEnd = std::min(first + static_cast<int>(rows.size()), count);
    auto onScroll = [setViewport = setViewport, user = options.onScroll](const ScrollEvent& e) {
        setViewport(std::pair{e.top, std::max(e.clientHeight, 1)});
        if (user) user(e);
    };
    std::vector<Element> parts;
    parts.reserve(rows.size() + 2);
#ifdef __EMSCRIPTEN__
    auto spacer = [](const char* key, int px) {
        return h("div", {{"key", key}, {"aria-hidden", "true"}, {"style", "height: " + std::to_string(px) + "px"}});
    };
    parts.push_back(spacer("before", first * unit));
    for (int i = first; i < drawnEnd; ++i)
        parts.push_back(h("div", {{"key", std::to_string(i)}, {"className", "rx-vlist__row"},
                                  {"style", "height: " + std::to_string(unit) + "px"}},
                          {std::move(rows[static_cast<std::size_t>(i - first)])}));
    parts.push_back(spacer("after", (count - drawnEnd) * unit));
    std::string style = options.height > 0 ? "height: " + std::to_string(options.height) + "px"
                                           : std::string("flex: 1 1 0; min-height: 4em; max-height: 100vh");
    return h("div", {{"ref", box}, {"className", options.border ? "rx-vlist rx-vlist--border" : "rx-vlist"},
                     {"style", style}, {"onScroll", onScroll}},
             std::move(parts));
#else
    auto spacer = [](const char* key, int lines) { return h("box", {{"key", key}, {"height", lines}}); };
    parts.push_back(spacer("before", first * unit));
    for (int i = first; i < drawnEnd; ++i)
        parts.push_back(h("box", {{"key", std::to_string(i)}, {"height", unit}, {"overflow", "hidden"}},
                          {std::move(rows[static_cast<std::size_t>(i - first)])}));
    parts.push_back(spacer("after", (count - drawnEnd) * unit));
    return h("box", {{"ref", box}, {"overflow", "scroll"}, {"border", options.border}, {"followEnd", options.follow},
                     {"onScroll", onScroll}},
             std::move(parts));
#endif
}

}  // namespace rx::ui
