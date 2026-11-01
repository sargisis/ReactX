// Browser events and elements added in 0.5: useKeyboard (app-wide keys),
// onFocus / onBlur, onScroll and SVG. WebAssembly under Node with
// tests/web/fake_dom.js.
#include <emscripten.h>

#include <iostream>
#include <string>

#include "reactx/dom.hpp"
#include "reactx/websocket.hpp"
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
NodeRef listRef;  // the #list element, for scrollMetrics / scrollTo
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
    auto list = useNodeRef();
    listRef = list;
    return h("div", {}, {
        h("input", {{"id", "field"}, {"value", ""},
                    {"onFocus", [add] { add("focus"); }},
                    {"onBlur", [add] { add("blur"); }},
                    {"onKey", [add](const KeyEvent& k) { add("field:" + k.key); }}}),
        h("div", {{"id", "list"}, {"ref", list},
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

// useWebSocket in the browser: the page's WebSocket (faked in fake_dom.js).
Element Live(const Props&) {
    auto [messages, setMessages] = useState(std::string{});
    auto socket = useWebSocket("/ws/chat", {
        .onMessage = [setMessages](const std::string& m) { setMessages([m](const std::string& all) { return all + "[" + m + "]"; }); },
        .reconnect = false,
    });
    auto send = socket.send;
    auto again = socket.reconnect;
    return h("div", {}, {
        h("p", {{"id", "status"}}, {socket.open() ? "open" : socket.status == WebSocketStatus::Connecting ? "connecting" : "closed:" + socket.error}),
        h("p", {{"id", "messages"}}, {messages}),
        h("button", {{"id", "wsend"}, {"onClick", [send] { send("ping"); }}}, {"send"}),
        h("button", {{"id", "again"}, {"onClick", [again] { again(); }}}, {"again"}),
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

    std::cout << "ref.scrollMetrics / scrollTo / scrollBy\n";
    auto m = listRef.scrollMetrics();
    CHECK(m && m->top == 1450 && m->scrollHeight == 2000 && m->clientHeight == 500);
    CHECK(listRef.scrollTo(100) && js("String(document.getElementById('list').scrollTop)") == "100");
    CHECK(listRef.scrollBy(40) && js("String(document.getElementById('list').scrollTop)") == "140");
    CHECK(!NodeRef{}.scrollMetrics() && !NodeRef{}.scrollTo(0));  // an empty ref

    std::cout << "SVG elements get the SVG namespace\n";
    CHECK(js("rxTest.ns('chart')") == svg);
    CHECK(js("rxTest.ns('bar')") == svg);
    CHECK(js("rxTest.ns('label')") == svg);  // "svg:text" -> <text> in SVG
    CHECK(js("rxTest.tag('label')") == "TEXT");
    CHECK(js("rxTest.ns('field')") == "http://www.w3.org/1999/xhtml");

    std::cout << "useWebSocket: the page's WebSocket\n";
    Root& live = mountDom(h(Live));  // replaces the app above
    auto text = [](const char* id) { return js(std::string("document.getElementById('") + id + "').textContent"); };
    CHECK(js("rxTest.ws.count()") == "1");
    CHECK(js("rxTest.ws.url(0)") == "ws://localhost/ws/chat");  // relative: on the page's host, ws://
    CHECK(text("status") == "connecting");
    js("rxTest.ws.open(0)");
    CHECK(text("status") == "open");
    js("rxTest.ws.message(0, 'hi')");
    js("rxTest.ws.message(0, 'there')");
    CHECK(text("messages") == "[hi][there]");
    js("rxTest.click('wsend')");
    CHECK(js("rxTest.ws.sent(0)") == "ping");
    js("rxTest.ws.drop(0, 1006)");
    CHECK(text("status") == "closed:closed (1006)");
    js("rxTest.click('wsend')");
    CHECK(js("rxTest.ws.sent(0)") == "ping");  // not sent while closed
    js("rxTest.click('again')");               // reconnect by hand
    CHECK(js("rxTest.ws.count()") == "2" && text("status") == "connecting");
    live.render(h("p", {}, {"gone"}));         // unmount: the socket is closed
    CHECK(js("rxTest.ws.closedWith(1)") == "1000");

    std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("OK, all tests passed")) << '\n';
    return failures ? 1 : 0;
}
