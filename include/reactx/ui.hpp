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
#include <map>
#include <set>
#include <optional>
#include <string>
#include <vector>

#include "reactx/context.hpp"
#include "reactx/element.hpp"
#include "reactx/hooks.hpp"
#include "reactx/store.hpp"

namespace rx::ui {

// ---- theme ----

// Colors are terminal color names (see terminal.hpp). In the browser each
// name has a light and a dark shade that keep text readable (WCAG AA 4.5:1);
// "#rrggbb" is used as is.
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
Element Textarea(const Props& props);    // value, onChange, placeholder, label, rows, error
Element RadioGroup(const Props& props);  // label, value, onChange, row, error, children: ui-option elements
Element Slider(const Props& props);      // label, value, min, max, step, onChange (int), showValue
Element Progress(const Props& props);    // value, max, label, variant

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

// What a ui::input holds. Password: dots in the terminal, a password field in
// the browser; the others pick the phone's keyboard (and browser checks).
enum class InputType { Text, Password, Email, Number, Search, Tel, Url };

struct InputProps {
    std::string value{};
    InputHandler onChange{};
    InputHandler onSubmit{};
    std::string placeholder{};
    std::string label{};
    bool labelAbove = false;  // the label on its own line above the field: forms line up
    bool disabled = false;    // shown, not editable, skipped by Tab
    std::string error{};      // shown under the field, which is marked invalid ("" = none)
    InputType type = InputType::Text;
    std::string key{};
};
Element input(InputProps props);

struct CheckboxProps {
    std::string label{};
    bool checked = false;
    Callback onToggle{};                     // the box was clicked
    std::function<void(bool)> onChange{};    // ... with the new state: [setOn](bool on) { setOn(on); }
    bool disabled = false;
    std::string error{};                     // shown under the box ("You must agree")
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
    std::vector<Element> items{};     // the old name of `children` (deprecated in v0.20, works until 2.0)
    std::vector<Element> children{};  // the list items (ui::listItem)
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
    // (v0.21.0+) The title's heading level in the browser (<h3>): 2 for cards
    // right under the page's <h1>, so no level is skipped (screen readers, Lighthouse).
    int level = 3;
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
    bool disabled = false;
    std::string error{};
    std::string key{};
};
Element select(SelectProps props);

// ---- more form fields ----

// Several lines of text: Enter starts a new line (in a terminal too, where Up /
// Down move between lines and the field scrolls).
struct TextareaProps {
    std::string value{};
    InputHandler onChange{};
    std::string placeholder{};
    std::string label{};
    int rows = 4;             // the height in lines
    bool disabled = false;
    std::string error{};
    std::string key{};
};
Element textarea(TextareaProps props);

// One of a few options, all visible (a select shows only the chosen one):
//   ui::radioGroup({.label = "Plan", .value = plan, .options = {{"free", "Free"}, {"pro", "Pro"}},
//                   .onChange = [setPlan](const std::string& v) { setPlan(v); }})
// Browser: native radio buttons (arrows move between them); terminal: (•) / ( ),
// Up / Down or a click.
struct RadioGroupProps {
    std::string label{};
    std::string value{};
    std::vector<Option> options{};
    InputHandler onChange{};
    Direction direction = Direction::Column;  // Row: the options side by side
    bool row = false;         // the old way to say direction = Row (deprecated in v0.20, works until 2.0)
    bool disabled = false;
    std::string error{};
    std::string key{};
};
Element radioGroup(RadioGroupProps props);

// A number picked by dragging (browser) or Left / Right (terminal).
struct SliderProps {
    std::string label{};
    int value = 0;
    int min = 0;
    int max = 100;
    int step = 1;
    std::function<void(int)> onChange{};
    bool showValue = true;    // the number next to it
    bool disabled = false;
    std::string error{};      // shown under the slider, which is marked invalid ("" = none)
    std::string key{};
};
Element slider(SliderProps props);

// How far something got: value of max (an upload, steps of a wizard).
struct ProgressProps {
    int value = 0;
    int max = 100;
    std::string label{};      // "Uploading" — the percentage is added
    Variant variant = Variant::Primary;
    std::string key{};
};
Element progress(ProgressProps props);

// ---- forms ----
// The values of a form in your own struct, checked by your function; each
// field's error shows once the user left that field changed, or tried to submit.
//   struct Signup { std::string name, email; bool agree = false; };
//   auto form = ui::useForm<Signup>({.validate = [](const Signup& s) {
//                                        ui::FormErrors e;
//                                        if (s.name.empty()) e["name"] = "Required";
//                                        if (!s.email.contains('@')) e["email"] = "Not an email";
//                                        return e;
//                                    },
//                                    .onSubmit = [](const Signup& s) { send(s); }});
//   ui::input({.value = form.values.name, .onChange = form.set("name", &Signup::name), .label = "Name",
//              .error = form.error("name")})
//   ui::button({.label = "Sign up", .onClick = form.submit})
using FormErrors = std::map<std::string, std::string>;  // field name -> message

template <class T>
struct FormOptions {
    T initial{};
    std::function<FormErrors(const T&)> validate{};
    std::function<void(const T&)> onSubmit{};  // called by submit() when there are no errors
};

template <class T>
struct Form {
    T values{};
    FormErrors errors{};      // all of them, shown or not
    bool submitted = false;   // submit() was tried: every error shows
    Callback submit{};        // checks; calls onSubmit if valid
    Callback reset{};         // back to `initial`, no errors shown
    std::function<void(T)> setValues{};  // replace every value (loaded from a server)

