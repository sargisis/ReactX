# Changelog

Все заметные изменения ReactX. Версии — [Semantic Versioning](https://semver.org/lang/ru/): `MAJOR.MINOR.PATCH`, теги в git — `vMAJOR.MINOR.PATCH`. Пока `MAJOR = 0`, API может меняться: несовместимые изменения поднимают `MINOR`, исправления — `PATCH`.

## [Не выпущено]

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
