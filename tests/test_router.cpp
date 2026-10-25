// Router tests on the text backend (the terminal build of rx::router).
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "reactx/path.hpp"
#include "reactx/reactx.hpp"
#include "reactx/router.hpp"
#include "reactx/text_renderer.hpp"

using namespace rx;

namespace {

int failures = 0;
std::vector<std::pair<std::string, std::function<void()>>> tests;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

#define TEST(name)                                                            \
    void name();                                                              \
    const bool name##_registered = (tests.emplace_back(#name, name), true);   \
    void name()

bool has(const TextRenderer& r, const std::string& part) { return r.toString().find(part) != std::string::npos; }

int pageMounts = 0;

Element Home(const Props&) { return h("page", {}, {"home"}); }
Element TodoPage(const Props&) {
    useEffect([] { ++pageMounts; }, deps());
    return h("page", {}, {"todo " + router::useParam("id") + " tab=" + router::useQuery("tab")});
}
Element NotFound(const Props&) { return h("page", {}, {"404 " + router::useLocation()}); }

Element App(const Props&) {
    return h("app", {}, {
        router::link({.to = "/", .label = "Home"}),
        router::link({.to = "/todos/7", .label = "Seven"}),
        router::routes({
            {"/", h(Home)},
            {"/todos/:id", h(TodoPage)},
            {"*", h(NotFound)},
        }),
    });
}

TEST(matches_paths) {
    std::map<std::string, std::string> p;
    CHECK(matchPath("/", "/", p));
    CHECK(matchPath("/", "", p));
    CHECK(matchPath("/todos/:id", "/todos/7/", p) && p["id"] == "7");
    CHECK(!matchPath("/todos/:id", "/todos", p));
    CHECK(matchPath("/docs/*", "/docs/a/b", p) && p["*"] == "a/b");
    CHECK(matchPath("*", "/anything/at/all", p));
    CHECK(queryParam("?q=milk%20tea&page=2", "q") == "milk tea");
    CHECK(queryParam("page=2", "page") == "2");
    CHECK(!queryParam("page=2", "q").has_value());
}

TEST(renders_the_matching_route) {
    router::reset("/");
    TextRenderer r;
    Root root(r);
    root.render(h(App));
    CHECK(has(r, "\"home\""));

    router::navigate("/todos/7?tab=info");
    root.flush();
    CHECK(has(r, "\"todo 7 tab=info\""));
    CHECK(router::currentPath() == "/todos/7?tab=info");

    router::navigate("/nope/x");
    root.flush();
    CHECK(has(r, "\"404 /nope/x\""));
}

TEST(same_route_keeps_the_page_new_route_remounts_it) {
    router::reset("/todos/1");
    TextRenderer r;
    Root root(r);
    pageMounts = 0;
    root.render(h(App));
    router::navigate("/todos/2");
    root.flush();
    CHECK(has(r, "\"todo 2 tab=\""));
    CHECK(pageMounts == 1);  // only the param changed: same page, new props
    router::navigate("/");
    root.flush();
    router::navigate("/todos/3");
    root.flush();
    CHECK(pageMounts == 2);  // left the route and came back: mounted again
}

TEST(back_goes_to_the_previous_path) {
    router::reset("/");
    TextRenderer r;
    Root root(r);
    root.render(h(App));
    router::navigate("/todos/5");
    router::navigate("/todos/6");
    router::back();
    root.flush();
    CHECK(has(r, "\"todo 5 tab=\""));
    router::back();
    root.flush();
    CHECK(has(r, "\"home\""));
    router::back();  // nothing before "/": stays
    root.flush();
    CHECK(router::currentPath() == "/");
}

TEST(replace_does_not_add_history) {
    router::reset("/");
    router::navigate("/todos/1");
    router::navigate("/todos/2", true);
    router::back();
    CHECK(router::currentPath() == "/");
}

TEST(links_navigate_and_show_the_active_one) {
    router::reset("/");
    TextRenderer r;
    Root root(r);
    root.render(h(App));
    CHECK(has(r, "<text bold=\"true\" focusable=\"true\" underline=\"true\" wrap=\"false\">\n      \"Home\""));
    CHECK(has(r, "<text bold=\"false\" focusable=\"true\" underline=\"true\" wrap=\"false\">\n      \"Seven\""));

    // Click the links in tree order: "Home" is the current page (no change),
    // the next one is "Seven".
    for (NodeHandle h = 2; h < 100 && router::currentPath() != "/todos/7"; ++h) root.dispatch(h, "onClick");
    CHECK(router::currentPath() == "/todos/7");
    CHECK(has(r, "\"todo 7 tab=\""));
    CHECK(has(r, "<text bold=\"true\" focusable=\"true\" underline=\"true\" wrap=\"false\">\n      \"Seven\""));
}

TEST(listeners_are_removed_on_unmount) {
    router::reset("/");
    {
        TextRenderer r;
        Root root(r);
        root.render(h(App));
    }  // unmounted: navigating must not touch dead components
    router::navigate("/todos/9");
    CHECK(router::currentPath() == "/todos/9");
}

}  // namespace

int main() {
    for (auto& [name, fn] : tests) {
        std::cout << name << '\n';
        try {
            fn();
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "  EXCEPTION: " << e.what() << '\n';
        }
    }
    std::cout << (failures ? "FAILED: " : "OK, all tests passed") << (failures ? std::to_string(failures) : "")
              << '\n';
    return failures ? 1 : 0;
}
