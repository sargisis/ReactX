// Tests for rx::ui components on the terminal backend (no real terminal needed).
#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>
#include <iostream>
#include <string>
#include <vector>

#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"
#include "reactx/ui.hpp"

using namespace rx;

namespace {

int failures = 0;
std::vector<std::pair<std::string, std::function<void()>>> tests;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

#define TEST(name)                                                            \
    void name();                                                              \
    const bool name##_registered = (tests.emplace_back(#name, name), true);   \
    void name()

KeyEvent key(std::string k) { return KeyEvent{.key = std::move(k)}; }

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// Renders `component` in a 40x12 terminal and keeps everything alive.
struct Screen {
    std::string out;
    TerminalRenderer renderer{[this](std::string_view s) { out += s; }};
    Root root{renderer};

    explicit Screen(Element e, int w = 40, int hgt = 12) {
        renderer.setSize(w, hgt);
        root.render(std::move(e));
    }
    std::string text() const { return renderer.frameText(); }
    void press(const std::string& k) { renderer.handleKey(root, key(k)); }
    void type(const std::string& s) {
        for (char c : s) press(std::string(1, c));
    }
};

// ---- apps used by tests ----

int clicks = 0;
std::string submitted;

Element Buttons(const Props&) {
    return h("box", {}, {
        ui::button({.label = "Save", .onClick = [] { ++clicks; }, .variant = ui::Variant::Primary}),
        ui::button({.label = "Locked", .disabled = true}),
        ui::button({.label = "Delete", .variant = ui::Variant::Danger}),
    });
}

Element Form(const Props&) {
    auto [name, setName] = useState(std::string{});
    auto [agree, setAgree] = useState(false);
    return h("box", {}, {
        ui::input({.value = name,
                   .onChange = [setName](const std::string& v) { setName(v); },
                   .onSubmit = [](const std::string& v) { submitted = v; },
                   .placeholder = "your name",
                   .label = "Name:"}),
        ui::checkbox({.label = "I agree", .checked = agree, .onToggle = [setAgree, agree] { setAgree(!agree); }}),
    });
}

Element TabsApp(const Props&) {
    return ui::tabs({.tabs = {
                         {"One", {h("text", {}, {"first page"})}},
                         {"Two", {h("text", {}, {"second page"}), ui::button({.label = "inside"})}},
                     }});
}

Element ModalApp(const Props&) {
    auto [open, setOpen] = useState(false);
    auto [deleted, setDeleted] = useState(false);
    return h("box", {{"flexGrow", 1}}, {
        h("text", {}, {deleted ? "deleted" : "BACKGROUND TEXT HERE"}),
        ui::button({.label = "open", .onClick = [setOpen] { setOpen(true); }}),
        ui::button({.label = "other"}),
        ui::modal({.title = "Sure?",
                   .open = open,
                   .onClose = [setOpen] { setOpen(false); },
                   .width = 24,
                   .children = {
                       h("text", {}, {"Delete it?"}),
                       h("box", {{"direction", "row"}, {"justifyContent", "space-between"}}, {
                           ui::button({.label = "yes", .onClick = [setOpen, setDeleted] { setDeleted(true); setOpen(false); },
                                       .variant = ui::Variant::Danger}),
                           ui::button({.label = "no", .onClick = [setOpen] { setOpen(false); }}),
                       }),
                   }}),
    });
}

Element ListApp(const Props& p) {
    auto [selected, setSelected] = useState(std::string{});
    std::vector<Element> items;
    if (p.get<bool>("filled")) {
        for (std::string name : {"apple", "pear"}) {
            items.push_back(ui::listItem({.label = name,
                                          .detail = std::to_string(name.size()) + " letters",
                                          .selected = selected == name,
                                          .onSelect = [setSelected, name] { setSelected(name); },
                                          .key = name}));
        }
    }
    return ui::list({.title = "Fruit", .emptyText = "no fruit", .items = std::move(items)});
}

// ---- tests ----

TEST(buttons_render_and_disabled_is_not_focusable) {
    Screen s(h(Buttons));
    CHECK(s.text() == "[ Save ]\n[ Locked ]\n[ Delete ]\n");
    CHECK(s.renderer.focusable().size() == 2);  // Locked is skipped
    clicks = 0;
    s.press("Enter");
    CHECK(clicks == 1);
}

TEST(button_variants_use_theme_colors) {
    Screen s(h(Buttons));
    CHECK(contains(s.out, ";36;1;7m[ Save ]"));  // primary: accent cyan, bold, focused
    CHECK(contains(s.out, ";31;1m[ Delete ]"));  // danger: red
    CHECK(contains(s.out, ";90;2m[ Locked ]"));  // disabled: muted, dim
}

TEST(theme_provider_changes_colors) {
    ui::Theme theme;
    theme.accent = "magenta";
    Screen s(ui::themeProvider(theme, {h(Buttons)}));
    CHECK(contains(s.out, ";35;1;7m[ Save ]"));
}

TEST(input_types_and_submits) {
    Screen s(h(Form));
    CHECK(contains(s.text(), "Name: _"));  // focused: cursor instead of the placeholder
    s.press("Tab");
    CHECK(contains(s.text(), "Name: your name"));
    s.press("Tab");  // back to the field
    s.type("Ann");
    CHECK(contains(s.text(), "Name: Ann_"));
    submitted.clear();
    s.press("Enter");
    CHECK(submitted == "Ann");
    s.press("Escape");  // clears the field
    CHECK(contains(s.text(), "Name: _"));
}

TEST(input_takes_the_free_room_in_a_row_but_not_in_a_column) {
    // A search box next to a button: the field fills the row up to the button.
    Screen row(ui::stack({.direction = ui::Direction::Row, .children = {
        ui::input({.placeholder = "find"}),
        ui::button({.label = "Go"}),
    }}), 30, 5);
    auto lines = row.renderer.frameLines();
    CHECK(!lines.empty() && lines[0].ends_with("[ Go ]") && lines[0].size() >= 28);
    // In a column (a full-height app) it stays one line high.
    Screen column(h("box", {{"flexGrow", 1}}, {ui::input({.placeholder = "find"}), h("text", {}, {"next"})}), 30, 6);
    auto rows = column.renderer.frameLines();
    CHECK(rows.size() > 1 && contains(rows[1], "next"));
}

Element BoolCheckbox(const Props&) {
    auto [on, setOn] = useState(false);
    return ui::checkbox({.label = on ? "on" : "off", .checked = on, .onChange = [setOn](bool next) { setOn(next); }});
}

TEST(checkbox_on_change_gets_the_new_state) {
    Screen s(h(BoolCheckbox));
    CHECK(contains(s.text(), "[ ] off"));
    s.press("Enter");
    CHECK(contains(s.text(), "[x] on"));
    s.press("Enter");
    CHECK(contains(s.text(), "[ ] off"));
}

TEST(label_above_lines_fields_up) {
    Screen s(ui::stack({.children = {
        ui::input({.value = "Bolt", .label = "Name", .labelAbove = true}),
        ui::input({.value = "42", .label = "Quantity", .labelAbove = true}),
    }}), 30, 8);
    auto lines = s.renderer.frameLines();
    CHECK(lines.size() > 4 && lines[0].starts_with("Name") && lines[1].starts_with("Bolt") &&
          lines[3].starts_with("Quantity") && lines[4].starts_with("42"));
}

struct Stock {
    std::string name;
    int qty = 0;
};

Element StockTable(const Props&) {
    auto [desc, setDesc] = useState(false);
    auto [picked, setPicked] = useState(std::string{});
    std::vector<Stock> items{{"bolt", 120}, {"nut", 7}, {"washer", 45}};
    std::ranges::sort(items, [desc](const Stock& a, const Stock& b) { return desc ? a.qty > b.qty : a.qty < b.qty; });
    std::vector<ui::TableRow> rows;
    for (const auto& it : items)
        rows.push_back({.cells = {it.name, std::to_string(it.qty)}, .key = it.name,
                        .onSelect = [setPicked, name = it.name] { setPicked(name); }, .selected = picked == it.name});
    return h("box", {}, {
        ui::table({.columns = {{.title = "Name"}, {.title = "Qty", .width = 5, .align = ui::Align::End, .sortable = true}},
                   .rows = std::move(rows),
                   .sortColumn = 1,
                   .sortDescending = desc,
                   .onSort = [setDesc, desc](int) { setDesc(!desc); }}),
        h("text", {}, {"picked:" + picked}),
    });
}

TEST(table_lines_up_columns_and_sorts) {
    Screen s(h(StockTable), 30, 10);
    auto lines = s.renderer.frameLines();
    // Border, header, then the rows sorted by quantity, numbers on the right.
    CHECK(lines.size() > 4);
    CHECK(contains(lines[1], "Name") && contains(lines[1], "Qty ▲"));
    CHECK(contains(lines[2], "│ nut") && contains(lines[2], "    7 │"));
    CHECK(contains(lines[4], "│ bolt") && contains(lines[4], "  120 │"));
    s.press("Enter");  // the focus starts on the sortable header
    lines = s.renderer.frameLines();
    CHECK(contains(lines[1], "Qty ▼") && contains(lines[2], "bolt") && contains(lines[4], "nut"));
    s.press("Tab");    // the first row
    s.press("Enter");
    CHECK(contains(s.text(), "picked:bolt"));
}

TEST(table_shows_empty_text) {
    Screen s(ui::table({.columns = {{.title = "Name"}}, .emptyText = "Nothing here"}), 30, 6);
    CHECK(contains(s.text(), "Name") && contains(s.text(), "Nothing here"));
}

struct BigTableProps {
    int count = 0;
};

Element BigTable(const BigTableProps& p) {
    auto list = ui::useVirtualList({.count = p.count});
    std::vector<ui::TableRow> rows;
    for (int i = list.first; i < list.last; ++i)
        rows.push_back({.cells = {"row " + std::to_string(i), std::to_string(i * 2)}, .key = std::to_string(i)});
    return ui::table({.columns = {{.title = "Row"}, {.title = "Double", .width = 8, .align = ui::Align::End}},
                      .rows = std::move(rows), .list = list});
}

TEST(table_with_a_virtual_list_draws_only_rows_on_screen) {
    Screen s(h(BigTable, {.count = 50000}), 30, 12);
    CHECK(contains(s.text(), "row 0") && contains(s.text(), "Double"));
    CHECK(s.renderer.nodeCount() < 400);
    s.press("PageDown");
    s.root.flush();
    CHECK(!contains(s.text(), "row 0 ") && contains(s.text(), "Double"));  // the header stays
}

struct BigLogProps {
    int count = 0;
    bool follow = false;
};

Element BigLog(const BigLogProps& p) {
    auto list = ui::useVirtualList({.count = p.count, .follow = p.follow, .border = true});
    std::vector<Element> rows;
    for (int i = list.first; i < list.last; ++i) rows.push_back(h("text", {}, {"line " + std::to_string(i)}));
    return h("box", {{"flexGrow", 1}}, {h("text", {}, {"head"}), list.view(std::move(rows))});
}

TEST(virtual_list_draws_only_the_rows_on_screen) {
    Screen s(h(BigLog, {.count = 100000}), 30, 12);
    CHECK(contains(s.text(), "line 0"));
    CHECK(!contains(s.text(), "line 99999"));
    CHECK(s.renderer.nodeCount() < 200);  // not 100,000 rows
    s.press("PageDown");
    s.root.flush();
    auto lines = s.renderer.frameLines();
    CHECK(lines.size() > 2 && !contains(lines[2], "line 0") && contains(s.text(), "line 12"));  // a page further
}

TEST(virtual_list_follow_keeps_the_end_in_view) {
    Screen s(h(BigLog, {.count = 5000, .follow = true}), 30, 12);
    s.root.flush();
    CHECK(contains(s.text(), "line 4999"));
    s.root.render(h(BigLog, {.count = 5100, .follow = true}));
    s.root.flush();
    CHECK(contains(s.text(), "line 5099"));
    CHECK(s.renderer.nodeCount() < 200);
}

struct LogLineProps {
    int n = 0;
};

Element LogLine(const LogLineProps& p) {
    std::string level = p.n % 3 == 0 ? "ERROR" : "INFO";
    return h("box", {{"direction", "row"}, {"gap", 1}}, {
        h("text", {{"color", "gray"}, {"wrap", false}}, {std::to_string(p.n)}),
        h("text", {{"color", level == "ERROR" ? "red" : "white"}, {"bold", level == "ERROR"}, {"wrap", false}}, {level}),
        h("text", {{"wrap", false}}, {"line " + std::to_string(p.n)}),
    });
}

struct GrowingLogProps {
    int* count = nullptr;
};

Element GrowingLog(const GrowingLogProps& p) {
    auto [n, setN] = useState(0);
    auto [follow, setFollow] = useState(true);
    *p.count = n;
    useInterval([setN] { setN([](int v) { return v + 20; }); }, std::optional(std::chrono::milliseconds(1)));
    auto list = ui::useVirtualList({.count = n, .follow = follow, .border = true,
                                    .onScroll = [setFollow](const ScrollEvent& e) { setFollow(e.nearBottom(1)); }});
    std::vector<Element> rows;
    for (int i = list.first; i < list.last; ++i) rows.push_back(h(LogLine, {.n = i}));
    return h("box", {{"flexGrow", 1}}, {h("text", {}, {follow ? "following" : "stopped"}), h("text", {}, {std::to_string(n)}), list.view(std::move(rows))});
}

TEST(virtual_list_follows_rows_that_keep_coming) {
    int count = 0;
    Screen s(h(GrowingLog, {.count = &count}), 80, 14);
    int blanks = 0;
    for (int tick = 0; tick < 60; ++tick) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        s.root.flush();
        s.renderer.sendScrollEvents(s.root);
        auto lines = s.renderer.frameLines();
        if (lines.size() > 4 && lines[4].find("line") == std::string::npos) {
            ++blanks;
            if (blanks == 1) std::cerr << "blank at tick " << tick << "\n" << s.text() << "\n";
        }
    }
    CHECK(blanks == 0);
    CHECK(contains(s.text(), "following"));
    CHECK(contains(s.text(), "line " + std::to_string(count - 1)));
    if (!contains(s.text(), "line " + std::to_string(count - 1))) std::cerr << s.text() << "\ncount " << count << "\n";
}

