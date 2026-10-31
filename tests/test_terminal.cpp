// Tests for the terminal backend: key decoding, layout, focus and input.
// Nothing here needs a real terminal.
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"

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

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// ---- components ----

Element Form(const Props&) {
    auto [name, setName] = useState(std::string{});
    auto [clicks, setClicks] = useState(0);
    return h("box", {}, {
        h("input", {{"value", name},
                    {"placeholder", "name"},
                    {"onInput", [setName](const std::string& t) { setName(t); }}}),
        h("button", {{"onClick", [setClicks] { setClicks([](int c) { return c + 1; }); }}}, {"ok"}),
        h("text", {}, {"name=" + name + " clicks=" + std::to_string(clicks)}),
    });
}

Element Removable(const Props&) {
    auto [items, setItems] = useState(std::vector<std::string>{"a", "b", "c"});
    std::vector<Element> rows;
    for (const auto& item : items) {
        rows.push_back(h("item", {{"key", item}, {"focusable", true}, {"onClick", [setItems, item] {
                                     setItems([item](std::vector<std::string> v) {
                                         std::erase(v, item);
                                         return v;
                                     });
                                 }}}, {h("text", {}, {item})}));
    }
    return h("box", {}, std::move(rows));
}

// ---- key decoding ----

TEST(decodes_plain_and_named_keys) {
    auto keys = decodeKeys("a\r\t\x7f");
    CHECK(keys.size() == 4);
    CHECK(keys[0] == key("a"));
    CHECK(keys[1] == key("Enter"));
    CHECK(keys[2] == key("Tab"));
    CHECK(keys[3] == key("Backspace"));
}

TEST(decodes_escape_sequences) {
    auto keys = decodeKeys("\x1b[A\x1b[B\x1b[C\x1b[D\x1b[3~\x1b[Z\x1bOH");
    CHECK(keys.size() == 7);
    CHECK(keys[0] == key("Up"));
    CHECK(keys[1] == key("Down"));
    CHECK(keys[2] == key("Right"));
    CHECK(keys[3] == key("Left"));
    CHECK(keys[4] == key("Delete"));
    CHECK((keys[5] == KeyEvent{.key = "Tab", .shift = true}));
    CHECK(keys[6] == key("Home"));
}

TEST(decodes_modifiers) {
    auto keys = decodeKeys("\x03\x1bx\x1b[1;5A");
    CHECK(keys.size() == 3);
    CHECK((keys[0] == KeyEvent{.key = "c", .ctrl = true}));
    CHECK((keys[1] == KeyEvent{.key = "x", .alt = true}));
    CHECK((keys[2] == KeyEvent{.key = "Up", .ctrl = true}));
}

TEST(decodes_utf8_and_lone_escape) {
    auto keys = decodeKeys("я€\x1b");
    CHECK(keys.size() == 3);
    CHECK(keys[0] == key("я"));
    CHECK(keys[1] == key("€"));
    CHECK(keys[2] == key("Escape"));
}

// ---- layout ----

TEST(box_with_border_and_title) {
    TerminalRenderer r;
    r.setSize(20, 10);
    Root root(r);
    root.render(h("box", {{"border", true}, {"title", "Hi"}, {"width", 12}}, {
        h("text", {}, {"one"}),
        h("text", {}, {"два"}),
    }));
    auto lines = r.frameLines();
    CHECK(lines.size() == 4);
    CHECK(lines[0] == "┌─ Hi ─────┐");
    CHECK(lines[1] == "│one       │");
    CHECK(lines[2] == "│два       │");
    CHECK(lines[3] == "└──────────┘");
}

std::vector<std::string> focusLog;
Element FocusForm(const Props&) {
    auto log = [](std::string what) { return Callback([what] { focusLog.push_back(what); }); };
    return h("box", {}, {
        h("input", {{"id", "name"}, {"value", ""}, {"onFocus", log("name+")}, {"onBlur", log("name-")}}),
        h("button", {{"id", "ok"}, {"onFocus", log("ok+")}, {"onBlur", log("ok-")}}, {"OK"}),
    });
}

