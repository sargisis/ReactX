#pragma once

// rx::ui — ready-made components for the terminal backend, styled by a theme.
//
// Each component exists twice:
//   - as a plain ReactX component taking Props:  h(ui::Button, {{"label", "OK"}})
//   - as a typed helper with C++20 designated initializers, so typos in prop
//     names don't compile:                       ui::button({.label = "OK", .onClick = save})

#include <cstdint>
#include <string>
#include <vector>

#include "reactx/context.hpp"
#include "reactx/element.hpp"

namespace rx::ui {

// ---- theme ----

// Colors are terminal color names (see terminal.hpp).
struct Theme {
    std::string accent = "cyan";    // primary buttons, active tab, checked boxes
    std::string text = "white";
    std::string muted = "gray";     // labels, hints, borders of lists
    std::string success = "green";
    std::string warning = "yellow";
    std::string danger = "red";
    std::string surface;            // modal background ("" = terminal default)

    bool operator==(const Theme&) const = default;
};

inline const Context<Theme> ThemeContext{Theme{}};

// Gives `theme` to every ui component below.
Element themeProvider(Theme theme, std::vector<Element> children);
// The theme of the nearest themeProvider (or the default one).
const Theme& useTheme();

// ---- components (Props versions) ----
//
// Button    label, onClick, variant (default|primary|danger|success), disabled
// Input     value, onChange(text), onSubmit(text) on Enter, placeholder, label
//           Escape clears the field through onChange.
// Checkbox  label, checked, onToggle
// Tabs      children: tab elements (see ui::tab); initial (index)
// Modal     title, open, onClose (Escape), width, children
//           Put it last inside a box that fills the screen: it covers that box,
//           traps focus while open and gives it back when closed.
// List      title, emptyText, children: list items
// ListItem  label, detail (right side), selected, onSelect (Enter), onKey

Element Button(const Props& props);
Element Input(const Props& props);
Element Checkbox(const Props& props);
Element Tabs(const Props& props);
Element Modal(const Props& props);
Element List(const Props& props);
Element ListItem(const Props& props);

// ---- typed helpers ----
// Every field has a default, so you only name the ones you need (and GCC's
// -Wmissing-field-initializers stays quiet).

enum class Variant { Default, Primary, Danger, Success };

struct ButtonProps {
    std::string label{};
    Callback onClick{};
    Variant variant = Variant::Default;
    bool disabled = false;
    std::string key{};
};
Element button(ButtonProps props);

struct InputProps {
    std::string value{};
    InputHandler onChange{};
    InputHandler onSubmit{};
    std::string placeholder{};
    std::string label{};
    std::string key{};
};
Element input(InputProps props);

struct CheckboxProps {
    std::string label{};
    bool checked = false;
    Callback onToggle{};
    std::string key{};
};
Element checkbox(CheckboxProps props);

struct Tab {
    std::string label{};
    std::vector<Element> content{};
};

struct TabsProps {
    std::vector<Tab> tabs{};
    int initial = 0;
    std::string key{};
};
Element tabs(TabsProps props);

struct ModalProps {
    std::string title{};
    bool open = false;
    Callback onClose{};
    int width = 0;  // 0 = 60% of the screen
    std::vector<Element> children{};
    std::string key{};
};
Element modal(ModalProps props);

struct ListItemProps {
    std::string label{};
    std::string detail{};
    bool selected = false;
    Callback onSelect{};
    KeyHandler onKey{};
    std::string key{};
};
Element listItem(ListItemProps props);

struct ListProps {
    std::string title{};
    std::string emptyText = "nothing here";
    std::vector<Element> items{};
    std::string key{};
};
Element list(ListProps props);

}  // namespace rx::ui
