#pragma once

// Loading data over HTTP.
//
//   auto todos = useFetch("/api/todos");
//   if (todos.loading) return h("p", {}, {"Loading..."});
//   if (!todos.ok()) return h("p", {}, {"Error: " + todos.errorText()});
//   for (const auto& t : todos.json().asArray()) ...
//   todos.reload();                       // fetch again (e.g. after a POST)
//
//   rx::fetch({.method = "POST", .url = "/api/todos", .body = R"({"title":"Milk"})"},
//             [](const rx::FetchResponse& r) { ... });
//
//   // Headers, a deadline, and cancelling (the callback then never runs):
//   auto req = rx::fetch({.url = "/api/me", .headers = {{"Authorization", "Bearer " + token}}, .timeout = 5s},
//                        [](const rx::FetchResponse& r) { ... r.header("etag") ... });
//   req.cancel();
//   auto me = useFetch({.url = "/api/me", .headers = {{"Authorization", "Bearer " + token}}});
//
// In the browser this is the page's fetch(). In the terminal build a request
// needs a full http:// URL ("http://127.0.0.1:8090/api/todos"; no https);
// it runs on its own thread and the answer arrives on the UI thread at the
// next Root::flush() (TerminalApp flushes on its own).

#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/json.hpp"

namespace rx {

using FetchHeaders = std::vector<std::pair<std::string, std::string>>;

struct FetchRequest {
    std::string method = "GET";
    std::string url{};
    std::string body{};
    std::string contentType = "application/json";  // sent with a body
    FetchHeaders headers{};                         // {{"Authorization", "Bearer ..."}, {"X-Request-Id", id}}
    // Give up after this long: the answer is then status 0 with error "timeout
    // after N ms". 0 means no deadline of our own (the terminal build still
    // gives up after 30 s; a local fetch answers at once).
    std::chrono::milliseconds timeout{0};
    bool operator==(const FetchRequest&) const = default;
};

struct FetchResponse {
    int status = 0;     // 0 if the request failed (network error)
    std::string body{};
    std::string error{};  // network error message
    FetchHeaders headers{};  // response headers, names in lower case

    // A response header by name, any case; "" if there is none.
    std::string header(std::string_view name) const;
    bool ok() const { return error.empty() && status >= 200 && status < 300; }
    // Parses the body; throws json::Error for invalid JSON.
    json::Value json() const { return json::parse(body); }
    // The same without exceptions (a server can always send broken JSON).
    std::expected<json::Value, json::Error> tryJson() const { return json::tryParse(body); }
    bool operator==(const FetchResponse&) const = default;
};

// A request in flight, returned by fetch(). cancel() (or calling the handle,
// so it can be an effect's cleanup) means `done` never runs; in the browser
// the request is aborted too. Dropping the handle doesn't cancel anything.
class FetchHandle {
public:
    FetchHandle() = default;
    void cancel() const;
    void operator()() const { cancel(); }
    bool cancelled() const { return state_ && *state_; }

private:
    friend FetchHandle fetch(FetchRequest, std::function<void(const FetchResponse&)>); // this is very dangerous, but the handle is only used in the effect's cleanup, so it should be fine
    explicit FetchHandle(std::shared_ptr<bool> state, int id) : state_(std::move(state)), id_(id) {}
    std::shared_ptr<bool> state_;  // true once cancelled
    int id_ = 0;                   // browser: the request to abort
};

// Starts a request; `done` runs when the response arrives, and the page is
// re-rendered after it.
FetchHandle fetch(FetchRequest request, std::function<void(const FetchResponse&)> done);

// A "server" inside the app: requests go to `handler` first; if it returns a
// response, that is the answer (delivered later, like a network one in the
// browser; right away in the terminal build), otherwise the request goes to
// the network. Used for static sites with no backend, and for tests.
// Pass {} to remove it.
using LocalFetch = std::function<std::optional<FetchResponse>(const FetchRequest&)>;
void setLocalFetch(LocalFetch handler);

struct FetchState {
    bool loading = true;
    FetchResponse response{};
    std::function<void()> reload{};

    bool ok() const { return !loading && response.ok(); }
    json::Value json() const { return response.json(); }
    std::expected<json::Value, json::Error> tryJson() const { return response.tryJson(); }
    // "404", "network error: ..." — for showing to the user.
    std::string errorText() const;
};

// GETs `url` when the component mounts and whenever `url` changes. While a new
// response is loading the previous one stays in `response`. A request that is
// replaced (new url, reload) or whose component unmounts is cancelled.
FetchState useFetch(const std::string& url);
// The same with headers, a timeout, another method: refetches when any of it changes.
FetchState useFetch(FetchRequest request);

}  // namespace rx
