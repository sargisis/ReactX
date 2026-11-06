# reactx::auth

Вход для сервера на [ReactX](https://github.com/sargisis/ReactX): хеши паролей, сессии в cookie с `HttpOnly`, CSRF-токены и middleware для [`reactx::api`](../api/README.ru.md). Для серверов (нативные сборки).

In English: [README.md](README.md).

```cpp
#include "rx/auth.hpp"
using namespace rx;

auth::Sessions sessions({.secure = true});   // .secure: сайт работает по HTTPS

api.post("/login", [&](const net::Request& r) {
    auto body = api::body(r, loginSchema);
    if (!body) return body.error();
    auto user = findUser((*body)["email"].asString());
    if (!user || !auth::verifyPassword((*body)["password"].asString(), user->passwordHash))
        return api::error(401, "wrong email or password");
    auto res = api::ok(json::object({{"name", user->name}}));
    sessions.start(res, user->id);           // Set-Cookie: sid=...; HttpOnly; SameSite=Lax
    return res;
});

api.use(auth::requireLogin(sessions)).use(auth::csrf(sessions));   // маршрутам ниже нужна сессия
api.get("/me", [&](const net::Request& r) {
    auto s = sessions.get(r);
    return api::ok(json::object({{"user", s->user}, {"csrf", s->csrf}}));
});
api.post("/logout", [&](const net::Request& r) {
    auto res = api::noContent();
    sessions.end(r, res);
    return res;
});
```

## Подключение

Библиотека идёт вместе с ReactX (с версии 1.5). Подключите её к цели сервера:

```cmake
target_link_libraries(my_server PRIVATE reactx::auth)
```

## Пароли

```cpp
std::string stored = auth::hashPassword(password);      // "pbkdf2-sha256$600000$<соль>$<хеш>": храните это, а не пароль
bool ok = auth::verifyPassword(attempt, stored);          // сравнение за постоянное время
if (ok && auth::needsRehash(stored)) save(auth::hashPassword(attempt));   // после увеличения числа раундов
```

PBKDF2-HMAC-SHA256 со случайной солью 16 байт и 600 000 раундов (рекомендация OWASP, 2023): около трети секунды на пароль — специально, подбор так же медленный. Испорченное или подделанное сохранённое значение никогда не совпадает.

## Сессии

| | |
|---|---|
| `Sessions sessions({...})` | `.cookie = "sid"`, `.idle = 2h` (заканчивается без запросов), `.lifetime = 7 дней`, `.secure`, `.sameSite = "Lax"`, `.path = "/"` |
| `sessions.start(response, user, data)` | вход: каждый раз новый случайный id сессии (id, подброшенный до входа, бесполезен) и его `Set-Cookie` |
| `sessions.get(request)` | `Session` (`id`, `user`, `data`, `csrf`, `started`) или ничего |
| `sessions.update(request, data)` | заменяет данные сессии |
| `sessions.end(request, response)` | выход и очистка cookie |
| `sessions.endAll(user)` | «выйти везде», например после смены пароля |

Сессии живут в памяти (заканчиваются при остановке сервера) и безопасны для потоков сервера.

## Middleware

- `auth::requireLogin(sessions)` — 401 `{"error": "sign in first"}` без живой сессии.
- `auth::csrf(sessions)` — запрос, который что-то меняет (не GET/HEAD/OPTIONS), с cookie сессии должен прислать токен сессии в `X-CSRF-Token`, иначе 403. Отдайте странице `session.csrf` (например, в `GET /api/me`) и присылайте обратно: `rx::fetch({.method = "POST", ..., .headers = {{"X-CSRF-Token", csrf}}}, ...)`. Cookie с `SameSite` и проверка Origin в `HttpServer` уже останавливают большинство межсайтовых запросов; этот закрывает остальное.

## Ещё

`auth::randomToken(bytes)` (URL-безопасный случайный текст от ядра — ссылки-приглашения, API-ключи), `auth::sha256Hex`, `auth::hmacSha256Hex` (подпись вебхуков), `auth::constantTimeEquals`.

## Лицензия

MIT или Apache-2.0, как ReactX.
