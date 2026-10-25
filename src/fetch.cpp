#include "reactx/fetch.hpp"

#include <map>
#include <memory>
#include <utility>

#include "reactx/hooks.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "reactx/dom.hpp"
#endif

namespace rx {

namespace {

#ifdef __EMSCRIPTEN__
std::map<int, std::function<void(const FetchResponse&)>>& pending() {
    static std::map<int, std::function<void(const FetchResponse&)>> map;
    return map;
}

EM_JS(void, rx_fetch_js, (int id, const char* method, const char* url, const char* body, const char* contentType), {
    const m = UTF8ToString(method);
    const opts = { method: m, headers: {} };
    if (m !== "GET" && m !== "HEAD") {
        opts.body = UTF8ToString(body);
        opts.headers["Content-Type"] = UTF8ToString(contentType);
    }
    const done = function(status, text, error) {
        Module.ccall("rx_fetch_done", null, ["number", "number", "string", "string"], [id, status, text, error]);
    };
    fetch(UTF8ToString(url), opts)
        .then(function(r) { return r.text().then(function(t) { done(r.status, t, ""); }); })
        .catch(function(e) { done(0, "", String((e && e.message) || e)); });
});
#endif

// What useFetch keeps in state (comparable, so equal results don't re-render).
struct Loaded {
    bool loading = true;
    FetchResponse response;
    bool operator==(const Loaded&) const = default;
};

}  // namespace

void fetch(FetchRequest request, std::function<void(const FetchResponse&)> done) {
#ifdef __EMSCRIPTEN__
    static int nextId = 1;
    int id = nextId++;
    pending()[id] = std::move(done);
    rx_fetch_js(id, request.method.c_str(), request.url.c_str(), request.body.c_str(), request.contentType.c_str());
#else
    (void)request;
    done(FetchResponse{.status = 0, .body = {}, .error = "fetch is only available in the browser build"});
#endif
}

std::string FetchState::errorText() const {
    if (loading) return {};
    if (!response.error.empty()) return "network error: " + response.error;
    if (!response.ok()) return "HTTP " + std::to_string(response.status);
    return {};
}

FetchState useFetch(const std::string& url) {
    auto [loaded, setLoaded] = useState(Loaded{});
    auto [reloads, setReloads] = useState(0);

    useEffect([url, setLoaded] {
        // Ignore the answer if the url changed or the component went away.
        auto alive = std::make_shared<bool>(true);
        setLoaded([](Loaded l) {
            l.loading = true;
            return l;
        });
        fetch({.method = "GET", .url = url}, [alive, setLoaded](const FetchResponse& r) {
            if (*alive) setLoaded(Loaded{.loading = false, .response = r});
        });
        return [alive] { *alive = false; };
    }, deps(url, reloads));

    return FetchState{.loading = loaded.loading,
                      .response = loaded.response,
                      .reload = [setReloads] { setReloads([](int n) { return n + 1; }); }};
}

}  // namespace rx

#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void rx_fetch_done(int id, int status, const char* body, const char* error) {
    auto& map = rx::pending();
    auto it = map.find(id);
    if (it == map.end()) return;
    auto done = std::move(it->second);
    map.erase(it);
    done(rx::FetchResponse{.status = status, .body = body, .error = error});
    rx::flushDom();  // the response arrived outside of a DOM event
}
#endif
