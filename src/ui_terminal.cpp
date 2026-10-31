#include "reactx/ui.hpp"

#include <algorithm>
#include <utility>

#include "reactx/hooks.hpp"
#include "reactx/store.hpp"

// rx::ui for the terminal backend (box/text/button/input tags, Yoga layout).
// The typed helpers (ui::button(...)) are in ui_helpers.cpp.
namespace rx::ui {

Element themeProvider(Theme theme, std::vector<Element> children) {
    return ThemeContext.provide(std::move(theme), std::move(children));
}

const Theme& useTheme() { return useContext(ThemeContext); }

// ---- components ----

Element Button(const Props& p) {
    const Theme& t = useTheme();
    std::string label = p.get<std::string>("label");
    std::string variant = p.get<std::string>("variant", "default");
    if (p.get<bool>("disabled"))
        return h("text", {{"color", t.muted}, {"dim", true}, {"wrap", false}, {"flexShrink", 0}},
                 {"[ " + label + " ]"});
    std::string color = variant == "primary"   ? t.accent
                        : variant == "danger"  ? t.danger
                        : variant == "success" ? t.success
                                               : t.text;
    return h("button", {{"color", color}, {"bold", variant != "default"}, {"onClick", p.get<Callback>("onClick")}},
             {label});
}

Element Input(const Props& p) {
    const Theme& t = useTheme();
    std::string value = p.get<std::string>("value");
    auto onChange = p.get<InputHandler>("onChange");
    auto onSubmit = p.get<InputHandler>("onSubmit");
    std::string label = p.get<std::string>("label");

    Element field = h("input", {
        {"flexGrow", 1},
        {"value", value},
        {"placeholder", p.get<std::string>("placeholder")},
        {"color", t.text},
        {"onInput", onChange},
        {"onKey", [onChange, onSubmit, value](const KeyEvent& e) {
             if (e.key == "Enter" && onSubmit) onSubmit(value);
             if (e.key == "Escape" && onChange && !value.empty()) onChange(std::string{});
         }},
    });
    if (label.empty()) return h("box", {{"direction", "row"}}, {std::move(field)});
    return h("box", {{"direction", "row"}, {"gap", 1}}, {
        h("text", {{"color", t.muted}, {"wrap", false}}, {label}),
        std::move(field),
    });
}

Element Checkbox(const Props& p) {
    const Theme& t = useTheme();
    bool checked = p.get<bool>("checked");
    return h("box", {{"direction", "row"}, {"gap", 1}, {"focusable", true}, {"onClick", p.get<Callback>("onToggle")}}, {
        h("text", {{"color", checked ? t.accent : t.muted}, {"wrap", false}}, {checked ? "[x]" : "[ ]"}),
        h("text", {{"color", t.text}}, {p.get<std::string>("label")}),
    });
}

Element Tabs(const Props& p) {
    const Theme& t = useTheme();
    const auto& tabs = p.children();
    auto [active, setActive] = useState(p.get<int>("initial", 0));
    if (tabs.empty()) return fragment({});
    int current = std::clamp(active, 0, static_cast<int>(tabs.size()) - 1);

    std::vector<Element> header;
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        int index = static_cast<int>(i);
        bool on = index == current;
        std::string label = tabs[i].props.get<std::string>("label");
        header.push_back(h("box", {{"key", label},
                                   {"focusable", true},
                                   {"paddingX", 1},
                                   {"onClick", [setActive, index] { setActive(index); }}},
                           {h("text", {{"color", on ? t.accent : t.muted}, {"bold", on}, {"underline", on}, {"wrap", false}},
                              {label})}));
    }
    const Element& tab = tabs[static_cast<std::size_t>(current)];
    return h("box", {{"gap", 1}}, {
        h("box", {{"direction", "row"}, {"gap", 1}}, std::move(header)),
        // Keyed by label: switching tabs mounts fresh content.
        fragment(tab.props.children(), tab.props.get<std::string>("label")),
    });
}

Element Modal(const Props& p) {
    if (!p.get<bool>("open")) return fragment({});
    const Theme& t = useTheme();
    auto onClose = p.get<Callback>("onClose");
    int width = p.get<int>("width");
    return h("box", {
        // fixed: the screen, even when the page under it is taller than the terminal
        {"position", "fixed"}, {"top", 0}, {"left", 0}, {"right", 0}, {"bottom", 0},
        {"justifyContent", "center"}, {"alignItems", "center"},
        {"trapFocus", true},
        {"onKey", [onClose](const KeyEvent& e) {
             if (e.key == "Escape" && onClose) onClose();
         }},
    }, {
        h("box", {
            {"border", true}, {"opaque", true},
            {"title", p.get<std::string>("title")},
            {"color", t.accent}, {"bg", t.surface},
            {"paddingX", 2}, {"paddingY", 1}, {"gap", 1},
            {"width", width > 0 ? PropValue(std::int64_t{width}) : PropValue(std::string("60%"))},
            {"maxWidth", "100%"},
        }, p.children()),
    });
}

Element List(const Props& p) {
    const Theme& t = useTheme();
    std::vector<Element> items = p.children();
    if (items.empty()) items.push_back(h("text", {{"color", t.muted}, {"dim", true}}, {p.get<std::string>("emptyText")}));
    std::string title = p.get<std::string>("title");
    return h("box", {{"border", !title.empty()}, {"title", title}, {"color", t.muted}, {"paddingX", title.empty() ? 0 : 1},
                     {"overflow", p.get<bool>("scroll") ? "scroll" : "visible"}},
             std::move(items));
}