TEST(checkbox_toggles_with_enter_and_space) {
    Screen s(h(Form));
    CHECK(contains(s.text(), "[ ] I agree"));
    s.press("Tab");
    s.press("Enter");
    CHECK(contains(s.text(), "[x] I agree"));
    s.press(" ");
    CHECK(contains(s.text(), "[ ] I agree"));
}

TEST(tabs_switch_content) {
    Screen s(h(TabsApp));
    CHECK(contains(s.text(), " One   Two\n"));
    CHECK(contains(s.text(), "first page"));
    CHECK(s.renderer.focusable().size() == 2);  // two tab headers
    s.press("Tab");
    s.press("Enter");
    CHECK(contains(s.text(), "second page"));
    CHECK(!contains(s.text(), "first page"));
    CHECK(s.renderer.focusable().size() == 3);  // + the button inside tab Two
}

TEST(tabs_initial_index) {
    Screen s(ui::tabs({.tabs = {{"A", {h("text", {}, {"aaa"})}}, {"B", {h("text", {}, {"bbb"})}}}, .initial = 1}));
    CHECK(contains(s.text(), "bbb"));
}

TEST(modal_is_hidden_until_open) {
    Screen s(h(ModalApp));
    CHECK(!contains(s.text(), "Sure?"));
    CHECK(s.renderer.focusable().size() == 2);
}

