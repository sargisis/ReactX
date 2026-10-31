#include "reactx/fetch.hpp"

#include <algorithm>
#include <cctype>
#include <format>
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

using Clock = std::chrono::steady_clock;

// Polls on the UI thread until the answer is there, the request was cancelled
// (then `done` never runs) or the deadline passed (a timeout answer; the
// thread's late answer is dropped).
void pollAnswer(std::shared_ptr<NativeRequest> request, std::shared_ptr<bool> cancelled,
                std::optional<Clock::time_point> deadline, std::chrono::milliseconds timeout,
                std::function<void(const FetchResponse&)> done) {
    using namespace std::chrono_literals;
    detail::setTimer(10ms, [request, cancelled, deadline, timeout, done = std::move(done)]() mutable {
        if (*cancelled) return;
        std::optional<FetchResponse> answer;
        {
            std::scoped_lock lock(request->mutex);
            answer = std::exchange(request->answer, std::nullopt);
        }
        if (answer) done(*answer);
        else if (deadline && Clock::now() >= *deadline)
            done(FetchResponse{.status = 0, .body = {}, .error = "timeout after " + std::to_string(timeout.count()) + " ms"});
        else pollAnswer(std::move(request), std::move(cancelled), deadline, timeout, std::move(done));
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

// `headers`: JSON [[name, value], ...]. The answer's headers go back the same way.
EM_JS(void, rx_fetch_js, (int id, const char* method, const char* url, const char* body, const char* contentType,
                          const char* headers, int timeoutMs), {
    const m = UTF8ToString(method);
    const opts = { method: m, headers: {} };
    if (m !== "GET" && m !== "HEAD") {
        opts.body = UTF8ToString(body);
        opts.headers["Content-Type"] = UTF8ToString(contentType);
    }
    for (const [name, value] of JSON.parse(UTF8ToString(headers))) opts.headers[name] = value;
    const controllers = Module.rxFetchControllers || (Module.rxFetchControllers = {});
    let timedOut = false;
    let timer = 0;
    if (typeof AbortController !== "undefined") {
        const controller = new AbortController();
        controllers[id] = controller;
        opts.signal = controller.signal;
        if (timeoutMs > 0) timer = setTimeout(function() { timedOut = true; controller.abort(); }, timeoutMs);
    }
    const done = function(status, text, error, hs) {
        clearTimeout(timer);
        if (!(id in controllers) && !timedOut && typeof AbortController !== "undefined") return;  // cancelled
        delete controllers[id];
        Module.ccall("rx_fetch_done", null, ["number", "number", "string", "string", "string"],
                     [id, status, text, error, JSON.stringify(hs || [])]);
    };
    fetch(UTF8ToString(url), opts)
        .then(function(r) {
            const hs = [];
            if (r.headers && r.headers.forEach) r.headers.forEach(function(value, name) { hs.push([name.toLowerCase(), value]); });
            return r.text().then(function(t) { done(r.status, t, "", hs); });
        })
        .catch(function(e) { done(0, "", timedOut ? "timeout after " + timeoutMs + " ms" : String((e && e.message) || e)); });
});

EM_JS(void, rx_fetch_abort, (int id), {
    const controllers = Module.rxFetchControllers || {};
    const c = controllers[id];
    delete controllers[id];
    if (c) c.abort();
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

std::string FetchResponse::header(std::string_view name) const {
    auto same = [name](const std::string& other) {
        return std::ranges::equal(other, name, [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
    };
    auto it = std::ranges::find_if(headers, [&](const auto& h) { return same(h.first); });
    return it == headers.end() ? std::string{} : it->second;
}

void FetchHandle::cancel() const {
    if (!state_ || *state_) return;
    *state_ = true;
#ifdef __EMSCRIPTEN__
    pending().erase(id_);
    localAnswers().erase(id_);
    rx_fetch_abort(id_);
#endif
}

FetchHandle fetch(FetchRequest request, std::function<void(const FetchResponse&)> done) {
    auto cancelled = std::make_shared<bool>(false);
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
        return FetchHandle(std::move(cancelled), id);
    }
    json::Value headers = json::array();
    for (const auto& [name, value] : request.headers) headers.push(json::array({name, value}));
    rx_fetch_js(id, request.method.c_str(), request.url.c_str(), request.body.c_str(), request.contentType.c_str(),
                headers.dump().c_str(), static_cast<int>(request.timeout.count()));
    return FetchHandle(std::move(cancelled), id);
#else
    if (local) {
        done(*local);
        return FetchHandle(std::move(cancelled), 0);
    }
    auto where = splitUrl(request.url);
    if (!where) {
        done(FetchResponse{.status = 0, .body = {}, .error = where.error()});
        return FetchHandle(std::move(cancelled), 0);
    }
    net::Request out{.method = request.method, .body = request.body, .target = where->second};
    for (const auto& [name, value] : request.headers) {
        std::string key = name;
        for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        out.headers.insert_or_assign(std::move(key), value);
    }
    if (!request.body.empty() || request.method != "GET") out.headers.emplace("content-type", request.contentType);
    out.headers.emplace("accept", "application/json, text/plain, */*");
    std::optional<Clock::time_point> deadline;
    int seconds = 30;
    if (request.timeout.count() > 0) {
        deadline = Clock::now() + request.timeout;
        // The socket gives up a little later; the deadline above answers on time.
        seconds = static_cast<int>(std::chrono::ceil<std::chrono::seconds>(request.timeout).count()) + 1;
    }
    auto shared = std::make_shared<NativeRequest>();
    std::thread([shared, out = std::move(out), upstream = where->first, seconds] {
        auto reply = net::forward(out, upstream, seconds);
        FetchResponse answer;
        if (reply) {
            answer = FetchResponse{.status = reply->status, .body = std::move(reply->body), .error = {}};
            answer.headers.emplace_back("content-type", reply->contentType);
            for (auto& [name, value] : reply->headers) {
                for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                answer.headers.emplace_back(std::move(name), std::move(value));
            }
        } else {
            answer = FetchResponse{.status = 0, .body = {}, .error = reply.error().message};
        }
        std::scoped_lock lock(shared->mutex);
        shared->answer = std::move(answer);
    }).detach();
    pollAnswer(std::move(shared), cancelled, deadline, request.timeout, std::move(done));
    return FetchHandle(std::move(cancelled), 0);
#endif
}

std::string FetchState::errorText() const {
    if (loading) return {};
    if (!response.error.empty()) return "network error: " + response.error;
    if (!response.ok()) return "HTTP " + std::to_string(response.status);
    return {};
}

FetchState useFetch(const std::string& url) { return useFetch(FetchRequest{.method = "GET", .url = url}); }

FetchState useFetch(FetchRequest request) {
    auto [loaded, setLoaded] = useState(Loaded{});
    auto [reloads, setReloads] = useState(0);

    useEffect([request, setLoaded] {
        // Ignore the answer if the request changed or the component went away,
        // and cancel the request itself.
        auto alive = std::make_shared<bool>(true);
        setLoaded([](Loaded l) {
            l.loading = true;
            return l;
        });
        FetchHandle handle = fetch(request, [alive, setLoaded](const FetchResponse& r) {
            if (*alive) setLoaded(Loaded{.loading = false, .response = r});
        });
        return [alive, handle] {
            *alive = false;
            handle.cancel();
        };
    }, deps(request, reloads));

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

extern "C" EMSCRIPTEN_KEEPALIVE void rx_fetch_done(int id, int status, const char* body, const char* error,
                                                   const char* headers) {
    auto& map = rx::pending();
    auto it = map.find(id);
    if (it == map.end()) return;
    auto done = std::move(it->second);
    map.erase(it);
    rx::FetchResponse response{.status = status, .body = body, .error = error};
    if (auto list = rx::json::tryParse(headers); list && list->isArray())
        for (const auto& h : list->asArray())
            if (h.isArray() && h.size() == 2 && h[0].isString() && h[1].isString())
                response.headers.emplace_back(h[0].asString(), h[1].asString());
    done(response);
    rx::flushDom();  // the response arrived outside of a DOM event
}
#endif
