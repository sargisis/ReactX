# Changelog

Все заметные изменения ReactX. Версии — [Semantic Versioning](https://semver.org/lang/ru/): `MAJOR.MINOR.PATCH`, теги в git — `vMAJOR.MINOR.PATCH`. Пока `MAJOR = 0`, API может меняться: несовместимые изменения поднимают `MINOR`, исправления — `PATCH`.

## [Не выпущено]

### Добавлено

- **Готовый `reactx` в GitHub Releases**: при пуше тега `vX.Y.Z` workflow `release.yml` проверяет, что тег совпадает с версией в `CMakeLists.txt`, собирает статические `reactx` и `reactx_serve` для Linux x86-64 (работают на любом дистрибутиве), создаёт релиз с разделом из CHANGELOG и публикует Docker-образ. Для веб-проектов теперь достаточно Emscripten, CMake и Ninja — GCC 16 нужен только терминальным.
- **Docker-образ** `ghcr.io/sargisis/reactx` (`Dockerfile` в репозитории): GCC 16, CMake, Ninja, Emscripten с прогретым кэшем и `reactx`; пользователь `dev` для запуска не от root.
- **devcontainer**: `.devcontainer/` для работы над ReactX в VS Code и GitHub Codespaces; `reactx new` кладёт такой же в каждый проект — с образом той же версии.
- **`reactx dev --proxy [ПРЕФИКС=]URL`** — запросы под префиксом (по умолчанию `/api`) уходят на свой бэкенд (Flask, Django, Express, …), ответ возвращается как есть; можно задать и в `reactx.json`: `"web": {"proxy": {"/api": "http://127.0.0.1:5000"}}`.
- **`reactx dev --host 0.0.0.0`** — сервер доступен из-за пределов контейнера и с других устройств в сети.
- **`HttpServer::proxy(prefix, upstream)`, `rx::net::forward`, `rx::net::parseUpstream`**: небольшой HTTP/1.1-клиент для прокси — `Content-Length`, `chunked` и ответы до закрытия соединения; 502 с причиной, если бэкенд недоступен, 504, если не ответил.
- `Request::target` — путь запроса как он пришёл, без раскодирования (`/api/a%2Fb?x=1`).

### Исправлено

- Скачанный (собранный на другой машине) `reactx` передавал CMake путь к компилятору машины сборки и ломал первую сборку терминального проекта; теперь путь передаётся, только если такой файл есть.
- Поля `rx::net::Request`, `Response` и `HttpError` получили значения по умолчанию: `{.method = "GET", .path = "/"}` компилируется под `-Wextra -Werror`, как и props-структуры.
- Строка статуса знает коды 202, 206, 303, 307, 308, 401, 409, 410, 415, 422, 429, 501–504 (раньше — «Unknown»).

## [0.5.0] — 2026-10-27

Лицензия MIT / Apache-2.0, новые хуки (`useKeyboard`, `useReducer`, `useTimeout`, `useInterval`), события фокуса и прокрутки, SVG в браузере, `reactx update` и документы для AI-помощников. Приложения обновляются без правок; свой бэкенд, который разбирает `EventData` или значения props через `std::visit`, должен обработать новые `ScrollEvent` / `ScrollHandler`.

### Добавлено

- **Лицензия**: ReactX распространяется на выбор пользователя по MIT или Apache-2.0 (`LICENSE-MIT`, `LICENSE-APACHE`), как Rust. До этого лицензии не было, и формально использовать код было нельзя.
- **`useKeyboard`** — сочетания клавиш для всего приложения (Ctrl+Z, «/», Escape), где бы ни был фокус: обработчик вызывается раньше `onKey` элемента, `return true` забирает клавишу (в браузере — и действие по умолчанию). Пока фокус в поле ввода, клавиши набора текста сюда не приходят. В терминале — через `TerminalApp`.
- **`useReducer`** — состояние, которое меняют действия: `auto [s, dispatch] = useReducer(reducer, initial)`; берётся самый свежий reducer.
- **`useTimeout` / `useInterval`** — один раз через время / с периодом, в потоке интерфейса; `std::nullopt` выключает, размонтирование отменяет.
- **События `onFocus`, `onBlur`** (браузер; в терминале — при переходе фокуса по Tab и стрелкам) и **`onScroll`** с `ScrollEvent` (`top`, `scrollHeight`, `clientHeight`, …, `nearBottom()`).
- **SVG** в браузере: `svg`, `rect`, `circle`, `path`, `g`, … создаются в пространстве имён SVG; `svg:text`, `svg:a` — для тегов, которые есть и в HTML.
- `TerminalRenderer::editingText()` — фокус в поле ввода.
- **Для AI-помощников**: `llms.txt` (по формату llmstxt.org: полный пример программы, API, события, правила, частые ошибки; публикуется на сайте) и `AGENTS.md` (сборка, проверки как в CI, правила, известные ловушки GCC 16, релизы). `reactx new` кладёт в проект свой `AGENTS.md` с командами и правилами для веба или терминала.
- **`reactx update`** — переводит проект на новейший релиз ReactX: теги из репозитория в `reactx.json` (`git ls-remote`), выбор по SemVer (`v0.10.0` новее `v0.9.0`, `-rc` пропускаются), ссылка на CHANGELOG и предупреждение, если новая MINOR (пока MAJOR = 0) может потребовать правок. `--check` — только проверить, `--to vX.Y.Z` — конкретная версия; без `--to` версия не понижается.

### Исправлено

- После `reactx update` / `add` / `remove` проект собирался со старым содержимым `reactx.json`: `reactx build` / `dev` настраивали CMake только один раз. Теперь, если `reactx.json` новее кэша CMake, они настраивают заново; шаблон `reactx new` к тому же объявляет `reactx.json` зависимостью настройки (для запуска `cmake` вручную).

## [0.4.0] — 2026-10-26

Исправления хуков из 0.3.0 и браузерной сборки; DSL для элементов. Если вы на `v0.3.0` — обновитесь: в нём не собирается ни одна веб-программа.

### Исправлено

- **Браузерная сборка снова собирается**: в `v0.3.0` `useDebounce` вызывал `detail::setTimeoutJs` до его объявления, и ни одна веб-программа (в том числе проекты из `reactx new`, которые подключают тег `v0.3.0`) не компилировалась.
- **`useDebounce` без потоков**: нативно он запускал отсоединённый `std::jthread`, который менял состояние параллельно с отрисовкой (гонка данных, ThreadSanitizer в CI). Теперь таймер срабатывает в потоке интерфейса: в браузере — `setTimeout`, нативно — на ближайшем `Root::flush()` (`TerminalApp` вызывает его каждые 100 мс). Новое значение или размонтирование отменяет таймер.
- **`useWindowSize` обновляется**: в браузере — по событию `resize`, в терминале — когда `TerminalApp` замечает новый размер. Раньше размер читался один раз.
- **`useLocalStorage`**: числа читаются через `rx::parseNumber` (без `std::stoll` и исключений), сохранённое значение читается один раз при монтировании, неподдерживаемый тип — ошибка компиляции с объяснением, а не молча несохраняемое значение.
- **Шаблон `reactx new` (веб) компилируется**: `h(Home)` для компонента со структурой props берёт props по умолчанию (`P{}`), как маршруты `router::routes({{"/", h(Home)}})`.

### Добавлено

- **Fluent DSL** (`reactx/dsl.hpp`): `tag::div.cls("card")("текст", tag::button.onClick(f)("OK"))`, свои теги — `"my-tag"_tag`.

## [0.3.0] — 2026-10-26

Типизированные props, C++26 рефлексия и контракты, новые хуки (`useDebounce`, `useLocalStorage`, `useWindowSize`).

### Добавлено

- **Новые хуки**:
  - `useDebounce(value, delay)` — задержка обновления состояния с безопасной отменой при unmount (WASM setTimeout / C++20 `std::jthread` + `std::stop_token`).
  - `useLocalStorage(key, defaultVal)` — сохранение состояния в `localStorage` (браузер) и в памяти (натив/CLI).
  - `useWindowSize()` — получение актуальных размеров окна браузера или геометрии терминала (`WindowSize`).
- **Props-структуры**: компонент принимает свою структуру — `Element Counter(const CounterProps&)`, `h(Counter, {.start = 5})`. Опечатки в именах полей и неверные типы ловит компилятор; работает везде, включая браузер. Поле `key` — ключ элемента, поле `children` получает детей (`h(Card, {.title = "Hi"}, {...})`), `memo<Counter>` сравнивает структуры их `operator==`. Шаблоны `reactx new` переведены на типизированные props.
- **Рефлексия C++26** (`<meta>`, GCC 16): `rx::json::to(значение)` и `rx::json::from<T>(json)` для обычных структур — поля становятся ключами, типы проверяются, ошибка показывает путь (`field "dependencies": "fmt": missing field "rev"`). Поддерживаются числа, строки, `bool`, `std::optional`, последовательности, словари со строковыми ключами, вложенные структуры. Подключается к цели через `reactx::reflection`.
- CLI читает `reactx.json` как структуру `Manifest` через `json::from`: забытый `rev` у зависимости теперь виден сразу, а не ошибкой CMake при сборке.
- `reactx/parse.hpp`: `rx::parseNumber<T>` (`std::from_chars` + `std::expected`, без исключений и локалей) и `rx::parsePort`.
- `TerminalRenderer::setUnicode`: рамки ASCII (`+-|`) для терминалов не в UTF-8; `TerminalApp` выбирает по `std::text_encoding`.
- CLI: чтение и запись файлов через `std::expected`, `reactx.json` — цепочка `and_then` (прочитать → разобрать → проверить), обход исходников в `reactx dev` — `std::generator`.

### Изменено

- **Нативная сборка требует GCC 16** (стандартная библиотека C++26); CI — образ `gcc:16`. CMake сразу говорит, если компилятор старше.
- **Контракты C++26**: на GCC 16 ReactX включает `-fcontracts`, и `REACTX_PRE` становится `contract_assert`. Без контрактов нарушение печатает стек вызовов (`<stacktrace>`) и останавливается в отладчике (`<debugging>`).
- `net::parseRequestHead` возвращает `std::expected<Request, HttpError>` с причиной; новый `net::bodyLength` проверяет `Content-Length`.
- `Props::values()`, `Request::headers` и props в `TextRenderer` — `std::flat_map` вместо `std::map`; `Props::has` / `find` / `get` принимают `std::string_view`.
- Терминал: ячейка экрана — `std::inplace_vector<char, 4>` (без кучи), экран — `std::mdspan` над одним массивом вместо `vector<vector<Cell>>`.
- Вывод CLI, `reactx_serve`, `api_server` и примеров — `std::print`; `std::format` вместо `ostringstream`.

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

[Не выпущено]: https://github.com/sargisis/ReactX/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/sargisis/ReactX/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/sargisis/ReactX/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/sargisis/ReactX/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/sargisis/ReactX/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/sargisis/ReactX/releases/tag/v0.1.0
