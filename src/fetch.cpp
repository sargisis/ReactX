#include "reactx/fetch.hpp"

#include <map>
#include <memory>
#include <utility>

#include "reactx/hooks.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "reactx/dom.hpp"
#else
#include <chrono>
#include <mutex>
#include <thread>

#include "reactx/http.hpp"
#endif

namespace rx {

namespace {

LocalFetch& localFetch() {
    static LocalFetch handler;
    return handler;
}

#ifndef __EMSCRIPTEN__
// Native requests run on a thread of their own; the answer waits here until
// the UI thread picks it up (a timer polls it, so it arrives on Root::flush()
// like every other state change). The thread touches nothing else, so a
// program may exit while a request is still out.
struct NativeRequest {
    std::mutex mutex;
    std::optional<FetchResponse> answer;
};

void pollAnswer(std::shared_ptr<NativeRequest> request, std::function<void(const FetchResponse&)> done) {
    using namespace std::chrono_literals;
    detail::setTimer(10ms, [request, done = std::move(done)]() mutable {
        std::optional<FetchResponse> answer;
        {
            std::scoped_lock lock(request->mutex);
            answer = std::exchange(request->answer, std::nullopt);
        }
        if (answer) done(*answer);
        else pollAnswer(std::move(request), std::move(done));
    });
}

// "http://host:port/path?q" -> the server and "/path?q".
std::expected<std::pair<net::Upstream, std::string>, std::string> splitUrl(const std::string& url) {
    if (url.starts_with("https://")) return std::unexpected("https isn't supported in the terminal build, use http://");
    if (!url.starts_with("http://"))
        return std::unexpected("the terminal build needs a full URL like http://127.0.0.1:8090" + url + " (or setLocalFetch)");
    auto rest = std::string_view(url).substr(7);
    auto cut = rest.find_first_of("/?#");
    std::string target = cut == std::string_view::npos ? "/" : std::string(rest.substr(cut));
    if (auto hash = target.find('#'); hash != std::string::npos) target.resize(hash);
    if (target.empty() || target.front() == '?') target.insert(0, "/");
    auto upstream = net::parseUpstream("http://" + std::string(rest.substr(0, cut)));
    if (!upstream) return std::unexpected(upstream.error());
    return std::pair{*upstream, target};
}
#endif

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
    if (local) {
        done(*local);
        return;
    }
    auto where = splitUrl(request.url);
    if (!where) {
        done(FetchResponse{.status = 0, .body = {}, .error = where.error()});
        return;
    }
    net::Request out{.method = request.method, .body = request.body, .target = where->second};
    if (!request.body.empty() || request.method != "GET") out.headers.emplace("content-type", request.contentType);
    out.headers.emplace("accept", "application/json, text/plain, */*");
    auto shared = std::make_shared<NativeRequest>();
    std::thread([shared, out = std::move(out), upstream = where->first] {
        auto reply = net::forward(out, upstream);
        FetchResponse answer = reply ? FetchResponse{.status = reply->status, .body = std::move(reply->body), .error = {}}
                                     : FetchResponse{.status = 0, .body = {}, .error = reply.error().message};
        std::scoped_lock lock(shared->mutex);
        shared->answer = std::move(answer);
    }).detach();
    pollAnswer(std::move(shared), std::move(done));
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
