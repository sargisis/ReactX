// Typed helpers of rx::ui (ui::button({.label = ...}) etc.). Shared by the
// terminal (ui_terminal.cpp) and browser (ui_web.cpp) implementations: they
// only turn the structs into props for the components.
#include <string>
#include <utility>
#include <vector>

#include "reactx/hooks.hpp"
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
    return withKey(h(List, {{"title", std::move(p.title)}, {"emptyText", std::move(p.emptyText)}, {"scroll", p.scroll}},
                     std::move(p.items)),
                   p.key);
}

Element text(TextProps p) {
    return withKey(h(Text, {{"text", std::move(p.text)}, {"muted", p.muted}}), p.key);
}

Element card(CardProps p) {
    return withKey(h(Card, {{"title", std::move(p.title)}}, std::move(p.children)), p.key);
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

Element select(SelectProps p) {
    std::vector<Element> options;
    for (auto& o : p.options) {
        std::string label = o.label.empty() ? o.value : o.label;
        options.push_back(h("ui-option", {{"key", o.value}, {"value", std::move(o.value)}, {"label", std::move(label)}}));
    }
    return withKey(h(Select, {{"label", std::move(p.label)}, {"value", std::move(p.value)}, {"onChange", std::move(p.onChange)}},
                     std::move(options)),
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

}  // namespace rx::ui