TEST(modal_covers_background_and_traps_focus) {
    Screen s(h(ModalApp), 30, 7);  // the modal (7 rows) is as tall as the screen
    s.press("Enter");  // "open" is focused first
    std::string screen = s.text();
    CHECK(contains(screen, "BAC┌─ Sure? "));  // the box hides the background under it...
    CHECK(!contains(screen, "BACKGROUND"));    // ...but not what is outside the box
    CHECK(contains(screen, "Delete it?"));

    auto inside = s.renderer.focusable();
    CHECK(inside.size() == 2);  // only yes / no
    s.press("Tab");
    s.press("Tab");
    CHECK(s.renderer.focused() == inside[0]);  // Tab wraps inside the modal
}

TEST(modal_closes_on_escape_and_restores_focus) {
    Screen s(h(ModalApp));
    NodeHandle openButton = s.renderer.focused();
    s.press("Enter");
    CHECK(contains(s.text(), "Sure?"));
    s.press("Escape");  // bubbles from the focused button to the modal
    CHECK(!contains(s.text(), "Sure?"));
    CHECK(s.renderer.focused() == openButton);
}

// A modal with only text in it has nothing to focus: Escape must still close it.
Element TextModalApp(const Props&) {
    auto [open, setOpen] = useState(false);
    return h("box", {{"flexGrow", 1}}, {
        ui::button({.label = "open", .onClick = [setOpen] { setOpen(true); }}),
        ui::modal({.title = "Note", .open = open, .onClose = [setOpen] { setOpen(false); },
                   .children = {h("text", {}, {"just text"})}}),
    });
}

