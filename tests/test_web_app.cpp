// Router + useFetch in the browser build: WebAssembly under Node with
// tests/web/fake_dom.js (fake history and fetch). Answers arrive later, so the
// test runs as steps on timers.
#include <emscripten.h>

#include <iostream>
#include <string>

#include "reactx/dom.hpp"
#include "reactx/fetch.hpp"
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
std::string html() { return js("rxTest.html()"); }
bool has(const std::string& part) { return html().find(part) != std::string::npos; }

std::string posted;

Element Home(const Props&) {
    auto todos = useFetch("/api/todos");
    std::string text = todos.loading ? "loading" : todos.ok() ? std::to_string(todos.json().size()) + " todos" : todos.errorText();
    return h("p", {{"id", "home"}}, {text});
}

Element TodoPage(const Props&) {
    std::string id = router::useParam("id");
    auto todo = useFetch("/api/todos/" + id);
    std::string text = todo.loading ? "loading " + id : todo.ok() ? todo.json()["title"].asString() : todo.errorText();
    return h("div", {}, {
        h("p", {{"id", "todo"}}, {text}),
        h("button", {{"id", "reload"}, {"onClick", todo.reload}}, {"reload"}),
    });
}

Element NotFound(const Props&) { return h("p", {{"id", "missing"}}, {"no page " + router::useLocation()}); }

Element App(const Props&) {
    return h("div", {}, {
        h("nav", {}, {
            router::link({.to = "/", .label = "Home", .key = "home"}),
            router::link({.to = "/todos/7", .children = {h("span", {{"id", "to7"}}, {"Todo 7"})}, .key = "t7"}),
        }),
        router::routes({{"/", h(Home)}, {"/todos/:id", h(TodoPage)}, {"*", h(NotFound)}}),
    });
}

void next(int step, int delayMs = 5) {
    EM_ASM({ setTimeout(function() { Module.ccall("test_step", null, ["number"], [$0]); }, $1); }, step, delayMs);
}

void finish() {
    std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("OK, all tests passed")) << std::endl;
    EM_ASM({ process.exitCode = $0; }, failures ? 1 : 0);
}

}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void test_step(int step) {
    switch (step) {
        case 1:
            next(2);
            break;
        case 2:
            CHECK(has("<p id=\"home\">2 todos</p>"));
            std::cout << "a link navigates without reloading\n";
            CHECK(has("<a class=\"active\" data-rx-link href=\"/\">Home</a>"));
            CHECK(js("rxTest.clickPrevented('to7')") == "prevented");
            CHECK(js("rxTest.path()") == "/todos/7");
            CHECK(has("<p id=\"todo\">loading 7</p>"));
            CHECK(has("<a class=\"active\" data-rx-link href=\"/todos/7\">"));
            next(3);
            break;
        case 3:
            CHECK(has("<p id=\"todo\">Buy milk</p>"));
            std::cout << "reload fetches again\n";
            js("rxTest.mock('GET', '/api/todos/7', 200, '{\"title\": \"Buy oat milk\"}')");
            js("rxTest.click('reload')");
            CHECK(has("<p id=\"todo\">loading 7</p>"));  // loading again (the old response is still in .response)
            next(4);
            break;
        case 4:
            CHECK(has("<p id=\"todo\">Buy oat milk</p>"));
            std::cout << "errors: HTTP status and network\n";
            router::navigate("/todos/404");
            next(5);
            break;
        case 5:
            CHECK(has("<p id=\"todo\">HTTP 404</p>"));
            router::navigate("/todos/offline");
            next(6);
            break;
        case 6:
            CHECK(has("<p id=\"todo\">network error: no route GET /api/todos/offline</p>"));
            std::cout << "Back button (popstate)\n";
            js("rxTest.back()");
            next(7);
            break;
        case 7:
            CHECK(js("rxTest.path()") == "/todos/404");
            CHECK(has("HTTP 404") || has("loading 404"));
            std::cout << "unknown path and Ctrl+click\n";
            router::navigate("/nothing/here");
            CHECK(has("<p id=\"missing\">no page /nothing/here</p>"));
            CHECK(js("rxTest.ctrlClick('to7')") == "followed");  // new tab: the browser handles it
            CHECK(js("rxTest.path()") == "/nothing/here");
            std::cout << "POST with fetch()\n";
            js("rxTest.mock('POST', '/api/todos', 201, '{\"id\": 3}')");
            fetch({.method = "POST", .url = "/api/todos", .body = R"({"title":"Tea"})"},
                  [](const FetchResponse& r) { posted = std::to_string(r.status) + " " + r.json().dump(); });
            next(8);
            break;
        case 8:
            CHECK(posted == "201 {\"id\":3}");
            CHECK(js("rxTest.requests()").find("POST /api/todos {\"title\":\"Tea\"}") != std::string::npos);
            finish();
            break;
    }
}

int main() {
    js("rxTest.mock('GET', '/api/todos', 200, '[{\"id\":1},{\"id\":2}]')");
    js("rxTest.mock('GET', '/api/todos/7', 200, '{\"title\": \"Buy milk\"}')");
    js("rxTest.mock('GET', '/api/todos/404', 404, 'not found')");
    mountDom(h(App));
    std::cout << "home loads data\n";
    CHECK(has("<p id=\"home\">loading</p>"));  // the answer comes later
    next(1, 0);
}
