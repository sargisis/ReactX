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
// The network is used in the browser build; in the terminal build requests
// only reach a setLocalFetch() handler (otherwise they fail with an error).

#include <expected>
#include <functional>
#include <optional>
#include <string>

#include "reactx/json.hpp"

namespace rx {

struct FetchRequest {
    std::string method = "GET";
    std::string url{};
    std::string body{};
    std::string contentType = "application/json";
};

struct FetchResponse {
    int status = 0;     // 0 if the request failed (network error)
    std::string body{};
    std::string error{};  // network error message

    bool ok() const { return error.empty() && status >= 200 && status < 300; }
    // Parses the body; throws json::Error for invalid JSON.
    json::Value json() const { return json::parse(body); }
    // The same without exceptions (a server can always send broken JSON).
    std::expected<json::Value, json::Error> tryJson() const { return json::tryParse(body); }
    bool operator==(const FetchResponse&) const = default;
};

// Starts a request; `done` runs when the response arrives, and the page is
// re-rendered after it.
void fetch(FetchRequest request, std::function<void(const FetchResponse&)> done);

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
// response is loading the previous one stays in `response`.
FetchState useFetch(const std::string& url);

}  // namespace rx