TEST(modal_without_buttons_closes_on_escape) {
    Screen s(h(TextModalApp));
    NodeHandle openButton = s.renderer.focused();
    s.press("Enter");
    CHECK(contains(s.text(), "just text"));
    CHECK(s.renderer.focused() == 0);  // nothing to focus inside
    s.press("Escape");
    CHECK(!contains(s.text(), "just text"));
    CHECK(s.renderer.focused() == openButton);
}

TEST(modal_button_acts) {
    Screen s(h(ModalApp));
    s.press("Enter");
    s.press("Enter");  // "yes" is focused first inside the modal
    CHECK(contains(s.text(), "deleted"));
    CHECK(!contains(s.text(), "Sure?"));
}

TEST(list_shows_empty_text) {
    Screen s(h(ListApp, {{"filled", false}}));
    CHECK(contains(s.text(), "┌─ Fruit "));
    CHECK(contains(s.text(), "no fruit"));
}

TEST(list_items_select_with_enter) {
    Screen s(h(ListApp, {{"filled", true}}), 30, 8);
    auto lines = s.renderer.frameLines();
    CHECK(lines.at(1) == "│   apple          5 letters │");
    s.press("Tab");
    s.press("Enter");
    CHECK(contains(s.text(), "› pear"));
    CHECK(!contains(s.text(), "› apple"));
}

