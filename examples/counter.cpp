// Counter: useState, useEffect and events on the text backend.
#include <iostream>
#include <print>
#include <string>

// __INTELLISENSE__: the VS Code C/C++ extension can't parse GCC modules yet,
// so the editor reads the headers while the compiler uses the module.
#if defined(REACTX_USE_MODULES) && !defined(__INTELLISENSE__)
import reactx;
#else
#include "reactx/reactx.hpp"
#include "reactx/text_renderer.hpp"
#endif

using namespace rx;

// Props are a plain struct: h(Counter, {.start = 5}) — a typo in a field
// name or a wrong type is a compile error.
struct CounterProps {
    int start = 0;
};

Element Counter(const CounterProps& props) {
    auto [count, setCount] = useState(props.start);

    useEffect(
        [count] {
            std::println("  [effect] count = {}", count);
            return [count] { std::println("  [cleanup] count was {}", count); };
        },
        deps(count));

    return h("box", {{"id", "counter"}}, {
        h("text", {}, {"Count: " + std::to_string(count)}),
        h("button", {{"id", "inc"}, {"onClick", [setCount] { setCount([](int c) { return c + 1; }); }}},
          {"+1"}),
        h("button", {{"id", "reset"}, {"onClick", [setCount] { setCount(0); }}}, {"reset"}),
    });
}

int main() {
    TextRenderer renderer(&std::cout);
    Root root(renderer);

    auto click = [&](const std::string& id) {
        std::println("\n>>> click #{}", id);
        root.dispatch(*renderer.findByProp("id", id), "onClick");
        std::print("{}", renderer.toString());
    };

    std::println(">>> mount");
    root.render(h(Counter, {.start = 5}));
    std::print("{}", renderer.toString());

    click("inc");
    click("inc");
    click("reset");
}
