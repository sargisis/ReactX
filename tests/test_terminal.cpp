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