TEST(key_events_bubble_to_ancestors) {
    std::vector<std::string> seen;
    struct App {
        static Element render(const Props& p) {
            auto* log = reinterpret_cast<std::vector<std::string>*>(p.get<std::int64_t>("log"));
            return h("box", {{"onKey", [log](const KeyEvent& e) { log->push_back("outer:" + e.key); }}}, {
                h("box", {{"onKey", [log](const KeyEvent& e) { log->push_back("inner:" + e.key); }}}, {
                    h("button", {{"onKey", [log](const KeyEvent& e) { log->push_back("button:" + e.key); }}}, {"b"}),
                }),
            });
        }
    };
    Screen s(h(&App::render, {{"log", reinterpret_cast<std::int64_t>(&seen)}}));
    s.press("x");
    CHECK((seen == std::vector<std::string>{"button:x", "inner:x", "outer:x"}));
}

// ---- text, card, stack, select, confirm, tooltip, toasts ----

TEST(card_and_stack_lay_out_their_children) {
    Screen s(ui::card({.title = "Stats", .children = {
                           ui::stack({.direction = ui::Direction::Row, .gap = 2, .children = {
                                          ui::text({.text = "one"}), ui::text({.text = "two", .muted = true})}}),
                       }}));
    CHECK(contains(s.text(), "┌─ Stats"));
    CHECK(contains(s.text(), "one  two"));  // a row, two cells apart
}

Element SelectApp(const Props&) {
    auto [size, setSize] = useState(std::string("m"));
    return h("box", {}, {
        ui::select({.label = "Size:", .value = size,
                    .options = {{"s", "Small"}, {"m", "Medium"}, {"l", "Large"}},
                    .onChange = [setSize](const std::string& v) { setSize(v); }}),
        h("text", {}, {"size=" + size}),
    });
}

TEST(select_steps_through_options_with_arrows) {
    Screen s(h(SelectApp));
    CHECK(contains(s.text(), "Size: ‹ Medium ›"));
    s.press("Right");
    CHECK(contains(s.text(), "‹ Large ›") && contains(s.text(), "size=l"));
    s.press("Right");  // wraps around
    CHECK(contains(s.text(), "size=s"));
    s.press("Left");
    CHECK(contains(s.text(), "size=l"));
}

