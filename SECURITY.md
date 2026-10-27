# Безопасность ReactX

[English summary below](#english)

## Поддерживаемые версии

Пока `MAJOR = 0`, исправления выходят только для **последнего релиза** (`reactx update` переводит проект на него). Старые версии не исправляются — обновитесь.

## Как сообщить об уязвимости

**Не открывайте публичный issue.** Напишите приватно: на GitHub — **Security → Report a vulnerability** в репозитории [sargisis/ReactX](https://github.com/sargisis/ReactX/security/advisories/new). Опишите, что можно сделать, с какими входными данными и в какой версии; пример кода или входа, на котором падает, сильно ускоряет исправление.

Мы ответим в течение недели, исправим в патч-релизе и упомянем вас в CHANGELOG (если вы не против).

## Что считается уязвимостью

ReactX — фреймворк для интерфейсов. Опасны данные, которые приходят **не от разработчика приложения**:

| Откуда данные | Что должно быть верно | Уязвимость, если… |
|---|---|---|
| Текст и атрибуты из пользовательских данных в браузере | текст — всегда текст, ссылки `javascript:` блокируются | данные пользователя выполняются как код (XSS) |
| JSON с сервера (`json::tryParse`, `useFetch`) | ошибка возвращается как значение | краш, зависание, чтение за границей памяти |
| Ответы бэкенда за `reactx dev --proxy` | ограничение 64 МБ и 30 с, ошибка — 502/504 | краш или неограниченный рост памяти |
| HTTP-запросы к `HttpServer` / `reactx_serve` | файлы только из своей папки, ограничения на размер | выход за пределы папки, краш от запроса |
| `reactx.json` проекта | понятная ошибка с местом | краш CLI |

**Не уязвимость**, а известные ограничения:

- `HttpServer`, `reactx_serve`, `reactx dev` — **серверы для разработки**: без HTTPS, keep-alive и защиты от перегрузки, по умолчанию слушают только `127.0.0.1`. Не выставляйте их в интернет; `--host 0.0.0.0` открывает их локальной сети.
- Код самого приложения доверенный: если разработчик сам передаёт строку в `onclick` как атрибут или вставляет чужой HTML — это его выбор.
- Нехватка памяти при очень больших, но корректных данных (дерево на миллионы элементов).

## Как проект проверяется

На каждый push и pull request ([.github/workflows/pages.yml](.github/workflows/pages.yml)):

- **ASan + UBSan + LeakSanitizer** — выход за границы, use-after-free, неопределённое поведение, утечки; первая же находка останавливает прогон.
- **ThreadSanitizer** — гонки потоков (сервер, `reactx dev`, сетевой `fetch` в терминале).
- **Фаззинг** (`tests/fuzz_parsers.cpp`) — минута случайных входов для JSON, HTTP-запросов и ответов, URL и маршрутов под ASan + UBSan, каждый раз с новым зерном; вход, на котором упало, сохраняется.
- **Статический анализ** (`gcc -fanalyzer`) кода, который читает внешние данные; любая находка — ошибка сборки.
- Hardening стандартной библиотеки (проверка границ в `v[i]`), контракты C++26 (`REACTX_PRE`), `-Wall -Wextra -Wpedantic -Werror`.

Это не доказывает отсутствие ошибок, но ловит большинство из них до релиза.

## English

Report vulnerabilities privately via **Security → Report a vulnerability** on GitHub, not in a public issue; only the latest release gets fixes while the version is 0.x. In scope: anything that untrusted data (user text in the browser, JSON from a server, replies behind `reactx dev --proxy`, requests to `HttpServer`, a project's `reactx.json`) can use to run code, crash, hang, read out of bounds or grow memory without limit. Out of scope: the development servers exposed to the internet (they are not meant for it), and code the app's developer writes themselves. Every push runs ASan + UBSan + LSan, TSan, a minute of fuzzing of the input parsers and `gcc -fanalyzer` on them.
