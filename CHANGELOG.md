# Changelog

Все заметные изменения ReactX. Версии — [Semantic Versioning](https://semver.org/lang/ru/): `MAJOR.MINOR.PATCH`, теги в git — `vMAJOR.MINOR.PATCH`. Пока `MAJOR = 0`, API может меняться: несовместимые изменения поднимают `MINOR`, исправления — `PATCH`.

## [Не выпущено]

Стандартная библиотека C++23/C++26 — всё, что в ней есть, и ничего самописного вместо того, чего нет.

### Изменено

- **Нативная сборка требует GCC 16** (стандартная библиотека C++26); CI — образ `gcc:16`. CMake сразу говорит, если компилятор старше. `reactx` собирает нативные проекты тем же компилятором, которым собран сам (или `CXX`).
- **Контракты C++26**: на GCC 16 ReactX включает `-fcontracts`, и `REACTX_PRE` становится `contract_assert`. Без контрактов нарушение печатает ещё и стек вызовов (`<stacktrace>`) и останавливается в отладчике (`<debugging>`) — где они есть.
- `net::parseRequestHead` возвращает `std::expected<Request, HttpError>` с причиной; новый `net::bodyLength` проверяет `Content-Length` (400 — не число, 413 — слишком большое; раньше `-5` читалось как огромное число).
- `Props::values()`, `Request::headers` и props в `TextRenderer` — `std::flat_map` вместо `std::map`; `Props::has` / `find` / `get` принимают `std::string_view`.
- Терминал: ячейка экрана — `std::inplace_vector<char, 4>` (без кучи), экран — `std::mdspan` над одним массивом вместо `vector<vector<Cell>>`.
- Вывод CLI, `reactx_serve`, `api_server` и примеров — `std::print`; `std::format` вместо `ostringstream` (числа в props печатаются в кратчайшей точной форме: `3.14159265`, а не `3.14159`).

### Добавлено

- **Рефлексия C++26** (`<meta>`, GCC 16): `rx::json::to(значение)` и `rx::json::from<T>(json)` для обычных структур — поля становятся ключами, типы проверяются, ошибка показывает путь (`field "dependencies": "fmt": missing field "rev"`). Поддерживаются числа, строки, `bool`, `std::optional`, последовательности, словари со строковыми ключами, вложенные структуры. Подключается к цели через `reactx::reflection` (в браузере рефлексии пока нет).
- CLI читает `reactx.json` как структуру `Manifest` через `json::from`: забытый `rev` у зависимости теперь виден сразу, а не ошибкой CMake при сборке.
- `reactx/parse.hpp`: `rx::parseNumber<T>` (`std::from_chars` + `std::expected`, без исключений и локалей) и `rx::parsePort`. `--port abc` в `reactx dev`, `reactx_serve` и `api_server` теперь даёт понятную ошибку, а не падение в `std::stoi`.
- `TerminalRenderer::setUnicode`: рамки ASCII (`+-|`) для терминалов не в UTF-8; `TerminalApp` выбирает по `std::text_encoding`.
- CLI: чтение и запись файлов через `std::expected`, `reactx.json` — цепочка `and_then` (прочитать → разобрать → проверить), обход исходников в `reactx dev` — `std::generator`.

## [0.2.0] — 2026-10-26

Безопасность и современный C++.

### Изменено

- **Режим C++26** (было C++20): нужен GCC 15+ или Clang 19+ (Emscripten 6+); шаблон `reactx new` тоже переходит на C++26. Используем весь C++23 и реализованные части C++26.
- CI на GCC 15 (контейнер `gcc:15`) вместо GCC 14.

### Добавлено

- **Hardening стандартной библиотеки** (`REACTX_HARDENING`, включено): проверка границ в `v[i]` / `s[i]` / `front()`.
- **`REACTX_PRE`** — предусловия, готовые к контрактам C++26 (`reactx/contract.hpp`); стоят в маршрутах `HttpServer`, `router::navigate`, хуках и `mountDom`.
- **`std::expected`**: `json::tryParse`, `FetchResponse::tryJson` / `FetchState::tryJson`, `HttpServer::tryStart`; CLI сообщает о сломанном `reactx.json`, а не падает.
- **Санитайзеры в CI**: ASan + UBSan + LeakSanitizer (фатально при первой ошибке и утечке) и ThreadSanitizer; публикация сайта только после них.
- **Тесты граничных случаев reconciler** (`tests/test_reconciler.cpp`, 24 теста): пакетные обновления, setState во время рендера и после размонтирования, порядок очистки эффектов, вложенные провайдеры контекста, сравнение props в `memo`.
- Раздел документации «Стандарт C++ и безопасность».

## [0.1.0] — 2026-10-26

Первый релиз.

### Добавлено

- **Ядро в духе React на C++20**: элементы `h()`, компоненты-функции, props, дети, фрагменты, ключи; reconciler, который переиспользует узлы и переставляет их по ключам; пакетные обновления.
- **Хуки**: `useState`, `useEffect` (deps и очистка), `useRef`, `useMemo`, `useCallback`; **контекст** (`createContext`, `provide`, `useContext`) и **`memo<Component>`**.
- **События с данными**: `Callback`, `KeyHandler` (`KeyEvent`), `InputHandler`; всплытие к родителям.
- **Модуль C++20** `import reactx;` (CMake 3.28+, Ninja).
- **Терминальный бэкенд**: ANSI-отрисовка с перерисовкой только изменившихся строк, рамки, цвета, фокус, ввод с клавиатуры; раскладка **Flexbox на Yoga**; `TerminalApp`.
- **Браузерный бэкенд** (WebAssembly, Emscripten): `DomRenderer`, `mountDom`, управляемые поля ввода, делегирование событий, `data-rx-self` / `data-rx-trap` / `data-rx-link`.
- **Компоненты `rx::ui`** с темой — один код для терминала и браузера: `button`, `input`, `checkbox`, `tabs`, `modal`, `list` / `listItem`.
- **Роутер** `rx::router`: маршруты с параметрами и `*`, ссылки без перезагрузки, History API, базовый путь из `<base href>`.
- **Данные**: `useFetch`, `fetch`, `setLocalFetch` («сервер внутри приложения»); свой **JSON** `rx::json`.
- **HTTP-сервер на C++** `rx::net::HttpServer`: файлы с правильными типами, маршруты с параметрами, SPA-режим, URL-префикс и страница 404; программы `reactx_serve` и `api_server` (REST API задач).
- **CLI `reactx`**: `new` (веб или `--terminal`), `dev` (встроенный сервер, пересборка и автообновление страницы), `build`, `add` / `remove` зависимостей; `reactx_add_app()` для своих CMake-проектов.
- **Сайт с документацией** на ReactX: https://sargisis.github.io/ReactX/, публикуется GitHub Actions через ветку `gh-pages`.
- Версия в одном месте (`project(ReactX VERSION ...)`) → `reactx/version.hpp`, `rx::version()`; новые проекты из `reactx new` подключают ReactX по тегу релиза.

[Не выпущено]: https://github.com/sargisis/ReactX/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/sargisis/ReactX/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/sargisis/ReactX/releases/tag/v0.1.0
