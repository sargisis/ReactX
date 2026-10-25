// Typed helpers of rx::ui (ui::button({.label = ...}) etc.). Shared by the
// terminal (ui_terminal.cpp) and browser (ui_web.cpp) implementations: they
// only turn the structs into props for the components.
#include <string>
#include <utility>
#include <vector>

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
    return withKey(h(List, {{"title", std::move(p.title)}, {"emptyText", std::move(p.emptyText)}}, std::move(p.items)),
                   p.key);
}

}  // namespace rx::ui