Element ConfirmApp(const Props&) {
    auto [open, setOpen] = useState(true);
    auto [result, setResult] = useState(std::string("none"));
    return h("box", {{"flexGrow", 1}}, {
        h("text", {}, {"result=" + result}),
        ui::confirm({.title = "Delete?", .message = "It can't be undone.", .open = open,
                     .confirmLabel = "Delete", .danger = true,
                     .onConfirm = [setOpen, setResult] { setResult("deleted"); setOpen(false); },
                     .onCancel = [setOpen, setResult] { setResult("kept"); setOpen(false); }}),
    });
}

TEST(confirm_asks_and_answers) {
    {
        Screen s(h(ConfirmApp), 50, 14);
        CHECK(contains(s.text(), "Delete?") && contains(s.text(), "It can't be undone."));
        CHECK(contains(s.text(), "[ Cancel ]") && contains(s.text(), "[ Delete ]"));
        s.press("Tab");  // Cancel -> Delete
        s.press("Enter");
        CHECK(contains(s.text(), "result=deleted") && !contains(s.text(), "It can't"));
    }
    {
        Screen s(h(ConfirmApp), 50, 14);
        s.press("Escape");
        CHECK(contains(s.text(), "result=kept"));
    }
}

TEST(tooltip_shows_while_the_mouse_is_over) {
    Screen s(h("box", {}, {ui::tooltip({.text = "Saves the file", .children = {ui::button({.label = "Save"})}})}));
    CHECK(!contains(s.text(), "Saves the file"));
    auto over = MouseInput{.action = MouseInput::Action::Move, .x = 2, .y = 0};
    s.renderer.handleMouse(s.root, over);
    CHECK(contains(s.text(), "Saves the file"));
    s.renderer.handleMouse(s.root, MouseInput{.action = MouseInput::Action::Move, .x = 30, .y = 8});
    CHECK(!contains(s.text(), "Saves the file"));
}

TEST(toasts_show_go_away_and_close_on_click) {
    Screen s(h("box", {{"flexGrow", 1}}, {h("text", {}, {"app"}), ui::toaster()}), 40, 10);
    ui::toast({.text = "Saved", .variant = ui::Variant::Success, .duration = std::chrono::milliseconds(0)});
    ui::toast({.text = "Quick", .duration = std::chrono::milliseconds(1)});
    s.root.flush();
    CHECK(contains(s.text(), "Saved") && contains(s.text(), "Quick"));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    s.root.flush();  // runs due timers: the 1 ms one is gone
    CHECK(contains(s.text(), "Saved") && !contains(s.text(), "Quick"));
    auto lines = s.renderer.frameLines();
    int y = 0, x = 0;
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (auto at = lines[i].find("Saved"); at != std::string::npos) y = static_cast<int>(i), x = static_cast<int>(at);
    s.renderer.handleMouse(s.root, MouseInput{.action = MouseInput::Action::Down, .x = x, .y = y});
    s.renderer.handleMouse(s.root, MouseInput{.action = MouseInput::Action::Up, .x = x, .y = y});
    CHECK(!contains(s.text(), "Saved"));
}

// ---- scrolling (overflow="scroll") ----

ScrollEvent lastScroll;

// A header, a scroll box with `rows` lines, a footer: in a 40x12 terminal.
Element Scroller(const Props& p) {
    int rows = p.get<int>("rows", 30);
    std::vector<Element> lines;
    for (int i = 0; i < rows; ++i)
        lines.push_back(h("button", {{"key", std::to_string(i)}}, {"line " + std::to_string(i)}));
    return h("box", {{"flexGrow", 1}}, {
        h("text", {}, {"HEADER"}),
        h("box", {{"overflow", "scroll"}, {"border", true}, {"id", "box"},
                  {"onScroll", [](const ScrollEvent& e) { lastScroll = e; }}},
          std::move(lines)),
        h("text", {}, {"FOOTER"}),
    });
}

NodeHandle byId(Screen& s, const std::string& id) { return *s.renderer.findByProp("id", id); }

TEST(scroll_box_takes_the_room_left_and_clips) {
    Screen s(h(Scroller));  // 40x12: header, box (10 rows, 8 inside the border), footer
    std::string screen = s.text();
    CHECK(contains(screen, "HEADER") && contains(screen, "FOOTER"));  // the footer isn't pushed off
    CHECK(contains(screen, "line 0") && contains(screen, "line 7") && !contains(screen, "line 8"));
    CHECK(contains(screen, "┃"));  // where we are, on the right border
}

