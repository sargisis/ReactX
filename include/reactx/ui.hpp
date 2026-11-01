#pragma once

// rx::ui — ready-made components for the terminal backend, styled by a theme.
//
// Each component exists twice:
//   - as a plain ReactX component taking Props:  h(ui::Button, {{"label", "OK"}})
//   - as a typed helper with C++20 designated initializers, so typos in prop
//     names don't compile:                       ui::button({.label = "OK", .onClick = save})

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "reactx/context.hpp"
#include "reactx/element.hpp"
#include "reactx/store.hpp"

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
    std::string surface{};          // modal background ("" = terminal default)

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
// Text      text, muted (bool): a paragraph
// Card      title, children: a bordered panel
// Stack     direction (column|row), gap, align (stretch|start|center|end),
//           justify (start|center|end|between), wrap (bool), children.
//           gap is in cells in a terminal and 8px steps in the browser.
// Select    label, value, onChange(value), children: ui-option elements
//           (value, label). Browser: a native <select>; terminal: Left /
//           Right, Enter or a click picks the next option.
// Tooltip   text, children: the text shows while the pointer is over the
//           children (and, in the browser, while they have focus)
// Confirm   title, message, open, confirmLabel, cancelLabel, danger (bool),
//           onConfirm, onCancel (also Escape and a click outside)
// Toaster   shows what ui::toast() sends; put one in the app, last inside a
//           box that fills the screen (like Modal)

Element Button(const Props& props);
Element Input(const Props& props);
Element Checkbox(const Props& props);
Element Tabs(const Props& props);
Element Modal(const Props& props);
Element List(const Props& props);
Element ListItem(const Props& props);
Element Text(const Props& props);
Element Card(const Props& props);
Element Stack(const Props& props);
Element Select(const Props& props);
Element Tooltip(const Props& props);
Element Confirm(const Props& props);
Element Toaster(const Props& props);

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
    bool labelAbove = false;  // the label on its own line above the field: forms line up
    std::string key{};
};
Element input(InputProps props);

struct CheckboxProps {
    std::string label{};
    bool checked = false;
    Callback onToggle{};                     // the box was clicked
    std::function<void(bool)> onChange{};    // ... with the new state: [setOn](bool on) { setOn(on); }
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
    // Scroll inside the list instead of growing: in a terminal the list takes the
    // rows that are left (the selected item stays visible); in the browser it
    // scrolls when its parent limits its height.
    bool scroll = false;
};
Element list(ListProps props);

struct TextProps {
    std::string text{};
    bool muted = false;
    std::string key{};
};
Element text(TextProps props);

struct CardProps {
    std::string title{};
    std::vector<Element> children{};
    std::string key{};
};
Element card(CardProps props);

enum class Direction { Column, Row };
enum class Align { Stretch, Start, Center, End };
enum class Justify { Start, Center, End, Between };

// Children in a column (or a row) with space between them:
//   ui::stack({.direction = ui::Direction::Row, .justify = ui::Justify::End, .children = {...}})
struct StackProps {
    Direction direction = Direction::Column;
    int gap = 1;
    Align align = Align::Stretch;
    Justify justify = Justify::Start;
    bool wrap = false;
    std::vector<Element> children{};
    std::string key{};
};
Element stack(StackProps props);

struct Option {
    std::string value{};
    std::string label{};  // "" = the value
};

struct SelectProps {
    std::string label{};
    std::string value{};
    std::vector<Option> options{};
    InputHandler onChange{};  // gets the chosen option's value
    bool labelAbove = false;  // like ui::input
    std::string key{};
};
Element select(SelectProps props);

struct TooltipProps {
    std::string text{};
    std::vector<Element> children{};
    std::string key{};
};
Element tooltip(TooltipProps props);

// "Delete it?": a modal with a message and two buttons.
//   ui::confirm({.title = "Delete the task?", .open = asking, .danger = true,
//                .onConfirm = remove, .onCancel = [setAsking] { setAsking(false); }})
struct ConfirmProps {
    std::string title{};
    std::string message{};
    bool open = false;
    std::string confirmLabel = "OK";
    std::string cancelLabel = "Cancel";
    bool danger = false;  // the confirm button is red
    Callback onConfirm{};
    Callback onCancel{};  // Cancel, Escape, a click outside
    std::string key{};
};
Element confirm(ConfirmProps props);

// ---- long lists ----
// A list of any length that draws only the rows on screen (and a few around
// them): 100,000 log lines cost what 40 do. Every row has the same height.
//   auto list = ui::useVirtualList({.count = n, .follow = atEnd,
//                                   .onScroll = [setAtEnd](const ScrollEvent& e) { setAtEnd(e.nearBottom(1)); }});
//   std::vector<Element> rows;
//   for (int i = list.first; i < list.last; ++i) rows.push_back(h(Line, {.line = lines[i]}));
//   return list.view(std::move(rows));
// It scrolls by itself: in a terminal it takes the rows left in a column (like
// overflow="scroll"); in the browser give it .height or a flex parent with a height.
// Rows that aren't drawn can't take the focus.
struct VirtualListOptions {
    int count = 0;           // rows in all
    int rowHeight = 32;      // px, in the browser
    int rowLines = 1;        // rows, in a terminal
    int height = 0;          // px, in the browser; 0 = fill a flex parent (at most the window)
    bool follow = false;     // keep the end in view while rows are added
    bool border = false;
    ScrollHandler onScroll{};  // the user scrolled (e.nearBottom(...) to turn follow off and on)
};

struct VirtualList {
    int first = 0;  // draw rows first .. last-1
    int last = 0;
    // The scroll box around `rows` (rows[0] is row `first`), with the room of
    // the rows that aren't drawn.
    Element view(std::vector<Element> rows) const;

    VirtualListOptions options{};
    NodeRef box{};
    Setter<std::pair<int, int>> setViewport{};  // top, height
};
VirtualList useVirtualList(VirtualListOptions options);

// ---- toasts ----
// A short message in a corner that goes away by itself, sent from anywhere on
// the UI thread (a handler, an effect, a fetch callback):
//   ui::toast({.text = "Saved", .variant = ui::Variant::Success});
// ui::toaster() shows them; a click on one closes it.
struct ToastProps {
    std::string text{};
    Variant variant = Variant::Default;
    std::chrono::milliseconds duration = std::chrono::seconds(4);  // 0 = until clicked
};
void toast(ToastProps props);
Element toaster();

namespace detail {
struct ToastItem {
    int id = 0;
    std::string text{};
    std::string variant{};  // default | primary | danger | success
    std::chrono::milliseconds duration{};
    bool operator==(const ToastItem&) const = default;
};
// The toasts on screen, oldest first; dismissToast removes one.
const Store<std::vector<ToastItem>>& toasts();
void dismissToast(int id);
}  // namespace detail

}  // namespace rx::ui
