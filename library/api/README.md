# reactx::api

A JSON API on [ReactX](https://github.com/sargisis/ReactX)'s `rx::net::HttpServer`: routes under a prefix with middleware (CORS, rate limits, logging, errors as JSON), request bodies checked with [`reactx::forms`](../forms/README.md), answers in one shape. For servers (native builds).

На русском: [README.ru.md](README.ru.md).

```cpp
#include "rx/api.hpp"
using namespace rx;

int main() {
    net::HttpServer server("127.0.0.1", 8080);
    api::Router api(server, "/api");
    api.use(api::catchErrors()).use(api::log()).use(api::rateLimit({.requests = 120}));

    const auto newTodo = forms::JsonSchema().text("title", {forms::required(), forms::maxLength(200)});

    api.get("/todos", [](const net::Request&) { return api::ok(allTodos()); });
    api.post("/todos", [&](const net::Request& r) {
        auto body = api::body(r, newTodo);
        if (!body) return body.error();               // 400 broken JSON, 415 not JSON, 422 {"errors": {...}}
        return api::ok(addTodo((*body)["title"].asString()), 201);
    });
    api.del("/todos/:id", [](const net::Request& r) {
        auto id = api::intParam(r, "id");
        if (!id) return id.error();                   // 400 {"error": "\"id\" must be a whole number"}
        return removeTodo(*id) ? api::noContent() : api::error(404, "no such todo");
    });
    server.run();
}
```

## Add it to a project

It comes with ReactX (since 1.5). Link it to the server's target (a server is native, not part of a web app):

```cmake
target_link_libraries(my_server PRIVATE reactx::api)
```

## Answers

| | |
|---|---|
| `api::ok(json, status = 200)` | the JSON |
| `api::error(status, "message")` | `{"error": "message"}` |
| `api::invalid(errors)` | 422 `{"errors": {"field": "message"}}` — what `ui::useForm` can show next to the fields |
| `api::noContent()` | 204 |

## Requests

- `api::body(request, schema)` — the JSON body checked against a `forms::JsonSchema`: `std::expected<json::Value, Response>`, the error being the answer to send (400, 415 or 422). An empty schema only parses.
- `api::intParam(request, "id")` — a path parameter as a whole number, or a 400.
- `api::clientAddress(request, trustProxy)` — the client's IP: `Request::remote`, or the first `X-Forwarded-For` address behind your own reverse proxy (`trustProxy = true`; never on a server that clients reach directly — they can write that header).

## Middleware

`api.use(m)` applies to the routes added after it, outermost first. A middleware is `Response(const Request&, const Next&)`; call `next(request)` to go on.

| | |
|---|---|
| `catchErrors()` | an exception becomes JSON: `json::Error` (a field of the wrong type) → 400, anything else → 500 `{"error": "internal error"}`, the message to stderr, not to the client |
| `cors({.origins, .methods, .headers, .credentials, .maxAge})` | lets pages on other sites call the API: answers preflight `OPTIONS` (the router adds those routes) and adds `Access-Control-*`. Add the origins' hosts to `server.allowHosts(...)` too: the server refuses POST/PUT/… from foreign origins otherwise |
| `rateLimit({.requests = 60, .per = 60s, .key})` | 429 with `Retry-After` past the limit; per client address by default, `.key` for another key (an API token) |
| `log(sink)` | `POST /api/todos 201 3 ms` per request, to stderr or your function |

Your own, e.g. a token check:

```cpp
api.use([](const net::Request& r, const api::Next& next) {
    if (r.header("authorization") != "Bearer " + token) return api::error(401, "unauthorized");
    return next(r);
});
```

`HttpServer` is made for development and small servers (a thread per connection); put a reverse proxy (nginx, Caddy) with HTTPS in front of it on the internet.

## License

MIT or Apache-2.0, like ReactX.