TEST(tab_moves_focus_with_focus_and_blur_events) {
    focusLog.clear();
    TerminalRenderer r;
    r.setSize(40, 10);
    Root root(r);
    root.render(h(FocusForm));
    CHECK(r.editingText());  // the input has focus first
    r.handleKey(root, KeyEvent{.key = "Tab"});
    CHECK(!r.editingText());  // now the button
    r.handleKey(root, KeyEvent{.key = "Tab"});
    CHECK(focusLog == (std::vector<std::string>{"name-", "ok+", "ok-", "name+"}));
    r.handleKey(root, KeyEvent{.key = "a"});  // typing doesn't move focus: no events
    CHECK(focusLog.size() == 4);
}

TEST(ascii_borders_for_non_utf8_locales) {
    TerminalRenderer r;
    r.setSize(20, 10);
    r.setUnicode(false);  // what TerminalApp does when std::text_encoding says the locale isn't UTF-8
    Root root(r);
    root.render(h("box", {{"border", true}, {"title", "Hi"}, {"width", 12}}, {h("text", {}, {"one"})}));
    auto lines = r.frameLines();
    CHECK(lines.size() == 3);
    CHECK(lines[0] == "+- Hi -----+");
    CHECK(lines[1] == "|one       |");
    CHECK(lines[2] == "+----------+");
}

TEST(cells_keep_multibyte_characters) {  // a cell is a std::inplace_vector<char, 4>
    TerminalRenderer r;
    r.setSize(10, 3);
    Root root(r);
    root.render(h("text", {}, {"€a𝄞ж"}));  // 3, 1, 4 and 2 bytes
    CHECK(r.frameLines().at(0) == "€a𝄞ж");
}

TEST(row_direction_places_children_side_by_side) {
    TerminalRenderer r;
    r.setSize(30, 5);
    Root root(r);
    root.render(h("box", {{"direction", "row"}, {"gap", 1}}, {
        h("text", {}, {"left"}),
        h("button", {}, {"go"}),
        h("text", {}, {"end"}),
    }));
    CHECK(r.frameLines().at(0) == "left [ go ] end");
}

TEST(padding_and_gap) {
    TerminalRenderer r;
    r.setSize(20, 10);
    Root root(r);
    root.render(h("box", {{"padding", 1}, {"gap", 1}}, {h("text", {}, {"a"}), h("text", {}, {"b"})}));
    auto lines = r.frameLines();  // trailing empty lines are dropped
    CHECK(lines.size() == 4);
    CHECK(lines[0].empty());
    CHECK(lines[1] == " a");
    CHECK(lines[2].empty());
    CHECK(lines[3] == " b");
}

TEST(long_text_is_clipped_with_wrap_false) {
    TerminalRenderer r;
    r.setSize(5, 3);
    Root root(r);
    root.render(h("text", {{"wrap", false}}, {"abcdefgh"}));
    CHECK(r.frameText() == "abcde\n");
}

// ---- focus and input ----

TEST(first_focusable_gets_focus_and_tab_cycles) {
    TerminalRenderer r;
    Root root(r);
    root.render(h(Form));
    auto list = r.focusable();
    CHECK(list.size() == 2);
    CHECK(r.focused() == list[0]);
    CHECK(r.handleKey(root, key("Tab")));
    CHECK(r.focused() == list[1]);
    r.handleKey(root, key("Tab"));
    CHECK(r.focused() == list[0]);  // wraps around
    r.handleKey(root, KeyEvent{.key = "Tab", .shift = true});
    CHECK(r.focused() == list[1]);
}

TEST(typing_edits_the_focused_input) {
    TerminalRenderer r;
    Root root(r);
    root.render(h(Form));
    for (const char* k : {"h", "i", "я"}) r.handleKey(root, key(k));
    CHECK(contains(r.frameText(), "name=hiя "));
    r.handleKey(root, key("Backspace"));
    CHECK(contains(r.frameText(), "name=hi "));
    CHECK(contains(r.frameText(), "hi_"));  // cursor in the focused field
}

TEST(enter_clicks_the_focused_button) {
    TerminalRenderer r;
    Root root(r);
    root.render(h(Form));
    r.handleKey(root, key("Tab"));
    CHECK(r.handleKey(root, key("Enter")));
    CHECK(r.handleKey(root, key(" ")));
    CHECK(contains(r.frameText(), "clicks=2"));
}

