#include "reactx/router.hpp"

#include <algorithm>
#include <functional>
#include <utility>

#include "reactx/context.hpp"
#include "reactx/hooks.hpp"
#include "reactx/path.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include <cstdlib>

#include "reactx/dom.hpp"
#endif

namespace rx::router {

namespace {

// ---- location store: the current path and who wants to know about it ----

struct Store {
    std::string base;  // "/ReactX" on GitHub Pages, "" at the root of a domain
    std::string path = "/";
    std::vector<std::string> history{"/"};  // terminal only; the browser keeps its own
    std::vector<std::pair<int, std::function<void()>>> listeners;
    int nextId = 0;
    bool started = false;
};

Store& store() {
    static Store s;
    return s;
}

std::string withoutQuery(const std::string& path) { return path.substr(0, path.find('?')); }

std::string queryOf(const std::string& path) {
    auto q = path.find('?');
    return q == std::string::npos ? std::string{} : path.substr(q + 1);
}

void setPath(std::string path);

// "/ReactX/todos/7" -> "/todos/7" (paths inside the app never include the base).
[[maybe_unused]] std::string inside(const std::string& full) {  // browser only
    const std::string& base = store().base;
    if (base.empty() || !full.starts_with(base)) return full.empty() ? "/" : full;
    std::string rest = full.substr(base.size());
    if (rest.empty() || rest.front() == '?') return "/" + rest;
    return rest.front() == '/' ? rest : full;  // "/ReactXtra" is not inside "/ReactX"
}

// "/todos/7" -> "/ReactX/todos/7"; "/" -> "/ReactX/" (the folder itself).
std::string outside(const std::string& path) {
    const std::string& base = store().base;
    if (base.empty()) return path;
    return base + (path.empty() || path.front() == '?' ? "/" + path : path);
}

#ifdef __EMSCRIPTEN__

EM_JS(char*, rx_router_js_current, (), {
    return stringToNewUTF8(location.pathname + location.search);
});

// Path of <base href="..."> without the trailing slash ("/ReactX"), or "".
EM_JS(char*, rx_router_js_base, (), {
    const el = document.querySelector("base[href]");
    if (!el) return stringToNewUTF8("");
    const page = location.href || ("http://localhost" + location.pathname);
    let path = new URL(el.getAttribute("href"), page).pathname;
    while (path.endsWith("/")) path = path.slice(0, -1);
    return stringToNewUTF8(path);
});

EM_JS(void, rx_router_js_listen, (), {
    window.addEventListener("popstate", function() {
        Module.ccall("rx_router_popstate", null, ["string"], [location.pathname + location.search]);
    });
});

EM_JS(void, rx_router_js_go, (const char* path, int replace), {
    const p = UTF8ToString(path);
    if (replace) history.replaceState(null, "", p); else history.pushState(null, "", p);
    if (window.scrollTo) window.scrollTo(0, 0);
});

EM_JS(void, rx_router_js_back, (), { history.back(); });

#endif

void start() {
    Store& s = store();
    if (s.started) return;
    s.started = true;
#ifdef __EMSCRIPTEN__
    char* base = rx_router_js_base();
    s.base = base;
    std::free(base);
    char* current = rx_router_js_current();
    s.path = inside(current);
    std::free(current);
    rx_router_js_listen();
#endif
}

void setPath(std::string path) {
    Store& s = store();
    if (path == s.path) return;
    s.path = std::move(path);
    auto listeners = s.listeners;  // a listener may unsubscribe
    for (auto& [id, fn] : listeners) fn();
#ifdef __EMSCRIPTEN__
    flushDom();  // re-render now, even outside a DOM event (Back button)
#endif
}

int subscribe(std::function<void()> fn) {
    Store& s = store();
    int id = s.nextId++;
    s.listeners.emplace_back(id, std::move(fn));
    return id;
}

void unsubscribe(int id) {
    std::erase_if(store().listeners, [id](const auto& l) { return l.first == id; });
}

const Context<Params> ParamsContext{Params{}};

// Re-renders the calling component when the path changes.
void useLocationUpdates() {
    auto [version, setVersion] = useState(0);
    useEffect([setVersion] {
        int id = subscribe([setVersion] { setVersion([](int v) { return v + 1; }); });
        return [id] { unsubscribe(id); };
    }, deps());
    start();
}

// ---- components ----

Element Routes(const Props& p) {
    useLocationUpdates();
    std::string path = withoutQuery(store().path);
    for (const Element& route : p.children()) {
        std::string pattern = route.props.get<std::string>("path");
        Params params;
        if (!matchPath(pattern, path, params)) continue;
        // Keyed by the pattern: going to another route mounts a fresh page.
        return ParamsContext.provide(std::move(params), {fragment(route.props.children(), pattern)});
    }
    return fragment({});
}

Element Link(const Props& p) {
    useLocationUpdates();
    std::string to = p.get<std::string>("to");
    bool active = withoutQuery(store().path) == withoutQuery(to);
    std::string className = p.get<std::string>("className");
    if (active && !p.get<std::string>("activeClassName").empty())
        className += (className.empty() ? "" : " ") + p.get<std::string>("activeClassName");
    Callback go = [to] { navigate(to); };

    std::vector<Element> content = p.children();
    if (content.empty()) content.push_back(p.get<std::string>("label"));
#ifdef __EMSCRIPTEN__
    return h("a", {{"href", outside(to)}, {"className", className}, {"data-rx-link", true}, {"onClick", go}},
             std::move(content));
#else
    return h("text", {{"focusable", true}, {"underline", true}, {"bold", active}, {"wrap", false}, {"onClick", go}},
             std::move(content));
#endif
}

}  // namespace

// ---- public API ----

Element routes(std::vector<Route> table) {
    std::vector<Element> children;
    for (auto& r : table) children.push_back(h("rx-route", {{"path", r.path}}, {std::move(r.element)}));
    return h(Routes, {}, std::move(children));
}

Element link(LinkProps p) {
    Element e = h(Link, {{"to", std::move(p.to)},
                         {"label", std::move(p.label)},
                         {"className", std::move(p.className)},
                         {"activeClassName", std::move(p.activeClassName)}},
                  std::move(p.children));
    e.key = std::move(p.key);
    return e;
}

void navigate(const std::string& path, bool replace) {
    start();
    if (path == store().path) return;
#ifdef __EMSCRIPTEN__
    rx_router_js_go(outside(path).c_str(), replace ? 1 : 0);
#else
    auto& h = store().history;
    if (replace && !h.empty()) h.back() = path;
    else h.push_back(path);
#endif
    setPath(path);
}

void back() {
    start();
#ifdef __EMSCRIPTEN__
    rx_router_js_back();  // the browser fires popstate, which calls setPath
#else
    auto& h = store().history;
    if (h.size() < 2) return;
    h.pop_back();
    setPath(h.back());
#endif
}

std::string base() {
    start();
    return store().base;
}

void setBase(std::string path) {
    start();
    while (!path.empty() && path.back() == '/') path.pop_back();
    store().base = std::move(path);
}

std::string href(const std::string& path) {
    start();
    return outside(path);
}

std::string currentPath() {
    start();
    return store().path;
}

void reset(const std::string& path) {
    start();
    store().history = {path};
#ifdef __EMSCRIPTEN__
    rx_router_js_go(outside(path).c_str(), 1);
#endif
    setPath(path);
}

std::string useLocation() {
    useLocationUpdates();
    return withoutQuery(store().path);
}

const Params& useParams() { return useContext(ParamsContext); }

std::string useParam(const std::string& name) {
    const Params& params = useParams();
    auto it = params.find(name);
    return it == params.end() ? std::string{} : it->second;
}

std::string useQuery(const std::string& name) {
    useLocationUpdates();
    return queryParam(queryOf(store().path), name).value_or("");
}

}  // namespace rx::router

#ifdef __EMSCRIPTEN__
// Back / Forward in the browser.
extern "C" EMSCRIPTEN_KEEPALIVE void rx_router_popstate(const char* path) {
    rx::router::setPath(rx::router::inside(path));
}
#endif
