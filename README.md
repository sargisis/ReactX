<div align="center">

<img src=".github/assets/logo.svg" width="88" alt="ReactX">

# ReactX

**React на C++26. Один и тот же код компонентов — в терминале и в браузере.**

**Русский** · [English](README.en.md)

[![CI](https://github.com/sargisis/ReactX/actions/workflows/pages.yml/badge.svg)](https://github.com/sargisis/ReactX/actions/workflows/pages.yml)
[![Версия](https://img.shields.io/github/v/tag/sargisis/ReactX?sort=semver&label=%D0%B2%D0%B5%D1%80%D1%81%D0%B8%D1%8F&color=f5a524)](CHANGELOG.md)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-0d0f12?logo=cplusplus)](https://sargisis.github.io/ReactX/docs/safety)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-Emscripten-654ff0?logo=webassembly&logoColor=white)](https://sargisis.github.io/ReactX/docs/browser)
[![Сайт](https://img.shields.io/badge/%D1%81%D0%B0%D0%B9%D1%82-sargisis.github.io%2FReactX-b45309)](https://sargisis.github.io/ReactX/)
[![Лицензия: MIT или Apache-2.0](https://img.shields.io/badge/%D0%BB%D0%B8%D1%86%D0%B5%D0%BD%D0%B7%D0%B8%D1%8F-MIT%20%7C%20Apache--2.0-6b675e)](#лицензия)

[Сайт и документация](https://sargisis.github.io/ReactX/) · [Быстрый старт](#быстрый-старт) · [Примеры](examples) · [Изменения](CHANGELOG.md) · [Для AI-помощников](#для-ai-помощников)

<img src=".github/assets/two-backends.svg" width="860" alt="Один компонент: в терминале и в браузере">

</div>

## Почему ReactX

- **Как React, только C++.** Компоненты — обычные функции, состояние — хуки, изменения вносит reconciler с ключами и пакетными обновлениями.
- **Один код — два мира.** Терминал (ANSI, Flexbox на Yoga, фокус, клавиатура) и браузер (WebAssembly + DOM). Готовые компоненты `rx::ui` выглядят уместно и там, и там.
- **Типы ловят ошибки.** Props — своя структура: `h(Counter, {.start = 5})`. Опечатку в имени поля или неверный тип найдёт компилятор, а не пользователь.
- **Весь стек на C++.** Роутер, `useFetch`, свой JSON (с рефлексией C++26), HTTP-сервер и CLI `reactx` — без Node, npm и webpack. Счётчик в браузере весит ~135 КБ.
- **Современный и строгий C++.** Контракты C++26, `std::expected` для ошибок, hardening стандартной библиотеки; каждый push проверяется под ASan, UBSan и ThreadSanitizer.
- **Этот сайт тоже на ReactX:** [sargisis.github.io/ReactX](https://sargisis.github.io/ReactX/).

## Быстрый старт

Установите `reactx` одним из способов:

- **Готовый `reactx`** для Linux x86-64 — архив в [GitHub Releases](https://github.com/sargisis/ReactX/releases) (`bin/reactx` положите в `PATH`). Для веб-проектов нужны [Emscripten](https://emscripten.org) 6+ (переменная `EMSDK`), CMake 3.28+ и Ninja; для терминальных — ещё **GCC 16+**.
- **Docker** — всё уже внутри (GCC 16, Emscripten, `reactx`):
  ```sh
  docker run --rm -it -u "$(id -u):$(id -g)" -v "$PWD:/app" -p 8090:8090 ghcr.io/sargisis/reactx
  ```
  Внутри — `reactx dev --host 0.0.0.0`, чтобы страница открылась снаружи. Проекты из `reactx new` открываются в VS Code «Reopen in Container» и в GitHub Codespaces: в них есть `.devcontainer/`.
- **Из исходников** (GCC 16+, CMake 3.28+, Ninja):
  ```sh
  git clone https://github.com/sargisis/ReactX.git && cd ReactX
  cmake --preset release -DCMAKE_CXX_COMPILER=g++-16 && cmake --build --preset release
  cmake --install build/release --prefix ~/.local          # ~/.local/bin/reactx
  ```

```sh
reactx new my-app && cd my-app      # веб-проект (или --terminal)
reactx dev                          # сервер на C++, пересборка и обновление страницы при сохранении
```

Откройте http://127.0.0.1:8090/ и правьте `src/main.cpp`. Дальше:

```sh
reactx build                        # build/web/dist — файлы для любого хостинга
reactx add magic_enum https://github.com/Neargye/magic_enum.git --rev v0.9.8 --target magic_enum::magic_enum
reactx update                       # перейти на новейший релиз ReactX
reactx dev --proxy http://127.0.0.1:5000   # /api — на свой бэкенд (Flask, Django, Express, ...)
```

## Как выглядит код

```cpp
#include <string>
#include "reactx/dom.hpp"       // mountDom — браузер
#include "reactx/reactx.hpp"    // Element, h, хуки, контекст, memo
using namespace rx;

struct TodoProps {
    std::string title{};
    bool done = false;
    Callback onToggle{};
};

Element Todo(const TodoProps& p) {
    return h("li", {{"className", p.done ? "done" : ""}, {"onClick", p.onToggle}}, {p.title});
}

Element App(const Props&) {
    auto [done, setDone] = useState(false);
    useKeyboard([setDone](const KeyEvent& k) {          // сочетания клавиш для всего приложения
        if (k.ctrl && k.key == "z") { setDone(false); return true; }
        return false;
    });
    return h("ul", {}, {
        h(Todo, {.title = "Написать reconciler", .done = done, .onToggle = [setDone, done] { setDone(!done); }}),
    });
}

int main() { mountDom(h(App)); }
```

Для терминала — `#include "reactx/terminal.hpp"` и `TerminalApp().run(h(App))` вместо `mountDom`. Компоненты те же.

## Что внутри

| | |
|---|---|
| **Ядро** | элементы `h()`, фрагменты, ключи; компоненты-функции; props-структуры; reconciler с переиспользованием узлов; пакетные обновления |
| **Хуки** | `useState`, `useEffect`, `useRef`, `useMemo`, `useCallback`, `useContext`, `useReducer`, `useTimeout`, `useInterval`, `useKeyboard`, `useDebounce`, `useLocalStorage`, `useWindowSize`, `memo<C>` |
| **События** | `onClick`, `onInput`, `onChange`, `onKey`, `onSubmit`, `onFocus`, `onBlur`, `onScroll`; всплытие как в React |
| **Терминал** | ANSI-отрисовка только изменённых строк, Flexbox на Yoga, рамки, цвета, фокус, Unicode/ASCII по локали |
| **Браузер** | WebAssembly + DOM, управляемые поля, SVG, делегирование событий |
| **Компоненты** | `rx::ui`: кнопки, поля, флажки, вкладки, модальные окна, списки — с темой |
| **Приложения** | роутер (History API), `useFetch`, JSON с рефлексией, HTTP-сервер на C++, REST API |
| **Инструменты** | CLI `reactx` (`new`, `dev`, `build`, `add`, `remove`, `update`), модуль `import reactx;`, `TextRenderer` для тестов |

## Справочник

<details>
<summary><b>Хуки и события</b></summary>

- `useState` — значение или функция-updater; равное значение не вызывает перерисовку.
- `useEffect` — без deps (каждый раз), `deps(a, b)` или `deps()` (один раз); может вернуть функцию очистки.
- `useRef`, `useMemo` (пересчёт только при смене deps), `useCallback` (обработчик сохраняет «личность», пока не сменились deps).
- `useReducer(reducer, initial)` → `[state, dispatch]`: состояние, которое меняют действия; берётся самый свежий reducer.
- `useTimeout(fn, std::optional(5s))`, `useInterval(fn, 1s)` — таймеры в потоке интерфейса; `std::nullopt` выключает, размонтирование отменяет.
- `useKeyboard(handler)` — сочетания клавиш для всего приложения, где бы ни был фокус: вызывается раньше `onKey` элемента, `return true` забирает клавишу; пока фокус в поле ввода, клавиши набора текста сюда не приходят.
- `useDebounce(value, 300ms)`, `useLocalStorage("key", default)` (браузер — `localStorage`, нативно — память), `useWindowSize()`.
- **Контекст**: `createContext<T>(default)`, `ctx.provide(value, {children})`, `useContext(ctx)`; при смене значения перерисовываются все читатели, даже сквозь `memo`.
- **memo**: `h(memo<Item>, props)` пропускает перерисовку при тех же props. Для props-структуры — её `operator==`; для `Props` — значения через `==`, обработчики равны, только если из одного `useCallback`; элементы с детьми перерисовываются всегда.
- **Батчинг**: `setState` только помечает компонент; перерисовка в `flush()` (сама — после `render`, `dispatch`, ответов `fetch` и таймеров), родители раньше детей.

**События** — четыре вида обработчиков: `Callback` (`void()`: `onClick`, `onFocus`, `onBlur`, `onSubmit`), `KeyHandler` (`void(const KeyEvent&)`: `onKey`), `InputHandler` (`void(const std::string&)`: `onInput`, `onChange`) и `ScrollHandler` (`void(const ScrollEvent&)`: `onScroll`, с `nearBottom()` для подгрузки). Бэкенд передаёт данные через `Root::dispatch(node, "onKey", KeyEvent{"Enter"})`; если данные не подходят обработчику, `dispatch` вернёт `false`. Обработчики остаются в ядре и в бэкенд не передаются.

</details>

<details>
<summary><b>Терминал</b></summary>

```cpp
#include "reactx/terminal.hpp"

struct AppProps { Callback onQuit{}; };

int main() {
    if (!Terminal::isInteractive()) return 1;
    TerminalApp app;
    app.run(h(App, {.onQuit = [&app] { app.quit(); }}));
}
```

Раскладка — **Flexbox**, её считает [Yoga](https://github.com/facebook/yoga) (движок React Native). Единица — клетка терминала.

```cpp
h("box", {{"flexGrow", 1}, {"justifyContent", "center"}, {"alignItems", "center"}}, {   // центр экрана
    h("box", {{"border", true}, {"title", "Окно"}, {"padding", 1}, {"gap", 1}, {"width", "60%"}}, {
        h("box", {{"direction", "row"}, {"gap", 1}}, {
            h("text", {}, {"Имя:"}),
            h("input", {{"flexGrow", 1}, {"value", name}}),                          // тянется на всю ширину
        }),
        h("box", {{"direction", "row"}, {"justifyContent", "space-between"}}, {
            h("button", {}, {"Отмена"}), h("button", {}, {"OK"}),
        }),
    }),
});
```

| Тег | Что рисует |
|---|---|
| `box` (и любой неизвестный тег) | flex-контейнер, по умолчанию столбец |
| `text` | свой текст; переносится по словам (`wrap=false` — обрезать) |
| `input` | поле ввода: `value`, `placeholder`, курсор `_` в фокусе |
| `button` | `[ label ]` |

**Flex-пропсы** (как в CSS): `direction` (`row`, `column`, `row-reverse`, `column-reverse`), `justifyContent` (`start`, `center`, `end`, `space-between`, `space-around`, `space-evenly`), `alignItems` / `alignSelf` (`start`, `center`, `end`, `stretch`), `flexGrow`, `flexShrink`, `flexBasis`, `flexWrap`, `width` / `height` / `min*` / `max*` (число или `"50%"`), `padding*` и `margin*` (`X`, `Y`, `Left`, …; `margin: "auto"`), `gap` / `rowGap` / `columnGap`, `position: "absolute"` + `top` / `left` / `right` / `bottom`, `display: "none"`, `overflow: "hidden"`.

**Оформление**: `border`, `title`, `color`, `bg`, `bold`, `dim`, `underline`, `focusable`. Цвета: `black red green yellow blue magenta cyan white gray`, `bright-<цвет>`. Рамки — `┌─┐` в UTF-8 и `+-+` в других локалях (по `std::text_encoding`).

**Клавиши**: Tab/↓ и Shift+Tab/↑ — фокус (элемент, который его потерял, получает `onBlur`, новый — `onFocus`); любая клавиша уходит в `onKey` элемента в фокусе и всплывает к родителям; в `input` символы и Backspace меняют `value` через `onInput` (держите `value` в `useState`); Enter/Space — `onClick`; Ctrl+C — выход. Фокус получают `input`, `button` и узлы с `focusable=true`; `trapFocus` не выпускает Tab из окна, `opaque` стирает то, что под коробкой.

Отличия от браузера: корень занимает весь терминал; `flexShrink` по умолчанию 1 в строках и 0 в столбцах (что не влезло по высоте — обрезается), кнопки не сжимаются; рамка всегда обрезает содержимое.

Демо: `./build/debug/todo_tui`, `./build/debug/ui_demo` (нужен настоящий терминал).

</details>

<details>
<summary><b>Компоненты <code>rx::ui</code></b></summary>

Готовые компоненты ([`reactx/ui.hpp`](include/reactx/ui.hpp)) с темой через контекст — **один и тот же код в терминале и в браузере**: в терминале они рисуются рамками и символами ([`ui_terminal.cpp`](src/ui_terminal.cpp)), в браузере — HTML с классами `rx-*` и CSS, который сам добавляется на страницу ([`ui_web.cpp`](src/ui_web.cpp)).

```cpp
ui::themeProvider(ui::Theme{.accent = "magenta"}, {
    ui::tabs({.tabs = {
        {"Задачи", {
            ui::input({.value = draft, .onChange = setDraftFn, .onSubmit = addFn, .label = "Новая:"}),
            ui::list({.title = "Список", .items = {
                ui::listItem({.label = "Купить молоко", .detail = "сегодня", .onSelect = toggleFn}),
            }}),
            ui::button({.label = "Удалить всё", .onClick = askFn, .variant = ui::Variant::Danger}),
        }},
        {"Настройки", {ui::checkbox({.label = "Тёмная тема", .checked = dark, .onToggle = flipFn})}},
    }}),
    ui::modal({.title = "Точно?", .open = asking, .onClose = closeFn, .children = {/* ... */}}),
});
```

| Компонент | Что умеет |
|---|---|
| `button` | `label`, `onClick`, `variant` (`Default`, `Primary`, `Danger`, `Success`), `disabled` (не получает фокус) |
| `input` | `value`, `onChange(text)`, `onSubmit(text)` по Enter, `placeholder`, `label`; Escape очищает |
| `checkbox` | `label`, `checked`, `onToggle` (Enter / Space) |
| `tabs` | вкладки `{label, content}`, `initial`; Enter на заголовке открывает вкладку |
| `modal` | `title`, `open`, `onClose` (Escape), `width`; закрывает то, что под ним, держит фокус внутри и возвращает его после закрытия |
| `list` / `listItem` | рамка с заголовком, `emptyText`; пункт: `label`, `detail` справа, `selected`, `onSelect`, `onKey` |

Тема (`ui::Theme`): `accent`, `text`, `muted`, `success`, `warning`, `danger`, `surface`. В браузере тема превращается в CSS-переменные (`--rx-accent`, …), имена цветов терминала переводятся в палитру, можно передавать и CSS-цвета (`"#ff6600"`); свои стили — по классам `rx-btn`, `rx-btn--primary`, `rx-input`, `rx-tab--active`, `rx-modal__dialog`, `rx-item--selected`. Каждый компонент есть и как функция с `Props`: `h(ui::Button, {{"label", "OK"}})`.

Демо: `./build/debug/ui_demo` (терминал) и `web_ui_demo.html` (браузер).

</details>

<details>
<summary><b>В браузере (WebAssembly)</b></summary>

Бэкенд [`DomRenderer`](include/reactx/dom.hpp) рисует в настоящий DOM: теги — обычный HTML, оформление — обычный CSS. `mountDom(h(App))` монтирует приложение в `<div id="app">`.

| Пропс / событие | В браузере |
|---|---|
| `className`, `style` (строка CSS), `htmlFor` | `class`, `style`, `for` |
| `value` | значение поля; поле управляемое — показывает то, что в состоянии |
| `checked`, `disabled`, `selected` | свойства DOM |
| остальное | атрибуты (`true` — есть, `false` — убрать) |
| `onClick`, `onInput(text)`, `onChange`, `onKey(KeyEvent)`, `onSubmit`, `onFocus`, `onBlur` | события; всплывают как в React; `onSubmit` не перезагружает страницу |
| `onScroll(ScrollEvent)` | прокрутка элемента (не всплывает): `top`, `scrollHeight`, `clientHeight`, `nearBottom()` |
| `svg`, `rect`, `circle`, `path`, `g`, … | SVG в своём пространстве имён; `svg:text`, `svg:a` — для тегов, которые есть и в HTML |
| `data-rx-self` | элемент получает только клики по себе, не по детям (фон окна) |
| `data-rx-trap` | Tab не выходит из элемента; при появлении он забирает фокус, при удалении — возвращает |

```sh
cmake --preset web && cmake --build --preset web
ctest --preset web                                     # тесты DOM-бэкенда под Node
./build/debug/reactx_serve build/web --port 8090       # http://localhost:8090/
```

Результат — обычные `.html`, `.js`, `.wasm`: их можно выложить на любой статический хостинг.

</details>

<details>
<summary><b>Роутер, данные и API</b></summary>

**Роутер** ([`reactx/router.hpp`](include/reactx/router.hpp)) — как React Router; в браузере через History API (адрес меняется, «назад»/«вперёд» работают, страница не перезагружается), в терминале — история в памяти:

```cpp
router::routes({
    {"/",          h(Home)},
    {"/todos",     h(TodoList)},
    {"/todos/:id", h(TodoPage)},     // router::useParam("id")
    {"*",          h(NotFound)},     // router::useLocation()
});
router::link({.to = "/todos/7", .label = "Открыть"});   // <a href>, Ctrl+клик — новая вкладка
router::navigate("/todos");  router::back();  router::useQuery("show");
```

Базовый путь (например `/ReactX/` на GitHub Pages) берётся из `<base href>`; маршруты и ссылки в коде пишутся без него.

**Данные** ([`reactx/fetch.hpp`](include/reactx/fetch.hpp)):

```cpp
auto todos = useFetch("/api/todos");            // заново при смене url; todos.reload()
if (todos.loading) ...; if (!todos.ok()) ... todos.errorText();
if (auto list = todos.tryJson()) for (const auto& t : list->asArray()) ...
fetch({.method = "POST", .url = "/api/todos", .body = json::object({{"title", "Молоко"}}).dump()},
      [](const FetchResponse& r) { ... });
```

`setLocalFetch` — «сервер внутри приложения»: запросы сначала идут в обработчик на C++, сеть не нужна (так работают статические сайты без бэкенда и тесты API).

**Полный стек** — интерфейс и сервер на C++ ([examples/web_app.cpp](examples/web_app.cpp) + [examples/api_server.cpp](examples/api_server.cpp)):

```sh
cmake --build --preset web && cmake --build --preset debug
./build/debug/api_server build/web/app --port 8091 --data todos.json   # http://127.0.0.1:8091/
```

| Запрос | Ответ |
|---|---|
| `GET /api/todos` | список |
| `POST /api/todos` `{"title": "..."}` | 201 + новая задача |
| `GET /api/todos/:id` | задача или 404 |
| `PATCH /api/todos/:id` `{"done": true}` | обновлённая задача |
| `DELETE /api/todos/:id` | 204 |

Ошибки — `{"error": "..."}`. С `--data файл.json` задачи сохраняются на диск атомарно и переживают перезапуск.

</details>

<details>
<summary><b>Сервер на C++</b></summary>

Свой HTTP/1.1-сервер ([`reactx/http.hpp`](include/reactx/http.hpp), POSIX-сокеты, поток на соединение) — для разработки Python и Node не нужны:

```cpp
rx::net::HttpServer server("127.0.0.1", 8090);
server.get("/api/todos/:id", [](const rx::net::Request& req) {
    return rx::net::Response::json(R"({"id": )" + req.pathParam("id") + "}");
});
server.serveDirectory("build/web", /*spaFallback=*/true);
if (auto started = server.tryStart(); !started) std::println("{}", started.error());   // порт занят
server.run();   // до Ctrl+C
```

```sh
./build/debug/reactx_serve [ПАПКА] [--port 8090] [--host 127.0.0.1] [--spa] [--base /ReactX/] [--not-found 404.html] [--quiet]
```

Отдаёт файлы с правильными типами (`.wasm` → `application/wasm`), не выпускает за пределы папки (`/../секрет` → 403), `--spa` отдаёт `index.html` для путей роутера, `--base` и `--not-found` повторяют GitHub Pages. Это сервер для разработки (`127.0.0.1`), не для открытого интернета: нет HTTPS, keep-alive и защиты от перегрузки.

**Прокси на другой бэкенд** (как `server.proxy` в Vite): запросы под префиксом уходят на другой сервер, ответ возвращается как есть — статус, заголовки (в том числе несколько `Set-Cookie`), тело; путь не раскодируется по дороге.

```cpp
server.proxy("/api", *rx::net::parseUpstream("http://127.0.0.1:5000"));   // /api и /api/...
auto reply = rx::net::forward(request, {.host = "127.0.0.1", .port = 5000});   // std::expected<Response, HttpError>
```

```sh
reactx dev --proxy http://127.0.0.1:5000              # /api → Flask на :5000
reactx dev --proxy /graphql=http://127.0.0.1:4000     # свой префикс; --proxy можно повторять
```

Или в `reactx.json`: `"web": {"base": "/", "proxy": {"/api": "http://127.0.0.1:5000"}}`. Только `http://`; бэкенд недоступен — 502 с причиной, не ответил за 30 с — 504.

</details>

<details>
<summary><b>Стандарт C++ и безопасность</b></summary>

**Правило:** берём из стандартной библиотеки всё, что в ней уже есть, и не пишем замен тому, чего нет. Нативная сборка требует **GCC 16**, браузерная — Emscripten 6 (libc++), где C++26-библиотеки пока нет, поэтому общий код использует только то, что есть в обеих.

| Где | Что используем |
|---|---|
| везде | `std::expected`, `std::flat_map`, `std::flat_set`, `std::format` / `std::print`, `std::from_chars` ([`reactx/parse.hpp`](include/reactx/parse.hpp)), `= delete("причина")`, `T...[0]`, `#embed` |
| терминал | `std::inplace_vector<char, 4>` — символ в ячейке без кучи, `std::mdspan` — экран `screen[строка, столбец]`, `std::text_encoding` |
| CLI | `std::generator` — обход файлов, `std::expected`-цепочка для `reactx.json` |
| предусловия | контракты C++26 (`-fcontracts`), `<stacktrace>`, `<debugging>` |
| JSON | рефлексия C++26 `<meta>`: `json::to(структура)`, `json::from<Структура>(json)` |

Пока нет ни в GCC 16, ни в Emscripten: `<hive>`, `<simd>`, `<linalg>`, `<rcu>`, `<hazard_pointer>`.

**JSON для структур через рефлексию** (GCC 16) — поля структуры становятся ключами, типы проверяются, ошибка говорит, где:

```cpp
struct Dependency { std::string git; std::string rev; std::optional<std::string> target; };
struct Manifest { std::string name; std::optional<std::map<std::string, Dependency>> dependencies; };

auto m = rx::json::from<Manifest>(rx::json::parse(text));   // std::expected<Manifest, json::Error>
// ошибка: field "dependencies": "fmt": missing field "rev"
rx::json::Value v = rx::json::to(*m);
```

Поддерживаются `bool`, числа, строки, `std::optional`, последовательности, словари со строковыми ключами и вложенные структуры; поле `template_` читается как ключ `"template"`. Подключается к цели: `target_link_libraries(app PRIVATE reactx::reflection)` (не на всё сразу: GCC 16.0.1 с `-freflection` не читает модуль `import reactx;`). В браузерной сборке рефлексии нет.

- **Hardening** (`REACTX_HARDENING`, включено): выход за границы в `v[i]`, `s[i]`, `front()` пустого контейнера останавливает программу, а не портит память. Действует и на программы, подключившие ReactX.
- **Предусловия** `REACTX_PRE(условие)` ([`reactx/contract.hpp`](include/reactx/contract.hpp)) — для ошибок программиста. На GCC 16 это контракт C++26 `contract_assert`; без контрактов макрос печатает условие, место и стек вызовов.
- **Ошибки как значения** (`std::expected`) там, где сбой — нормальная ситуация: `json::tryParse`, `FetchResponse::tryJson`, `HttpServer::tryStart`, `net::parseRequestHead`, `rx::parseNumber` / `rx::parsePort` вместо `std::stoi`, файлы в CLI.
- **Санитайзеры в CI**: каждый push — все тесты под ASan + UBSan + LeakSanitizer и под ThreadSanitizer.

</details>

<details>
<summary><b>Сборка из исходников</b></summary>

Зависимость одна — [Yoga](https://github.com/facebook/yoga) v3.2.1 (MIT), CMake скачает её сам (без интернета: `-DFETCHCONTENT_SOURCE_DIR_YOGA=/путь/к/yoga`).

```sh
cmake --preset debug -DCMAKE_CXX_COMPILER=g++-16   # или release
cmake --build --preset debug
ctest --preset debug -LE slow                       # быстрые тесты; без -LE — ещё и полный прогон CLI
./build/debug/counter                               # и todo, todo_tui, ui_demo
```

С генератором Makefiles модуль отключается сам и всё собирается через заголовки; вручную — `-DREACTX_MODULES=OFF`.

**`import` или `#include`**: `import reactx;` подключает весь публичный API (стандартные заголовки подключайте сами; `import std;` не используется). Заголовки (`reactx/reactx.hpp`) работают так же — модуль ([modules/reactx.cppm](modules/reactx.cppm)) лишь реэкспортирует их. С `-fsanitize=undefined` или `thread` модуль отключается сам (GCC их не переносит).

</details>

<details>
<summary><b>Публикация сайта на GitHub Pages</b></summary>

Сайт собирается и публикуется автоматически ([.github/workflows/pages.yml](.github/workflows/pages.yml)) при каждом push в `main`: нативные тесты (GCC 16), санитайзеры, сборка Emscripten, веб-тесты под Node; если всё прошло — `build/pages/site` записывается в ветку **`gh-pages`**. Один раз: **Settings → Pages → Deploy from a branch → `gh-pages` / `(root)`**.

Сайт ([examples/site.cpp](examples/site.cpp), [web/site_shell.html](web/site_shell.html)) — на ReactX: главная с живым примером, документация ([examples/site_docs.hpp](examples/site_docs.hpp)) с поиском и оглавлением, демо, `llms.txt`. `404.html` — копия сайта, поэтому прямые ссылки вида `/ReactX/docs/hooks` открываются. Проверить локально как на Pages:

```sh
cmake --preset pages && cmake --build --preset pages
./build/debug/reactx_serve build/pages/site --base /ReactX/ --not-found 404.html   # http://127.0.0.1:8090/ReactX/
```

</details>

<details>
<summary><b>Как это работает</b></summary>

1. `h(...)` строит дерево **элементов** — неизменяемое описание UI.
2. `Root::render` передаёт его в **reconciler** ([src/reconciler.cpp](src/reconciler.cpp)); он хранит дерево **fiber'ов** — смонтированные элементы с состоянием хуков и узлами бэкенда.
3. При перерисовке reconciler сравнивает новые элементы со старыми fiber'ами: совпали тип и ключ (или позиция) — fiber обновляется, бэкенду уходят только изменённые пропсы и текст; не совпали — старый удаляется, новый создаётся.
4. `sync` приводит порядок детей в бэкенде к нужному с минимумом `insertChild` / `removeChild`.
5. После commit запускаются эффекты; если в них меняется состояние, сразу идёт следующий проход.

| Действие | Операции бэкенда |
|---|---|
| клик `+1` в counter | одна: `text #4 = "Count: 6"` |
| отметить задачу в todo | две: счётчик «N left» и `[x]` |
| reverse списка с ключами | две перестановки, ничего не создаётся |
| удалить задачу | `remove` одного узла + `destroy` его поддерева |

</details>

<details>
<summary><b>Структура репозитория</b></summary>

```
include/reactx/   публичный API: element, hooks, context, memo, router, fetch, json, http,
                  dom, terminal, ui, dsl, parse, contract, text_renderer
src/              реализация; reconciler.cpp — ядро, utility_hooks.cpp — таймеры, клавиши,
                  размер окна, хранилище; dom.cpp — браузер; terminal*.cpp — терминал
modules/          reactx.cppm — import reactx;
tools/            reactx.cpp — CLI (шаблоны проектов внутри); serve.cpp — reactx_serve
examples/         терминал: counter, todo, todo_tui, ui_demo; браузер: web_counter, web_todo,
                  web_ui_demo; полный стек: web_app + api_server + todo_api.hpp;
                  сайт: site.cpp + site_docs.hpp
web/              HTML-оболочки: shell.html (демо), app_shell.html (web_app), site_shell.html (сайт)
tests/            по файлу на область; test_cli.cpp запускает настоящий CLI;
                  test_dom*.cpp, test_ui_web.cpp, test_web_*.cpp + web/fake_dom.js — браузер под Node
cmake/            ReactXApp.cmake — reactx_add_app(); version.hpp.in
llms.txt          API для AI-помощников, которые пишут приложения на ReactX
AGENTS.md         правила и проверки для тех, кто меняет сам ReactX
```

</details>

<details>
<summary><b>Ограничения сейчас</b></summary>

- Числа вроде `size_t` в props нужно явно приводить к `int64_t`.
- В тестах после `setState` вне `dispatch` нужно самому вызвать `root.flush()` (в браузере и `TerminalApp` это происходит само).
- Терминал: ширина символа считается за 1 колонку (эмодзи и иероглифы могут сдвигать строку); курсор в `input` только в конце текста; нет прокрутки — что не влезло, обрезается; только POSIX (Linux/macOS).
- `rx::ui`: переключение вкладок стрелками и выбор «по фокусу» пока не сделаны; нет прокрутки длинных списков.
- Браузер: `import reactx;` в веб-сборке выключен; нет событий мыши кроме клика (наведение, перетаскивание); `onScroll` — только у элемента с прокруткой; страница строится в браузере (нет серверного рендеринга).
- `useFetch` в терминальной сборке ходит только в `setLocalFetch` (HTTP-клиента там нет).
- Рефлексия (`json::to` / `from`) — только в нативной сборке.
- Раскладка терминала пересчитывается целиком на каждую перерисовку.
- `memo` не сравнивает детей; хуки нельзя вызывать в цикле с разным числом итераций (как в React) — для элементов списка нужен отдельный компонент.

</details>

## План

- [x] Ядро, хуки, reconciler с ключами, абстрактный бэкенд и `TextRenderer`
- [x] Терминал: ANSI, Flexbox на Yoga, фокус и клавиатура; `rx::ui` с темой
- [x] Браузер: WebAssembly + DOM, роутер, `useFetch`, JSON, HTTP-сервер и REST API на C++
- [x] Сайт с документацией на GitHub Pages; CLI `reactx` (`new`, `dev`, `build`, `add`, `remove`, `update`)
- [x] v0.2–0.4: C++26, контракты, рефлексия, props-структуры, санитайзеры в CI, DSL `tag::div`
- [x] `llms.txt` и `AGENTS.md` для AI-помощников
- [x] v0.5: `useKeyboard`, `useReducer`, таймеры, события фокуса и прокрутки, SVG
- [x] Готовый `reactx` в GitHub Releases, Docker-образ и devcontainer; `reactx dev --proxy`
- [ ] Самообновление `reactx`
- [ ] Свои библиотеки для фронтенда и для работы с C++
- [ ] Позже — окна на рабочем столе: SDL3 + Skia/ThorVG

## Для AI-помощников

[llms.txt](llms.txt) — API, полные программы и частые ошибки для тех, кто пишет приложения на ReactX (опубликован на сайте: https://sargisis.github.io/ReactX/llms.txt). [AGENTS.md](AGENTS.md) — правила и проверки для тех, кто меняет сам ReactX. `reactx new` кладёт в каждый проект свой `AGENTS.md`.

## Лицензия

ReactX распространяется **на ваш выбор** на условиях одной из двух лицензий:

- [Apache License 2.0](LICENSE-APACHE)
- [MIT](LICENSE-MIT)

Если вы присылаете изменения в ReactX (pull request), вы соглашаетесь, что они распространяются на тех же условиях — под обеими лицензиями, без дополнительных требований.

---

<div align="center">

Версии по [SemVer](https://semver.org/lang/ru/) (`vX.Y.Z`), номер — в `project(ReactX VERSION ...)` и `rx::version()` · [CHANGELOG](CHANGELOG.md) · [Сайт](https://sargisis.github.io/ReactX/)

</div>
