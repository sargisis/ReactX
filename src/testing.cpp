// rx::testing (reactx/testing.hpp): a component on an in-memory terminal, and
// a small test runner. main() is in testing_main.cpp of the same library, so
// it is linked only when the tests don't bring their own.
#include "reactx/testing.hpp"

#include <exception>
#include <iostream>
#include <thread>
#include <utility>

#include "utf8.hpp"

namespace rx::testing {

Screen::Screen(Element app, int width, int height)
    : renderer_(std::make_unique<TerminalRenderer>()), root_(std::make_unique<Root>(*renderer_)) {
    renderer_->setSize(width, height);
    detail::notifyResize({width, height});  // useWindowSize sees this screen
    root_->render(std::move(app));
}

// The tree goes first: effect cleanups may still use the renderer.
Screen::~Screen() { root_.reset(); }

std::string Screen::text() const { return renderer_->frameText(); }
std::vector<std::string> Screen::lines() const { return renderer_->frameLines(); }
bool Screen::shows(std::string_view part) const { return text().find(part) != std::string::npos; }

void Screen::press(std::string_view key) {
    KeyEvent e;
    // "Ctrl+s", "Shift+Tab": modifiers first; a lone "+" is the key itself.
    while (key.size() > 1) {
        if (key.starts_with("Ctrl+")) e.ctrl = true, key.remove_prefix(5);
        else if (key.starts_with("Alt+")) e.alt = true, key.remove_prefix(4);
        else if (key.starts_with("Shift+")) e.shift = true, key.remove_prefix(6);
        else break;
    }
    e.key = std::string(key);
    // As TerminalApp does: shortcuts of the whole app (useKeyboard) first.
    if (!detail::dispatchGlobalKey(e, renderer_->editingText())) renderer_->handleKey(*root_, e);
    root_->flush();
}

void Screen::type(std::string_view text) {
    for (const detail::Glyph& g : detail::utf8Glyphs(text)) {
        if (g.text == "\n") press("Enter");
        else press(g.text);
    }
}

bool Screen::click(std::string_view label) {
    auto rows = lines();
    for (std::size_t y = 0; y < rows.size(); ++y) {
        auto at = rows[y].find(label);
        if (at == std::string::npos) continue;
        // The middle of the label, in terminal columns (wide characters take two).
        int x = detail::textCells(std::string_view(rows[y]).substr(0, at)) + detail::textCells(label) / 2;
        int row = static_cast<int>(y);
        renderer_->handleMouse(*root_, MouseInput{.action = MouseInput::Action::Down, .x = x, .y = row});
        renderer_->handleMouse(*root_, MouseInput{.action = MouseInput::Action::Up, .x = x, .y = row});
        root_->flush();
        return true;
    }
    return false;
}

void Screen::settle(std::chrono::milliseconds wait) {
    if (wait.count() > 0) std::this_thread::sleep_for(wait);
    root_->flush();  // runs the timers that are due, then re-renders what changed
}

void Screen::render(Element app) { root_->render(std::move(app)); }

void Screen::resize(int width, int height) {
    renderer_->setSize(width, height);
    detail::notifyResize({width, height});  // useWindowSize; re-rendered by the flush
    root_->flush();
}

// ---- runner ----

namespace {
struct Test {
    const char* name;
    void (*run)();
};
std::vector<Test>& tests() {
    static std::vector<Test> all;
    return all;
}
int failedChecks = 0;
}  // namespace

void registerTest(const char* name, void (*test)()) { tests().push_back({name, test}); }

void recordCheck(bool ok, const char* expression, const char* file, int line) {
    if (ok) return;
    ++failedChecks;
    std::cout << "  FAIL " << file << ':' << line << ": " << expression << '\n';
}

int runTests(int argc, char** argv) {
    std::string_view filter = argc > 1 ? argv[1] : "";
    int ran = 0, failed = 0;
    for (const Test& t : tests()) {
        if (!filter.empty() && std::string_view(t.name).find(filter) == std::string_view::npos) continue;
        ++ran;
        int before = failedChecks;
        std::cout << t.name << '\n';
        try {
            t.run();
        } catch (const std::exception& e) {
            ++failedChecks;
            std::cout << "  FAIL threw: " << e.what() << '\n';
        } catch (...) {
            ++failedChecks;
            std::cout << "  FAIL threw something that isn't a std::exception\n";
        }
        if (failedChecks != before) ++failed;
    }
    if (ran == 0) {
        std::cout << (filter.empty() ? std::string("no tests (write one with RX_TEST)")
                                     : "no test name contains \"" + std::string(filter) + "\"")
                  << '\n';
        return filter.empty() ? 0 : 1;
    }
    if (failed == 0) std::cout << "OK, " << ran << (ran == 1 ? " test passed\n" : " tests passed\n");
    else std::cout << "FAILED: " << failed << " of " << ran << (ran == 1 ? " test\n" : " tests\n");
    return failed == 0 ? 0 : 1;
}

}  // namespace rx::testing
