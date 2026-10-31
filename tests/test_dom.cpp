// DOM backend tests: WebAssembly under Node with tests/web/fake_dom.js.
#include <emscripten.h>

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "reactx/dom.hpp"
#include "reactx/js.hpp"
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
std::string html() { return js("rxTest.html()"); }
bool has(const std::string& part) { return html().find(part) != std::string::npos; }

// ---- app ----

Element Item(const Props& p) {
    return h("li", {{"key", p.get<std::string>("name")}, {"id", "item-" + p.get<std::string>("name")}},
             {p.get<std::string>("name")});
}

Element App(const Props&) {
    auto [count, setCount] = useState(0);
    auto [text, setText] = useState(std::string{});
    auto [agree, setAgree] = useState(false);
    auto [lastKey, setLastKey] = useState(std::string{});
    auto [submits, setSubmits] = useState(0);
    auto [order, setOrder] = useState(std::string("abc"));
    auto [upper, setUpper] = useState(false);

    std::vector<Element> items;
    for (char c : order) items.push_back(h(Item, {{"key", std::string(1, c)}, {"name", std::string(1, c)}}));

    return h("div", {{"className", "card"}}, {
        h("button", {{"id", "inc"}, {"className", count ? "primary" : ""},
                     {"onClick", [setCount] { setCount([](int c) { return c + 1; }); }}}, {"count=" + std::to_string(count)}),
        // Only the outer div has onClick: the click on the span bubbles to it.
        h("div", {{"id", "outer"}, {"onClick", [setCount] { setCount([](int c) { return c + 10; }); }}}, {
            h("span", {{"id", "inner"}}, {"inner"}),
            // Both run, like React: +100 here, then +10 on the outer div.
            h("button", {{"id", "both"}, {"onClick", [setCount] { setCount([](int c) { return c + 100; }); }}}, {"both"}),
            // data-rx-self: only clicks on this element itself.
            h("div", {{"id", "self"}, {"data-rx-self", true}, {"onClick", [setCount] { setCount([](int c) { return c + 1000; }); }}}, {
                h("span", {{"id", "selfchild"}}, {"child"}),
            }),
        }),
        h("input", {{"type", "text"}, {"id", "name"}, {"value", text},
                    {"onInput", [setText, upper](const std::string& t) {
                         std::string v = t;
                         if (upper) for (auto& ch : v) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                         setText(v);
                     }},
                    {"onKey", [setLastKey](const KeyEvent& e) { setLastKey(e.key + (e.shift ? "+shift" : "")); }}}),
        h("input", {{"type", "checkbox"}, {"id", "agree"}, {"checked", agree},
                    {"onChange", [setAgree, agree] { setAgree(!agree); }}}),
        h("button", {{"id", "upper"}, {"onClick", [setUpper] { setUpper(true); }}}, {"upper"}),
        // Controlled and never changed by its handler: clicks are undone.
        h("input", {{"type", "checkbox"}, {"id", "locked"}, {"checked", true}, {"onChange", [] {}}}),
        h("form", {{"id", "form"}, {"onSubmit", [setSubmits] { setSubmits([](int s) { return s + 1; }); }}}, {
            h("button", {{"type", "submit"}}, {"go"}),
        }),
        h("p", {{"id", "out"}, {"style", agree ? "color: green" : ""}, {"disabled", agree}},
          {"text=" + text + " agree=" + (agree ? "yes" : "no") + " key=" + lastKey + " submits=" + std::to_string(submits)}),
        h("button", {{"id", "reverse"}, {"onClick", [setOrder] { setOrder(std::string("cba")); }}}, {"reverse"}),
        h("button", {{"id", "drop"}, {"onClick", [setOrder] { setOrder(std::string("ac")); }}}, {"drop"}),
        h("ul", {{"id", "list"}}, std::move(items)),
        // Links built from user input: a javascript: URL must never reach the page.
        h("a", {{"id", "evil"}, {"href", " \x01Java\tScript:alert(1)"}}, {"evil"}),
        h("a", {{"id", "fine"}, {"href", "/javascript:notes"}}, {"fine"}),
        h("img", {{"id", "pic"}, {"src", "javascript:alert(2)"}}),
    });
}

// useNodeRef + domElement: the C++ side reaches the real element.
std::string refTag;

