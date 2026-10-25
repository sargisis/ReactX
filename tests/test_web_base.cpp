// The router under a base path, like a site on GitHub Pages at /ReactX/.
// WebAssembly under Node with tests/web/fake_dom.js.
#include <emscripten.h>

#include <iostream>
#include <string>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"
#include "reactx/router.hpp"

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
bool has(const std::string& part) { return js("rxTest.html()").find(part) != std::string::npos; }

Element Page(const Props& p) { return h("p", {{"id", "page"}}, {p.get<std::string>("name") + " " + router::useParam("id")}); }

Element App(const Props&) {
    return h("div", {}, {
        router::link({.to = "/", .label = "Home", .key = "h"}),
        router::link({.to = "/todos/7", .children = {h("span", {{"id", "to7"}}, {"7"})}, .key = "t"}),
        router::routes({
            {"/", h(Page, {{"name", "home"}})},
            {"/todos/:id", h(Page, {{"name", "todo"}})},
            {"*", h(Page, {{"name", "404"}})},
        }),
    });
}

void next(int step) {
    EM_ASM({ setTimeout(function() { Module.ccall("test_step", null, ["number"], [$0]); }, 5); }, step);
}

}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void test_step(int step) {
    if (step == 1) {
        std::cout << "Back strips the base again\n";
        CHECK(js("rxTest.path()") == "/ReactX/todos/3");
        CHECK(has("<p id=\"page\">todo 3</p>"));
        std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("OK, all tests passed")) << std::endl;
        EM_ASM({ process.exitCode = $0; }, failures ? 1 : 0);
    }
}

int main() {
    js("rxTest.setBase('/ReactX/', '/ReactX/todos/3')");  // opened directly (GitHub Pages serves 404.html = the app)
    mountDom(h(App));

    std::cout << "base comes from <base href>\n";
    CHECK(router::base() == "/ReactX");
    CHECK(router::currentPath() == "/todos/3");
    CHECK(has("<p id=\"page\">todo 3</p>"));

    std::cout << "links and navigate add the base\n";
    CHECK(has("<a class data-rx-link href=\"/ReactX/\">Home</a>"));
    CHECK(has("href=\"/ReactX/todos/7\""));
    CHECK(router::href("/about") == "/ReactX/about");
    js("rxTest.click('to7')");
    CHECK(js("rxTest.path()") == "/ReactX/todos/7");
    CHECK(has("<p id=\"page\">todo 7</p>"));
    router::navigate("/");
    CHECK(js("rxTest.path()") == "/ReactX/");
    CHECK(has("<p id=\"page\">home </p>"));
    router::navigate("/x?y=1");
    CHECK(js("rxTest.path()") == "/ReactX/x?y=1");
    CHECK(has("<p id=\"page\">404 </p>"));

    js("history.back(); history.back(); history.back()");  // to the first page
    next(1);
}
