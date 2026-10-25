// Tests for rx::ui components on the terminal backend (no real terminal needed).
#include <functional>
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
