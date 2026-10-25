// Counter in the browser: the same ReactX components, rendered into the DOM.
#include <string>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"

using namespace rx;

Element Counter(const Props& props) {
    auto [count, setCount] = useState(props.get<int>("start", 0));
    return h("div", {{"className", "card demo"}}, {
        h("h1", {}, {"ReactX in the browser"}),
        h("p", {{"className", "count"}, {"id", "count"}}, {std::to_string(count)}),
        h("div", {{"className", "row"}}, {
            h("button", {{"id", "dec"}, {"onClick", [setCount] { setCount([](int c) { return c - 1; }); }}}, {"−"}),
            h("button", {{"id", "inc"}, {"className", "primary grow"},
                         {"onClick", [setCount] { setCount([](int c) { return c + 1; }); }}}, {"+1"}),
            h("button", {{"id", "reset"}, {"onClick", [setCount] { setCount(0); }}}, {"reset"}),
        }),
        h("p", {{"className", "hint"}}, {"C++20 → WebAssembly. No JavaScript written by hand."}),
    });
}

int main() { mountDom(h(Counter, {{"start", 0}})); }
