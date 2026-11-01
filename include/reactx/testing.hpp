#pragma once

// reactx/testing.hpp — tests for your components, run natively by
// `reactx test` (every tests/*.cpp of the project, with the app's sources
// except main.cpp). A component is drawn on an in-memory terminal; the test
// presses keys, types and clicks like a user and reads the screen:
//
//   #include "reactx/testing.hpp"
//   #include "../src/app.hpp"
//
//   RX_TEST(counter_counts) {
//       rx::testing::Screen s(h(Counter));
//       CHECK(s.shows("Clicked 0 times"));
//       s.click("+1");
//       CHECK(s.shows("Clicked 1 times"));
//   }
//
// No main() needed: reactx::testing brings one (`./tests run` filters by name:
// `reactx test counter`). In a web project ui:: components draw their terminal
// form here; what a test checks is the text and the behaviour.

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"

namespace rx::testing {

class Screen {
public:
    explicit Screen(Element app, int width = 80, int height = 24);
    ~Screen();
    Screen(const Screen&) = delete("a Screen owns the mounted app");
    Screen& operator=(const Screen&) = delete("a Screen owns the mounted app");

    // What the screen shows: one line per row, without trailing spaces.
    std::string text() const;
    std::vector<std::string> lines() const;
    bool shows(std::string_view part) const;

    // A key, as the user presses it, through useKeyboard and the focused
    // element: "Enter", "Tab", "Escape", "Up", "Backspace", "a", "Ctrl+s",
    // "Shift+Tab", "Alt+x".
    void press(std::string_view key);
    // Text into the focused field, one character at a time.
    void type(std::string_view text);
    // A mouse click on the first place that shows `label` (a button's label, a
    // list item); false if the screen doesn't show it.
    bool click(std::string_view label);

    // Runs timers that are due (useTimeout, useDebounce, useInterval; after
    // waiting `wait` first) and setters called outside events, then draws.
    void settle(std::chrono::milliseconds wait = std::chrono::milliseconds(0));
    // Draws another element in the same place (new props): like a re-render from above.
    void render(Element app);
    void resize(int width, int height);

    Root& root() { return *root_; }
    TerminalRenderer& renderer() { return *renderer_; }

private:
    std::unique_ptr<TerminalRenderer> renderer_;
    std::unique_ptr<Root> root_;
};

// ---- a small test runner ----

void registerTest(const char* name, void (*test)());
void recordCheck(bool ok, const char* expression, const char* file, int line);
// Runs the tests whose name contains argv[1] (all without it); prints each
// failed check and a summary. Returns the exit code: 0 if everything passed.
int runTests(int argc, char** argv);

}  // namespace rx::testing

#define RX_TEST(name)                                                                          \
    static void rx_test_##name();                                                              \
    [[maybe_unused]] static const bool rx_test_registered_##name =                             \
        (::rx::testing::registerTest(#name, &rx_test_##name), true);                           \
    static void rx_test_##name()

// A failed check prints the expression and its place and the test goes on.
#define RX_CHECK(...) ::rx::testing::recordCheck(static_cast<bool>(__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)
#ifndef CHECK
#define CHECK(...) RX_CHECK(__VA_ARGS__)
#endif
