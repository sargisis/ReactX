# reactx::api

JSON API поверх `rx::net::HttpServer` из [ReactX](https://github.com/sargisis/ReactX): маршруты под префиксом с middleware (CORS, ограничение частоты, логи, ошибки в JSON), проверка тела запроса через [`reactx::forms`](../forms/README.ru.md), ответы одного вида. Для серверов (нативные сборки).

In English: [README.md](README.md).

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
        if (!body) return body.error();               // 400 сломанный JSON, 415 не JSON, 422 {"errors": {...}}
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

## Подключение

Библиотека идёт вместе с ReactX (с версии 1.5). Подключите её к цели сервера (сервер нативный, он не часть web-приложения):

```cmake
target_link_libraries(my_server PRIVATE reactx::api)
```

## Ответы

| | |
|---|---|
| `api::ok(json, status = 200)` | сам JSON |
| `api::error(status, "сообщение")` | `{"error": "сообщение"}` |
| `api::invalid(errors)` | 422 `{"errors": {"поле": "сообщение"}}` — то, что `ui::useForm` покажет рядом с полями |
| `api::noContent()` | 204 |

## Запросы

- `api::body(request, schema)` — JSON-тело, проверенное `forms::JsonSchema`: `std::expected<json::Value, Response>`, ошибка — готовый ответ (400, 415 или 422). Пустая схема только разбирает JSON.
- `api::intParam(request, "id")` — параметр пути как целое число или 400.
- `api::clientAddress(request, trustProxy)` — IP клиента: `Request::remote` или первый адрес из `X-Forwarded-For` за вашим обратным прокси (`trustProxy = true`; никогда — на сервере, куда клиенты ходят напрямую: этот заголовок они могут написать сами).

## Middleware

`api.use(m)` действует на маршруты, добавленные после него, внешний — первым. Middleware — это `Response(const Request&, const Next&)`; `next(request)` передаёт дальше.

| | |
|---|---|
| `catchErrors()` | исключение становится JSON: `json::Error` (поле не того типа) → 400, всё остальное → 500 `{"error": "internal error"}`, текст — в stderr, а не клиенту |
| `cors({.origins, .methods, .headers, .credentials, .maxAge})` | разрешает страницам других сайтов вызывать API: отвечает на preflight `OPTIONS` (роутер сам добавляет эти маршруты) и ставит `Access-Control-*`. Хосты этих источников добавьте и в `server.allowHosts(...)`: иначе сервер отклонит POST/PUT/… с чужих источников |
| `rateLimit({.requests = 60, .per = 60s, .key})` | 429 с `Retry-After` сверх лимита; по адресу клиента, `.key` — свой ключ (API-токен) |
| `log(sink)` | `POST /api/todos 201 3 ms` на каждый запрос — в stderr или в вашу функцию |

Свой, например проверка токена:

```cpp
api.use([](const net::Request& r, const api::Next& next) {
    if (r.header("authorization") != "Bearer " + token) return api::error(401, "unauthorized");
    return next(r);
});
```

`HttpServer` сделан для разработки и небольших серверов (поток на соединение); в интернете ставьте перед ним обратный прокси (nginx, Caddy) с HTTPS.

## Лицензия

MIT или Apache-2.0, как ReactX.
