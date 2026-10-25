#include "reactx/ui.hpp"

#include <algorithm>
#include <utility>

#include "reactx/hooks.hpp"

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

// Adds "key" only when set, so elements without a key match by position.
Element withKey(Element e, const std::string& key) {
    e.key = key;
    return e;
}

}  // namespace

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
        {"position", "absolute"}, {"top", 0}, {"left", 0}, {"right", 0}, {"bottom", 0},
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
    return h("box", {{"border", !title.empty()}, {"title", title}, {"color", t.muted}, {"paddingX", title.empty() ? 0 : 1}},
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
                     {"onClick", p.get<Callback>("onSelect")},
                     {"onKey", p.get<KeyHandler>("onKey")}},
             std::move(parts));
}

// ---- typed helpers ----

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
                             {"label", std::move(p.label)}}),
                   p.key);
}

Element checkbox(CheckboxProps p) {
    return withKey(h(Checkbox, {{"label", std::move(p.label)}, {"checked", p.checked}, {"onToggle", std::move(p.onToggle)}}),
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
    return withKey(h(List, {{"title", std::move(p.title)}, {"emptyText", std::move(p.emptyText)}}, std::move(p.items)),
                   p.key);
}

}  // namespace rx::ui