TEST(scroll_by_moves_the_content_and_sends_onScroll) {
    Screen s(h(Scroller));
    NodeHandle box = byId(s, "box");
    lastScroll = {};
    CHECK(s.renderer.scrollBy(box, 5));
    CHECK(s.renderer.scrollTop(box) == 5);
    CHECK(!contains(s.text(), "line 4 ") && contains(s.text(), "line 5") && contains(s.text(), "line 12"));
    s.renderer.sendScrollEvents(s.root);
    CHECK(lastScroll.top == 5 && lastScroll.scrollHeight == 30 && lastScroll.clientHeight == 8);
    CHECK(!s.renderer.scrollBy(box, -100) || s.renderer.scrollTop(box) == 0);  // clamped at the top
    s.renderer.scrollBy(box, 1000);
    CHECK(s.renderer.scrollTop(box) == 22);  // 30 lines, 8 visible
    CHECK(contains(s.text(), "line 29") && contains(s.text(), "FOOTER"));
}

TEST(scroll_follows_the_focus) {
    Screen s(h(Scroller));
    for (int i = 0; i < 20; ++i) s.press("Down");
    CHECK(contains(s.text(), "line 20"));  // the focused line is on screen
    CHECK(s.renderer.scrollTop(byId(s, "box")) == 13);  // the least scroll that shows it
    for (int i = 0; i < 20; ++i) s.press("Up");
    CHECK(s.renderer.scrollTop(byId(s, "box")) == 0);
}

TEST(page_down_and_the_wheel_scroll) {
    Screen s(h(Scroller));
    NodeHandle box = byId(s, "box");
    lastScroll = {};
    s.press("PageDown");
    CHECK(s.renderer.scrollTop(box) == 7);  // a page is the visible rows minus one
    CHECK(lastScroll.top == 7);             // handleKey sends onScroll
    s.press("PageUp");
    CHECK(s.renderer.scrollTop(box) == 0);
    s.renderer.handleMouse(s.root, MouseInput{.action = MouseInput::Action::WheelDown, .x = 5, .y = 4});
    CHECK(s.renderer.scrollTop(box) == 3);
    s.renderer.handleMouse(s.root, MouseInput{.action = MouseInput::Action::WheelUp, .x = 5, .y = 4});
    CHECK(s.renderer.scrollTop(box) == 0);
    // Over the header nothing scrolls.
    CHECK(!s.renderer.handleMouse(s.root, MouseInput{.action = MouseInput::Action::WheelDown, .x = 2, .y = 0}));
}

TEST(page_down_scrolls_while_typing_in_an_input) {
    Element (*app)(const Props&) = [](const Props&) -> Element {
        auto [q, setQ] = useState(std::string{});
        return h("box", {{"flexGrow", 1}}, {
            ui::input({.value = q, .onChange = [setQ](const std::string& t) { setQ(t); }}),  // has its own onKey
            h(Scroller),
        });
    };
    Screen s(h(app));
    s.press("PageDown");
    CHECK(s.renderer.scrollTop(byId(s, "box")) > 0);
}

// Keep the reader's place when rows are added above (a chat loading older messages).
Element Prepend(const Props&) {
    auto [rows, setRows] = useState(20);
    auto box = useNodeRef();
    auto& before = useRef(std::optional<ScrollEvent>{});
    useEffect([box, &before] {
        if (auto now = box.scrollMetrics(); now && before) box.scrollBy(now->scrollHeight - before->scrollHeight);
    }, deps(rows));
    std::vector<Element> lines;
    for (int i = 100 - rows; i < 100; ++i) lines.push_back(h("text", {{"key", std::to_string(i)}}, {"msg " + std::to_string(i)}));
    return h("box", {{"flexGrow", 1}}, {
        h("button", {{"id", "older"}, {"onClick", [setRows, box, &before] {
             before = box.scrollMetrics();
             setRows([](int r) { return r + 10; });
         }}}, {"older"}),
        h("box", {{"overflow", "scroll"}, {"id", "box"}, {"ref", box}}, std::move(lines)),
    });
}

