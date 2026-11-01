// reactx/testing.hpp tested with itself: Screen drives components like a user,
// RX_TEST / CHECK and the main() of reactx::testing run them.
#include <string>

#include "reactx/testing.hpp"
#include "reactx/ui.hpp"

using namespace rx;

namespace {

Element Counter(const Props&) {
    auto [count, setCount] = useState(0);
    return h("box", {}, {
        h("text", {}, {"Clicked " + std::to_string(count) + " times"}),
        ui::button({.label = "+1", .onClick = [setCount] { setCount([](int c) { return c + 1; }); }}),
    });
}

Element Greeter(const Props&) {
    auto [name, setName] = useState(std::string{});
    auto [saved, setSaved] = useState(std::string{});
    useKeyboard([setSaved, name](const KeyEvent& k) {
        if (k.ctrl && k.key == "s") {
            setSaved(name);
            return true;
        }
        return false;
    });
    return h("box", {}, {
        ui::input({.value = name, .onChange = [setName](const std::string& t) { setName(t); }, .label = "Name"}),
        h("text", {}, {"saved=" + saved}),
    });
}

Element Later(const Props&) {
    auto [done, setDone] = useState(false);
    useTimeout([setDone] { setDone(true); }, std::chrono::milliseconds(20));
    return h("text", {}, {done ? "ready" : "waiting"});
}

}  // namespace

RX_TEST(click_finds_a_label_and_presses_it) {
    testing::Screen s(h(Counter));
    CHECK(s.shows("Clicked 0 times"));
    CHECK(s.click("+1"));
    CHECK(s.click("+1"));
    CHECK(s.shows("Clicked 2 times"));
    CHECK(!s.click("missing"));
}

RX_TEST(press_and_type_go_through_shortcuts_and_the_focus) {
    testing::Screen s(h(Greeter));
    s.type("Аня 👋");
    s.press("Ctrl+s");
    CHECK(s.shows("saved=Аня 👋"));
    s.press("Backspace");
    s.press("Backspace");
    CHECK(s.shows("Name Аня") && !s.shows("Name Аня 👋"));
    CHECK(s.lines().size() >= 2);
}

RX_TEST(settle_runs_timers_that_are_due) {
    testing::Screen s(h(Later));
    CHECK(s.shows("waiting"));
    s.settle(std::chrono::milliseconds(40));
    CHECK(s.shows("ready"));
}

RX_TEST(render_and_resize_redraw) {
    testing::Screen s(h("text", {}, {"one"}), 20, 4);
    s.render(h("text", {}, {"two"}));
    CHECK(s.shows("two") && !s.shows("one"));
    s.resize(3, 4);
    CHECK(s.lines().at(0) == "two");
}
