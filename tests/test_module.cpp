// Checks that the library works through `import reactx;` (C++20 modules).
#include <iostream>
#include <string>

// The VS Code C/C++ extension can't parse GCC modules yet: show it the headers.
#ifdef __INTELLISENSE__
#include "reactx/reactx.hpp"
#include "reactx/text_renderer.hpp"
#else
import reactx;
#endif

using namespace rx;

namespace {

const auto Accent = createContext<std::string>("blue");

Element Badge(const Props& p) {
    return h("badge", {{"color", useContext(Accent)}}, {p.get<std::string>("label")});
}

Element Field(const Props& p) {
    auto [value, setValue] = useState(p.get<std::string>("initial"));
    int& renders = useRef(0);
    ++renders;
    useEffect([] {}, deps(value));
    const auto& upper = useMemo([&] { return value + "!"; }, deps(value));
    Callback reset = useCallback([setValue] { setValue(std::string{}); }, deps());
    return h("box", {}, {
        h("input", {{"id", "field"},
                    {"value", value},
                    {"onInput", [setValue](const std::string& t) { setValue(t); }},
                    {"onKey", [setValue](const KeyEvent& e) {
                         if (e.key == "Escape") setValue(std::string{});
                     }}}),
        fragment({text("renders=" + std::to_string(renders))}),
        h("button", {{"id", "reset"}, {"onClick", reset}}, {upper}),
        Accent.provide("red", {h(memo<Badge>, {{"label", "new"}})}),
    });
}

}  // namespace

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            ++failures;
            std::cerr << "FAIL: " << what << '\n';
        }
    };

    TextRenderer r;
    Root root(r);
    root.render(h(Field, {{"initial", "a"}}));
    NodeHandle field = *r.findByProp("id", "field");

    check(root.dispatch(field, "onInput", std::string("hello")), "onInput dispatched");
    check(r.toString().find("value=\"hello\"") != std::string::npos, "value updated");
    check(root.dispatch(field, "onKey", KeyEvent{"Escape"}), "onKey dispatched");
    check(r.toString().find("value=\"\"") != std::string::npos, "value cleared");
    check(r.toString().find("renders=3") != std::string::npos, "useRef counts renders");
    check(r.toString().find("<badge color=\"red\">") != std::string::npos, "useContext + memo");
    root.dispatch(field, "onInput", std::string("x"));
    check(r.toString().find("\"x!\"") != std::string::npos, "useMemo");
    check(root.dispatch(*r.findByProp("id", "reset"), "onClick"), "useCallback handler");
    check(r.toString().find("value=\"\"") != std::string::npos, "useCallback reset value");

    std::cout << (failures ? "module test FAILED\n" : "module test OK\n");
    return failures ? 1 : 0;
}