TEST(scroll_metrics_keep_the_place_when_rows_come_above) {
    Screen s(h(Prepend));  // 40x12: the box shows 11 of 20 rows
    NodeHandle box = byId(s, "box");
    auto m = s.renderer.scrollMetrics(box);
    CHECK(m && m->top == 0 && m->scrollHeight == 20 && m->clientHeight == 11);
    CHECK(s.renderer.scrollTo(box, 2) && s.renderer.scrollTop(box) == 2);
    CHECK(contains(s.text(), "msg 82") && !contains(s.text(), "msg 81"));
    s.root.dispatch(byId(s, "older"), "onClick");  // 10 rows above
    CHECK(s.renderer.scrollTop(box) == 12);       // the same rows are still on screen
    CHECK(contains(s.text(), "msg 82") && !contains(s.text(), "msg 81"));
    CHECK(!s.renderer.scrollMetrics(byId(s, "older")));  // not a scroll box
}

TEST(scroll_clamps_when_the_content_shrinks) {
    Element (*app)(const Props&) = [](const Props&) -> Element {
        auto [rows, set] = useState(30);
        return h("box", {{"flexGrow", 1}}, {
            h("button", {{"id", "less"}, {"onClick", [set] { set(3); }}}, {"less"}),
            h(Scroller, {{"rows", rows}}),
        });
    };
    Screen s(h(app));
    NodeHandle box = byId(s, "box");
    s.renderer.scrollBy(box, 10);
    s.root.dispatch(byId(s, "less"), "onClick");
    CHECK(s.renderer.scrollTop(box) == 0);
    CHECK(contains(s.text(), "line 2"));
}

// A page taller than the terminal: a modal and toasts still show on the screen.
Element TallPage(const Props&) {
    auto [open, setOpen] = useState(false);
    std::vector<Element> rows;
    for (int i = 0; i < 40; ++i) rows.push_back(h("text", {{"key", std::to_string(i)}}, {"row " + std::to_string(i)}));
    rows.push_back(ui::button({.label = "open", .onClick = [setOpen] { setOpen(true); }, .key = "open"}));
    rows.push_back(ui::modal({.title = "Hi", .open = open, .onClose = [setOpen] { setOpen(false); },
                              .children = {h("text", {}, {"modal body"})}, .key = "modal"}));
    rows.push_back(ui::toaster());
    return h("box", {{"flexGrow", 1}}, std::move(rows));
}

TEST(fixed_modal_and_toasts_stay_on_the_screen) {
    Screen s(h(TallPage));
    s.press("Enter");  // "open" is the only focusable node
    CHECK(contains(s.text(), "modal body"));
    s.press("Escape");
    CHECK(!contains(s.text(), "modal body"));
    ui::toast({.text = "Saved", .duration = std::chrono::milliseconds(0)});
    s.root.flush();
    CHECK(contains(s.text(), "Saved"));
    ui::detail::toasts().set({});  // don't leak into other tests
}

// ui::listItem keeps the selected item visible (selection without focus, like useKeyboard).
Element LongList(const Props&) {
    auto [sel, setSel] = useState(0);
    std::vector<Element> items;
    for (int i = 0; i < 30; ++i)
        items.push_back(ui::listItem({.label = "item " + std::to_string(i), .selected = i == sel, .key = std::to_string(i)}));
    return h("box", {{"flexGrow", 1}}, {
        h("button", {{"id", "next"}, {"onClick", [setSel] { setSel([](int v) { return v + 1; }); }}}, {"next"}),
        h("box", {{"overflow", "scroll"}, {"id", "box"}}, {ui::list({.items = std::move(items)})}),
    });
}

TEST(selected_list_item_scrolls_into_view) {
    Screen s(h(LongList));
    for (int i = 0; i < 15; ++i) s.root.dispatch(byId(s, "next"), "onClick");
    CHECK(contains(s.text(), "› item 15"));
    CHECK(s.renderer.scrollTop(byId(s, "box")) > 0);
}

}  // namespace

int main() {
    for (auto& [name, fn] : tests) {
        std::cout << name << '\n';
        try {
            fn();
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "  EXCEPTION: " << e.what() << '\n';
        }
    }
    std::cout << (failures ? "FAILED: " : "OK, all tests passed") << (failures ? std::to_string(failures) : "")
              << '\n';
    return failures ? 1 : 0;
}