Element RefProbe(const Props&) {
    auto box = useNodeRef();
    useEffect([box] {
        emscripten::val el = domElement(box);
        refTag = el.isUndefined() ? "undefined" : el["tagName"].as<std::string>() + "#" + el.call<std::string>("getAttribute", std::string("id"));
        el.call<void>("setAttribute", std::string("data-seen"), std::string("yes"));
    }, deps());
    return h("section", {{"id", "probe"}, {"ref", box}});
}

}  // namespace

int main() {
    js("document.querySelector('#app').textContent = 'loading'");  // a loading text or prerendered HTML
    mountDom(h(App));

    std::cout << "mount replaces what the container showed\n";
    CHECK(!has("loading"));
    CHECK(html().starts_with("<div class=\"card\">"));
    CHECK(has("<div class=\"card\"><button class id=\"inc\">count=0</button>"));

    std::cout << "click updates text and attributes\n";
    js("rxTest.click('inc')");
    js("rxTest.click('inc')");
    CHECK(has("<button class=\"primary\" id=\"inc\">count=2</button>"));

    std::cout << "click bubbles to the nearest handler\n";
    js("rxTest.click('inner')");
    CHECK(has("count=12"));
    js("rxTest.click('both')");
    CHECK(has("count=122"));  // the button's handler and the outer div's
    js("rxTest.click('selfchild')");
    CHECK(has("count=132"));  // the data-rx-self div skipped, the outer div ran
    js("rxTest.click('self')");
    CHECK(has("count=1142"));

    std::cout << "input is controlled\n";
    js("rxTest.type('name', 'ann')");
    CHECK(has("text=ann "));
    CHECK(js("rxTest.value('name')") == "ann");
    js("rxTest.click('upper')");
    js("rxTest.type('name', 'bob')");
    CHECK(has("text=BOB "));
    CHECK(js("rxTest.value('name')") == "BOB");  // the page shows the state, not the raw typing

    std::cout << "keys\n";
    js("rxTest.key('name', 'ArrowUp')");
    CHECK(has("key=Up "));
    js("rxTest.key('name', 'Tab', true)");
    CHECK(has("key=Tab+shift "));

    std::cout << "checkbox, style, boolean props\n";
    js("rxTest.toggle('agree')");
    CHECK(has("agree=yes"));
    CHECK(has("style=\"color: green\""));
    CHECK(js("String(document.querySelector('#out').disabled)") == "true");  // a property, not an attribute
    js("rxTest.toggle('agree')");
    CHECK(has("agree=no"));
    CHECK(!has("style="));
    CHECK(js("String(document.querySelector('#out').disabled)") == "false");

    std::cout << "a checked prop is controlled: an ignored click is undone\n";
    js("rxTest.toggle('locked')");
    CHECK(js("String(document.getElementById('locked').checked)") == "true");

    std::cout << "form submit does not reload the page\n";
    CHECK(js("rxTest.submit('form')") == "prevented");
    CHECK(has("submits=1"));

    std::cout << "keyed list reorders and removes nodes\n";
    js("globalThis.firstA = document.querySelector('#item-a')");
    js("rxTest.click('reverse')");
    CHECK(has("<ul id=\"list\"><li id=\"item-c\">c</li><li id=\"item-b\">b</li><li id=\"item-a\">a</li></ul>"));
    CHECK(js("String(document.querySelector('#item-a') === globalThis.firstA)") == "true");  // moved, not recreated
    js("rxTest.click('drop')");
    CHECK(has("<ul id=\"list\"><li id=\"item-a\">a</li><li id=\"item-c\">c</li></ul>"));

    std::cout << "javascript: URLs are blocked, others kept\n";
    CHECK(js("document.getElementById('evil').getAttribute('href')") == "about:blank#blocked");
    CHECK(js("document.getElementById('pic').getAttribute('src')") == "about:blank#blocked");
    CHECK(js("document.getElementById('fine').getAttribute('href')") == "/javascript:notes");

    std::cout << "a ref gives the real element to C++ (reactx/js.hpp)\n";
    mountDom(h(RefProbe));  // replaces the app above: the last check
    CHECK(refTag == "SECTION#probe");
    CHECK(has("data-seen=\"yes\""));

    std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("OK, all tests passed")) << '\n';
    return failures ? 1 : 0;
}