TEST(focus_moves_to_neighbour_when_focused_node_is_removed) {
    TerminalRenderer r;
    Root root(r);
    root.render(h(Removable));
    r.handleKey(root, key("Down"));  // focus "b"
    r.handleKey(root, key("Enter"));  // remove "b"
    CHECK(r.frameText() == "a\nc\n");
    auto list = r.focusable();
    CHECK(list.size() == 2);
    CHECK(r.focused() == list[1]);  // "c" took b's place
}

TEST(repaint_writes_only_changed_lines) {
    std::string out;
    TerminalRenderer r([&out](std::string_view s) { out += s; });
    r.setSize(30, 5);
    Root root(r);
    root.render(h(Form));
    CHECK(contains(out, "\x1b[2J"));  // first frame clears the screen

    out.clear();
    r.handleKey(root, key("x"));
    CHECK(!contains(out, "\x1b[2J"));
    CHECK(contains(out, "\x1b[1;1H"));  // input line
    CHECK(contains(out, "\x1b[3;1H"));  // "name=x" line
    CHECK(!contains(out, "\x1b[2;1H"));  // button did not change
}

TEST(focused_node_is_reverse_video) {
    std::string out;
    TerminalRenderer r([&out](std::string_view s) { out += s; });
    Root root(r);
    root.render(h("box", {}, {h("button", {}, {"ok"})}));
    CHECK(contains(out, ";7m[ ok ]"));
}

// ---- flexbox (Yoga) ----

std::vector<std::string> renderLines(int w, int hgt, Element e) {
    TerminalRenderer r;
    r.setSize(w, hgt);
    Root root(r);
    root.render(std::move(e));
    return r.frameLines();
}

TEST(flex_grow_takes_free_space) {
    auto lines = renderLines(20, 3, h("box", {{"direction", "row"}}, {
        h("text", {}, {"a"}),
        h("box", {{"flexGrow", 1}}),
        h("text", {}, {"b"}),
    }));
    CHECK(lines.at(0) == "a                  b");
}

TEST(flex_grow_splits_by_ratio) {
    // Like CSS flex: 1 / flex: 2 -> 4 and 8 columns.
    auto lines = renderLines(12, 3, h("box", {{"direction", "row"}}, {
        h("box", {{"flexGrow", 1}, {"flexBasis", 0}}, {h("text", {}, {"a"})}),
        h("box", {{"flexGrow", 2}, {"flexBasis", 0}}, {h("text", {}, {"b"})}),
    }));
    CHECK(lines.at(0) == "a   b");
}

TEST(free_space_is_shared_after_the_border) {
    // Each empty bordered box needs 2 columns; the other 8 are split 1:2.
    auto lines = renderLines(12, 3, h("box", {{"direction", "row"}}, {
        h("box", {{"flexGrow", 1}, {"border", true}}),
        h("box", {{"flexGrow", 2}, {"border", true}}),
    }));
    CHECK(lines.at(0) == "┌───┐┌─────┐");
}

TEST(justify_content_in_a_row) {
    auto center = renderLines(10, 2, h("box", {{"direction", "row"}, {"justifyContent", "center"}}, {h("text", {}, {"hi"})}));
    CHECK(center.at(0) == "    hi");
    auto end = renderLines(10, 2, h("box", {{"direction", "row"}, {"justifyContent", "end"}}, {h("text", {}, {"hi"})}));
    CHECK(end.at(0) == "        hi");
    auto between = renderLines(7, 2, h("box", {{"direction", "row"}, {"justifyContent", "space-between"}},
                                       {h("text", {}, {"a"}), h("text", {}, {"b"}), h("text", {}, {"c"})}));
    CHECK(between.at(0) == "a  b  c");
}

TEST(align_items_in_a_column) {
    auto center = renderLines(10, 2, h("box", {{"alignItems", "center"}}, {h("text", {}, {"hi"})}));
    CHECK(center.at(0) == "    hi");
    auto end = renderLines(10, 2, h("box", {{"alignItems", "end"}}, {h("text", {}, {"hi"})}));
    CHECK(end.at(0) == "        hi");
}

