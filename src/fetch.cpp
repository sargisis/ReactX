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

LocalFetch& localFetch() {
    static LocalFetch handler;
    return handler;
}

#ifdef __EMSCRIPTEN__
std::map<int, std::function<void(const FetchResponse&)>>& pending() {
    static std::map<int, std::function<void(const FetchResponse&)>> map;
    return map;
}

std::map<int, FetchResponse>& localAnswers() {
    static std::map<int, FetchResponse> map;
    return map;
}

// Delivers a local answer on a timer, like a network response.
EM_JS(void, rx_fetch_js_later, (int id), {
    setTimeout(function() { Module.ccall("rx_fetch_local_done", null, ["number"], [id]); }, 0);
});

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

void setLocalFetch(LocalFetch handler) { localFetch() = std::move(handler); }

void fetch(FetchRequest request, std::function<void(const FetchResponse&)> done) {
    std::optional<FetchResponse> local;
    if (localFetch()) {
        try {
            local = localFetch()(request);
        } catch (const std::exception& e) {
            local = FetchResponse{.status = 500, .body = e.what(), .error = {}};
        }
    }
#ifdef __EMSCRIPTEN__
    static int nextId = 1;
    int id = nextId++;
    pending()[id] = std::move(done);
    if (local) {
        localAnswers()[id] = std::move(*local);
        rx_fetch_js_later(id);
        return;
    }
    rx_fetch_js(id, request.method.c_str(), request.url.c_str(), request.body.c_str(), request.contentType.c_str());
#else
    if (local) done(*local);
    else done(FetchResponse{.status = 0, .body = {}, .error = "no network in the terminal build (use setLocalFetch)"});
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
extern "C" EMSCRIPTEN_KEEPALIVE void rx_fetch_local_done(int id) {
    auto& answers = rx::localAnswers();
    auto it = answers.find(id);
    if (it == answers.end()) return;
    rx::FetchResponse r = std::move(it->second);
    answers.erase(it);
    auto& map = rx::pending();
    auto cb = map.find(id);
    if (cb == map.end()) return;
    auto done = std::move(cb->second);
    map.erase(cb);
    done(r);
    rx::flushDom();
}

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
