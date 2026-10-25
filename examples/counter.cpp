// Counter: useState, useEffect and events on the text backend.
#include <iostream>
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

Element Counter(const Props& props) {
    auto [count, setCount] = useState(props.get<int>("start", 0));

    useEffect(
        [count] {
            std::cout << "  [effect] count = " << count << '\n';
            return [count] { std::cout << "  [cleanup] count was " << count << '\n'; };
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
        std::cout << "\n>>> click #" << id << '\n';
        root.dispatch(*renderer.findByProp("id", id), "onClick");
        std::cout << renderer.toString();
    };

    std::cout << ">>> mount\n";
    root.render(h(Counter, {{"start", 5}}));
    std::cout << renderer.toString();

    click("inc");
    click("inc");
    click("reset");
}