TEST(centers_in_the_whole_screen) {
    auto lines = renderLines(10, 5, h("box", {{"flexGrow", 1}, {"justifyContent", "center"}, {"alignItems", "center"}},
                                      {h("text", {}, {"hi"})}));
    CHECK(lines.size() == 3);
    CHECK(lines[2] == "    hi");
}

TEST(centering_with_half_cell_does_not_overlap) {
    // 7 rows, 4 rows of content: Yoga puts it at 1.5. Text and boxes must
    // round the same way, or the text lands on the border.
    auto lines = renderLines(10, 7, h("box", {{"flexGrow", 1}, {"justifyContent", "center"}}, {
        h("box", {{"border", true}, {"width", 4}, {"height", 3}}),
        h("text", {}, {"t"}),
    }));
    CHECK(lines.size() == 6);
    CHECK(lines[2] == "┌──┐");
    CHECK(lines[4] == "└──┘");
    CHECK(lines[5] == "t");
}

TEST(buttons_do_not_shrink_in_a_row) {
    auto lines = renderLines(12, 3, h("box", {{"direction", "row"}, {"flexWrap", "wrap"}}, {
        h("button", {}, {"one"}), h("button", {}, {"two"}),
    }));
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "[ one ]");
    CHECK(lines[1] == "[ two ]");
}

TEST(percent_width) {
    auto lines = renderLines(20, 3, h("box", {{"width", "50%"}, {"border", true}}));
    CHECK(lines.at(0) == "┌────────┐");
}

TEST(text_wraps_at_word_boundaries) {
    auto lines = renderLines(10, 5, h("text", {}, {"hello big world"}));
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "hello big");
    CHECK(lines[1] == "world");
}

TEST(text_shrinks_and_wraps_inside_a_row) {
    auto lines = renderLines(12, 5, h("box", {{"direction", "row"}}, {
        h("text", {}, {"Name:"}),
        h("text", {{"flexShrink", 1}}, {"one two three"}),
    }));
    CHECK(lines.at(0) == "Name:one two");
    CHECK(lines.at(1) == "     three");
}

TEST(absolute_position) {
    auto lines = renderLines(10, 4, h("box", {}, {
        h("text", {}, {"base"}),
        h("box", {{"position", "absolute"}, {"top", 2}, {"left", 3}}, {h("text", {}, {"z"})}),
    }));
    CHECK(lines.size() == 3);
    CHECK(lines[0] == "base");
    CHECK(lines[2] == "   z");
}

TEST(display_none_hides_a_node) {
    auto lines = renderLines(10, 3, h("box", {}, {
        h("text", {{"display", "none"}}, {"hidden"}),
        h("text", {}, {"shown"}),
    }));
    CHECK(lines.size() == 1);
    CHECK(lines[0] == "shown");
}

TEST(padding_x_and_margin) {
    auto lines = renderLines(10, 3, h("box", {{"paddingX", 2}, {"marginTop", 1}}, {h("text", {}, {"p"})}));
    CHECK(lines.size() == 2);
    CHECK(lines[1] == "  p");
}

TEST(overflow_hidden_clips_children) {
    auto lines = renderLines(10, 5, h("box", {{"height", 1}, {"overflow", "hidden"}}, {
        h("text", {}, {"one"}), h("text", {}, {"two"}),
    }));
    CHECK(lines.size() == 1);
    CHECK(lines[0] == "one");
}

TEST(children_are_clipped_by_the_border) {
    auto lines = renderLines(10, 6, h("box", {{"border", true}, {"height", 3}, {"width", 6}}, {
        h("text", {}, {"one"}), h("text", {}, {"two"}), h("text", {}, {"three"}),
    }));
    CHECK(lines.size() == 3);
    CHECK(lines[0] == "┌────┐");
    CHECK(lines[1] == "│one │");
    CHECK(lines[2] == "└────┘");
}

// ---- mouse ----