    bool valid() const { return errors.empty(); }
    // The message to show for a field: after it was changed or a submit was tried.
    std::string error(const std::string& name) const {
        auto it = errors.find(name);
        if (it == errors.end() || (!submitted && !touched_.contains(name))) return {};
        return it->second;
    }
    // An onChange for a field: writes it and marks it changed.
    template <class V>
    std::function<void(const V&)> set(std::string name, V T::* field) const {
        return [update = update_, name = std::move(name), field](const V& v) {
            update([&](State& s) {
                s.values.*field = v;
                s.touched.insert(name);
            });
        };
    }

    // ---- inside ----
    struct State {
        T values{};
        std::set<std::string> touched{};
        bool submitted = false;
    };
    std::set<std::string> touched_{};
    std::function<void(const std::function<void(State&)>&)> update_{};
};

template <class T>
Form<T> useForm(FormOptions<T> options = {}) {
    using State = typename Form<T>::State;
    auto [state, setState] = useState(State{options.initial});
    Form<T> form;
    form.values = state.values;
    form.errors = options.validate ? options.validate(state.values) : FormErrors{};
    form.submitted = state.submitted;
    form.touched_ = state.touched;
    form.update_ = [setState](const std::function<void(State&)>& change) {
        setState([&change](State s) {
            change(s);
            return s;
        });
    };
    form.submit = [setState, values = state.values, validate = options.validate, onSubmit = options.onSubmit] {
        setState([](State s) {
            s.submitted = true;
            return s;
        });
        if ((!validate || validate(values).empty()) && onSubmit) onSubmit(values);
    };
    form.reset = [setState, initial = options.initial] { setState(State{initial}); };
    form.setValues = [setState](T values) { setState(State{std::move(values)}); };
    return form;
}

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

// ---- table ----
// Columns with headers that line up in the browser and in a terminal. The
// rows are yours: sort them yourself when a header is clicked.
//   ui::table({.columns = {{.title = "Name", .sortable = true},
//                          {.title = "Qty", .width = 6, .align = ui::Align::End, .sortable = true}},
//              .rows = rows,          // {.cells = {item.name, std::to_string(item.qty)}, .key = item.id}
//              .sortColumn = sortBy, .sortDescending = desc,
//              .onSort = [setSortBy](int column) { setSortBy(column); }})
// Long tables: pass a ui::useVirtualList and only the rows list.first ..
// list.last-1 (rows[0] is row list.first):
//   auto list = ui::useVirtualList({.count = n, .rowHeight = 36, .height = 480});
//   ui::table({.columns = ..., .rows = visibleRows, .list = list})
struct TableColumn {
    std::string title{};
    int width = 0;               // characters (terminal cells, `ch` in the browser); 0 = share the room left
    Align align = Align::Start;  // End for numbers
    bool sortable = false;       // the header is a button that calls onSort
};

struct TableRow {
    std::vector<Element> cells{};  // one per column: text or any element
    std::string key{};             // keeps the row when rows move (sorting)
    Callback onSelect{};           // click or Enter on the row
    bool selected = false;
};

struct TableProps {
    std::vector<TableColumn> columns{};
    std::vector<TableRow> rows{};
    int sortColumn = -1;           // shows an arrow on that header
    bool sortDescending = false;
    std::function<void(int column)> onSort{};
    std::string emptyText{};       // shown when there are no rows
    std::optional<VirtualList> list{};  // draw only the rows on screen (see above)
    std::string key{};
};
Element table(TableProps props);

namespace detail {
Element TableView(const TableProps& props);  // ui_web.cpp / ui_terminal.cpp
}  // namespace detail

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
