// Browser events and elements added in 0.5: useKeyboard (app-wide keys),
// onFocus / onBlur, onScroll and SVG. WebAssembly under Node with
// tests/web/fake_dom.js.
#include <emscripten.h>

#include <iostream>
#include <string>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"

using namespace rx;

namespace {

int failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

std::string js(const std::string& code) { return emscripten_run_script_string(code.c_str()); }
std::string log() { return js("document.querySelector('#log').textContent"); }
void clearLog() { js("rxTest.click('clear')"); }

Element App(const Props&) {
    auto [events, setEvents] = useState(std::string{});
    auto add = [setEvents](const std::string& what) {
        setEvents([what](const std::string& all) { return all + what + " "; });
    };
    useKeyboard([add](const KeyEvent& k) {
        if (k.ctrl && k.key == "z") {
            add("undo");
            return true;
        }
        if (k.key == "/") {
            add("search");
            return true;
        }
        return false;
    });
    return h("div", {}, {
        h("input", {{"id", "field"}, {"value", ""},
                    {"onFocus", [add] { add("focus"); }},
                    {"onBlur", [add] { add("blur"); }},
                    {"onKey", [add](const KeyEvent& k) { add("field:" + k.key); }}}),
        h("div", {{"id", "list"},
                  {"onScroll", [add](const ScrollEvent& e) {
                       add("scroll:" + std::to_string(e.top) + (e.nearBottom() ? ":end" : ""));
                   }}},
          {h("p", {{"id", "row"}}, {"row"})}),
        h("svg", {{"id", "chart"}, {"viewBox", "0 0 10 10"}}, {
            h("rect", {{"id", "bar"}, {"width", "5"}, {"height", "10"}}),
            h("svg:text", {{"id", "label"}, {"x", "1"}}, {"5"}),
        }),
        h("button", {{"id", "clear"}, {"onClick", [setEvents] { setEvents(std::string{}); }}}, {"clear"}),
        h("p", {{"id", "log"}}, {events}),
    });
}

}  // namespace

int main() {
    mountDom(h(App));
    const std::string svg = "http://www.w3.org/2000/svg";

    std::cout << "useKeyboard: app-wide shortcuts come first\n";
    CHECK(js("rxTest.pressKey('field', 'z', true)") == "consumed");  // Ctrl+Z even inside the field
    CHECK(log() == "undo ");                                         // the field's onKey didn't run
    clearLog();
    CHECK(js("rxTest.pressKey('field', '/')") == "passed");  // typing "/" in a field is text
    CHECK(log() == "field:/ ");
    clearLog();
    CHECK(js("rxTest.pressKey('', '/')") == "consumed");  // nothing focused: the shortcut works
    CHECK(log() == "search ");
    clearLog();
    CHECK(js("rxTest.pressKey('field', 'a')") == "passed");
    CHECK(log() == "field:a ");
    clearLog();

    std::cout << "onFocus / onBlur\n";
    js("rxTest.focusIn('field')");
    js("rxTest.focusOut('field')");
    CHECK(log() == "focus blur ");
    clearLog();

    std::cout << "onScroll gets the position\n";
    js("rxTest.scroll('list', 300, 2000, 500)");
    js("rxTest.scroll('list', 1450, 2000, 500)");
    CHECK(log() == "scroll:300 scroll:1450:end ");
    js("rxTest.scroll('row', 10, 100, 50)");  // the <p> has no onScroll and scroll doesn't bubble
    CHECK(log() == "scroll:300 scroll:1450:end ");
    clearLog();

    std::cout << "SVG elements get the SVG namespace\n";
    CHECK(js("rxTest.ns('chart')") == svg);
    CHECK(js("rxTest.ns('bar')") == svg);
    CHECK(js("rxTest.ns('label')") == svg);  // "svg:text" -> <text> in SVG
    CHECK(js("rxTest.tag('label')") == "TEXT");
    CHECK(js("rxTest.ns('field')") == "http://www.w3.org/1999/xhtml");

    std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("OK, all tests passed")) << '\n';
    return failures ? 1 : 0;
}