TEST(decodes_sgr_mouse_reports) {
    auto in = decodeInput("a\x1b[<0;5;3M\x1b[<32;9;4M\x1b[<0;9;4m\x1b[<35;2;2M\x1b[<64;1;1M\x1b[<18;1;1M");
    CHECK(in.size() == 7);
    CHECK(std::get<KeyEvent>(in[0]).key == "a");
    auto m = [&](std::size_t i) { return std::get<MouseInput>(in[i]); };
    CHECK((m(1) == MouseInput{.action = MouseInput::Action::Down, .x = 4, .y = 2}));
    CHECK((m(2) == MouseInput{.action = MouseInput::Action::Move, .x = 8, .y = 3, .held = true}));
    CHECK((m(3) == MouseInput{.action = MouseInput::Action::Up, .x = 8, .y = 3}));
    CHECK((m(4) == MouseInput{.action = MouseInput::Action::Move, .x = 1, .y = 1}));
    CHECK(m(5).action == MouseInput::Action::WheelUp);
    CHECK(m(6).action == MouseInput::Action::Down && m(6).button == 2 && m(6).ctrl);
    CHECK(decodeKeys("\x1b[<0;5;3Mb") == std::vector<KeyEvent>{key("b")});  // keys alone skip the mouse
}

// Where `text` is on screen (ASCII), to aim the mouse at it.
std::pair<int, int> where(const TerminalRenderer& r, const std::string& text) {
    auto lines = r.frameLines();
    for (std::size_t y = 0; y < lines.size(); ++y)
        if (auto x = lines[y].find(text); x != std::string::npos) return {static_cast<int>(x), static_cast<int>(y)};
    return {-1, -1};
}

MouseInput mouse(MouseInput::Action action, std::pair<int, int> at, bool held = false) {
    return MouseInput{.action = action, .x = at.first, .y = at.second, .held = held};
}

TEST(mouse_click_presses_and_focuses_a_button) {
    TerminalRenderer r;
    r.setSize(30, 6);
    Root root(r);
    root.render(h(Form));
    auto ok = where(r, "[ ok ]");
    CHECK(r.handleMouse(root, mouse(MouseInput::Action::Down, ok)));
    CHECK(r.focused() == r.nodeAt(ok.first, ok.second));
    CHECK(r.handleMouse(root, mouse(MouseInput::Action::Up, ok)));
    CHECK(contains(r.frameText(), "clicks=1"));
    // Press on the button, release elsewhere: no click.
    r.handleMouse(root, mouse(MouseInput::Action::Down, ok));
    r.handleMouse(root, mouse(MouseInput::Action::Up, {29, 5}));
    CHECK(contains(r.frameText(), "clicks=1"));
}

Element Slider(const Props&) {
    auto [pos, setPos] = useState(0);
    auto [grab, setGrab] = useState(-1);  // where in the knob it was taken
    return h("box", {}, {
        h("box", {{"direction", "row"}, {"height", 1}}, {
            h("box", {{"width", pos}}),
            h("box", {{"width", 3}, {"height", 1}, {"pointerCapture", true},
                      {"onPointerDown", [setGrab](const PointerEvent& e) { setGrab(static_cast<int>(e.localX)); }},
                      {"onPointerMove", [grab, setPos](const PointerEvent& e) {
                           if (grab >= 0 && e.buttons) setPos(std::max(0, static_cast<int>(e.x) - grab));
                       }},
                      {"onPointerUp", [setGrab](const PointerEvent&) { setGrab(-1); }}},
              {h("text", {}, {"[#]"})}),
        }),
        h("text", {}, {"pos=" + std::to_string(pos) + (grab >= 0 ? " dragging" : "")}),
    });
}

TEST(mouse_drag_keeps_the_pointer_outside_the_node) {
    TerminalRenderer r;
    r.setSize(30, 6);
    Root root(r);
    root.render(h(Slider));
    auto knob = where(r, "[#]");
    CHECK((knob == std::pair(0, 0)));
    r.handleMouse(root, mouse(MouseInput::Action::Down, {1, 0}));  // the middle of the knob
    CHECK(contains(r.frameText(), "pos=0 dragging"));
    r.handleMouse(root, mouse(MouseInput::Action::Move, {11, 4}, true));  // far below: still captured
    CHECK(contains(r.frameText(), "pos=10 dragging"));
    CHECK((where(r, "[#]") == std::pair(10, 0)));
    r.handleMouse(root, mouse(MouseInput::Action::Up, {11, 4}));
    CHECK(contains(r.frameText(), "pos=10") && !contains(r.frameText(), "dragging"));
    r.handleMouse(root, mouse(MouseInput::Action::Move, {20, 0}));  // no button: nothing moves
    CHECK(contains(r.frameText(), "pos=10"));
}