Element ListItem(const Props& p) {
    const Theme& t = useTheme();
    bool selected = p.get<bool>("selected");
    std::string detail = p.get<std::string>("detail");
    std::vector<Element> parts;
    parts.push_back(h("text", {{"color", selected ? t.accent : t.text}, {"bold", selected}, {"flexShrink", 1}},
                      {(selected ? "› " : "  ") + p.get<std::string>("label")}));
    if (!detail.empty()) parts.push_back(h("text", {{"color", t.muted}, {"wrap", false}}, {detail}));
    return h("box", {{"direction", "row"},
                     {"justifyContent", "space-between"},
                     {"gap", 1},
                     {"focusable", true},
                     {"scrollIntoView", selected},  // the selected item stays on screen in a scroll box
                     {"onClick", p.get<Callback>("onSelect")},
                     {"onKey", p.get<KeyHandler>("onKey")}},
             std::move(parts));
}

Element Text(const Props& p) {
    const Theme& t = useTheme();
    bool muted = p.get<bool>("muted");
    return h("text", {{"color", muted ? t.muted : t.text}, {"dim", muted}}, {p.get<std::string>("text")});
}

Element Card(const Props& p) {
    const Theme& t = useTheme();
    return h("box", {{"border", true}, {"title", p.get<std::string>("title")}, {"color", t.muted},
                     {"paddingX", 1}, {"gap", 1}},
             p.children());
}

Element Stack(const Props& p) {
    std::string align = p.get<std::string>("align", "stretch");
    std::string justify = p.get<std::string>("justify", "start");
    return h("box", {{"direction", p.get<std::string>("direction", "column")},
                     {"gap", p.get<int>("gap", 1)},
                     {"alignItems", align},
                     {"justifyContent", justify == "between" ? std::string("space-between") : justify},
                     {"flexWrap", p.get<bool>("wrap") ? "wrap" : "nowrap"}},
             p.children());
}

// "‹ Medium ›": Left / Right step through the options, Enter or a click picks the next.
Element Select(const Props& p) {
    const Theme& t = useTheme();
    std::vector<std::string> values, labels;
    for (const Element& o : p.children()) {
        values.push_back(o.props.get<std::string>("value"));
        labels.push_back(o.props.get<std::string>("label"));
    }
    std::string value = p.get<std::string>("value");
    auto it = std::ranges::find(values, value);
    int current = it == values.end() ? -1 : static_cast<int>(it - values.begin());
    int n = static_cast<int>(values.size());
    auto onChange = p.get<InputHandler>("onChange");
    auto pick = [onChange, values, n](int i) {
        if (onChange && n > 0) onChange(values[static_cast<std::size_t>(((i % n) + n) % n)]);
    };
    std::string shown = current >= 0 ? labels[static_cast<std::size_t>(current)] : "—";
    std::string label = p.get<std::string>("label");

    Element field = h("box", {{"direction", "row"}, {"focusable", true},
                              {"onClick", [pick, current] { pick(current + 1); }},
                              {"onKey", [pick, current](const KeyEvent& e) {
                                   if (e.key == "Right" || e.key == "Enter") pick(current + 1);
                                   else if (e.key == "Left") pick(current - 1);
                               }}},
                      {h("text", {{"color", t.accent}, {"wrap", false}}, {"‹ " + shown + " ›"})});
    if (label.empty()) return field;
    return h("box", {{"direction", "row"}, {"gap", 1}}, {
        h("text", {{"color", t.muted}, {"wrap", false}}, {label}),
        std::move(field),
    });
}

// The text shows under the children while the mouse is over them.
Element Tooltip(const Props& p) {
    auto [shown, setShown] = useState(false);
    std::vector<Element> parts = p.children();
    if (shown)
        parts.push_back(h("text", {{"position", "absolute"}, {"top", 1}, {"left", 0}, {"wrap", false},
                                   {"color", "black"}, {"bg", "gray"}},
                          {" " + p.get<std::string>("text") + " "}));
    return h("box", {{"onMouseEnter", [setShown] { setShown(true); }},
                     {"onMouseLeave", [setShown] { setShown(false); }}},
             std::move(parts));
}

namespace {

struct ToastViewProps {
    detail::ToastItem item{};
    std::string key{};
    bool operator==(const ToastViewProps&) const = default;
};

Element ToastView(const ToastViewProps& p) {
    const Theme& t = useTheme();
    int id = p.item.id;
    useTimeout([id] { detail::dismissToast(id); },
               p.item.duration.count() > 0 ? std::optional(p.item.duration) : std::nullopt);
    const std::string& v = p.item.variant;
    std::string color = v == "primary" ? t.accent : v == "danger" ? t.danger : v == "success" ? t.success : t.muted;
    return h("box", {{"border", true}, {"opaque", true}, {"color", color}, {"bg", t.surface}, {"paddingX", 1},
                     {"onClick", [id] { detail::dismissToast(id); }}},
             {h("text", {{"color", t.text}}, {p.item.text})});
}

}  // namespace

// Bottom right of the screen, over everything else.
Element Toaster(const Props&) {
    const auto& list = useStore(detail::toasts());
    if (list.empty()) return fragment({});
    std::vector<Element> items;
    for (const auto& t : list) items.push_back(h(ToastView, {.item = t, .key = std::to_string(t.id)}));
    return h("box", {{"position", "fixed"}, {"bottom", 0}, {"right", 1}, {"maxWidth", "60%"}, {"alignItems", "end"}},
             std::move(items));
}

}  // namespace rx::ui