// Without pointerCapture the release goes to what is under the pointer: a drop target.
Element DropZone(const Props&) {
    auto [dragging, setDragging] = useState(false);
    auto [dropped, setDropped] = useState(std::string("-"));
    return h("box", {{"direction", "row"}, {"height", 3}}, {
        h("box", {{"width", 6}, {"onPointerDown", [setDragging](const PointerEvent&) { setDragging(true); }}},
          {h("text", {}, {"card"})}),
        h("box", {{"width", 16}, {"onPointerUp", [dragging, setDragging, setDropped](const PointerEvent& e) {
                       if (dragging) setDropped("done@" + std::to_string(static_cast<int>(e.localX)));
                       setDragging(false);
                   }}},
          {h("text", {}, {"column " + dropped})}),
    });
}

TEST(mouse_drop_goes_to_the_node_under_the_pointer) {
    TerminalRenderer r;
    r.setSize(30, 4);
    Root root(r);
    root.render(h(DropZone));
    r.handleMouse(root, mouse(MouseInput::Action::Down, where(r, "card")));
    r.handleMouse(root, mouse(MouseInput::Action::Move, {9, 1}, true));
    r.handleMouse(root, mouse(MouseInput::Action::Up, {9, 1}));
    CHECK(contains(r.frameText(), "column done@3"));  // localX inside the column (it starts at 6)
}

Element HoverPair(const Props&) {
    auto [hovered, setHovered] = useState(std::string{});
    auto [events, setEvents] = useState(0);
    auto zone = [&](std::string name) {
        return h("box", {{"key", name}, {"width", 5}, {"height", 1},
                         {"onMouseEnter", [setHovered, setEvents, name] { setHovered(name); setEvents([](int n) { return n + 1; }); }},
                         {"onMouseLeave", [setHovered, setEvents] { setHovered(std::string{}); setEvents([](int n) { return n + 1; }); }}},
                 {h("text", {}, {name})});
    };
    return h("box", {}, {
        h("box", {{"direction", "row"}, {"height", 1}}, {zone("aaaaa"), zone("bbbbb")}),
        h("text", {}, {"hover=" + hovered + " events=" + std::to_string(events)}),
    });
}

TEST(mouse_enter_and_leave_follow_the_pointer) {
    TerminalRenderer r;
    r.setSize(30, 4);
    Root root(r);
    root.render(h(HoverPair));
    r.handleMouse(root, mouse(MouseInput::Action::Move, {1, 0}));
    CHECK(contains(r.frameText(), "hover=aaaaa events=1"));
    r.handleMouse(root, mouse(MouseInput::Action::Move, {2, 0}));  // same node: nothing new
    CHECK(contains(r.frameText(), "events=1"));
    r.handleMouse(root, mouse(MouseInput::Action::Move, {6, 0}));  // leave a, enter b
    CHECK(contains(r.frameText(), "hover=bbbbb events=3"));
    r.handleMouse(root, mouse(MouseInput::Action::Move, {20, 3}));
    CHECK(contains(r.frameText(), "hover= events=4"));
}

NodeHandle focusedByRef = 0;

Element FocusOnOpen(const Props&) {
    auto second = useNodeRef();
    useEffect([second] {
        if (second.focus()) focusedByRef = second.get();
    }, deps());
    return h("box", {}, {
        h("input", {{"value", "first"}}),
        h("input", {{"value", "second"}, {"ref", second}}),
    });
}

TEST(node_ref_focus_moves_the_terminal_focus) {
    TerminalRenderer r;
    r.setSize(30, 4);
    Root root(r);
    root.render(h(FocusOnOpen));
    CHECK(focusedByRef != 0);
    CHECK(r.focused() == focusedByRef);
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
