# ReactX

**Сайт с документацией и демо: https://sargisis.github.io/ReactX/** — сам сайт тоже написан на ReactX (C++20 → WebAssembly).

React-подобный UI-фреймворк на C++20 (только стандарт C++20, без C++23) с подключаемым бэкендом отрисовки.

```cpp
import reactx;          // C++20-модуль; или #include "reactx/reactx.hpp"
#include <iostream>
#include <string>
using namespace rx;

Element Counter(const Props& props) {
    auto [count, setCount] = useState(props.get<int>("start", 0));

    useEffect([count] {
        std::cout << "count = " << count << '\n';
        return [] { /* cleanup */ };
    }, deps(count));

    return h("box", {}, {
        h("text", {}, {"Count: " + std::to_string(count)}),
        h("button", {{"id", "inc"}, {"onClick", [setCount] { setCount([](int c) { return c + 1; }); }}}, {"+1"}),
    });
}

int main() {
    TextRenderer renderer;          // любой класс, реализующий rx::Renderer
    Root root(renderer);
    root.render(h(Counter, {{"start", 5}}));
    root.dispatch(*renderer.findByProp("id", "inc"), "onClick");
}
```

## Что есть

- **Элементы**: `h("tag", props, children)`, `h(Component, props, children)`, строки как текстовые узлы, `fragment(...)`, проп `key`.
- **Компоненты**: обычные функции `Element(const Props&)`; дети доступны через `props.children()`.
- **Хуки**: `useState` (значение или функция-updater, пропуск ререндера при равном значении), `useEffect` (без deps / `deps(a, b)` / `deps()` = один раз, cleanup), `useRef`, `useMemo` (пересчёт только при смене deps), `useCallback` (обработчик сохраняет «личность», пока не сменились deps).
- **Контекст**: `createContext<T>(default)`, `ctx.provide(value, {children})`, `useContext(ctx)` — значение передаётся вниз без props; при смене значения перерисовываются все, кто его читает, даже сквозь `memo`.
- **memo**: `h(memo<Item>, props)` пропускает перерисовку, если props те же. Значения сравниваются через `==`, обработчики — равны, только если пришли из одного `useCallback`; элементы с детьми перерисовываются всегда.
- **Reconciler**: сравнивает деревья, сопоставляет детей по `key` или позиции, переставляет узлы вместо пересоздания, удаляет лишние пропсы.
- **Батчинг**: `setState` только помечает компонент; перерисовка в `flush()` (автоматически после `render`/`dispatch`), родители раньше детей.
- **События**: три вида обработчиков — `Callback` (`void()`, например `onClick`), `KeyHandler` (`void(const KeyEvent&)`, `onKey`) и `InputHandler` (`void(const std::string&)`, `onInput`). Бэкенд передаёт данные через `Root::dispatch(node, "onKey", KeyEvent{"Enter"})` или `dispatch(node, "onInput", std::string("текст"))`; если данные не подходят обработчику, `dispatch` вернёт `false`.
- **Бэкенд**: интерфейс [`rx::Renderer`](include/reactx/renderer.hpp). Обработчики остаются в ядре и в бэкенд не передаются.
- **TextRenderer**: хранит дерево в памяти, печатает его и лог операций — для отладки и тестов.
- **Терминал** ([`reactx/terminal.hpp`](include/reactx/terminal.hpp)): `TerminalRenderer` рисует дерево ANSI-кодами и перерисовывает только изменившиеся строки; `TerminalApp` включает raw-режим и запускает цикл событий; `decodeKeys` превращает байты терминала в `KeyEvent` (стрелки, Tab/Shift+Tab, Delete, Ctrl/Alt, UTF-8).

## Терминальный бэкенд

```cpp
TerminalApp app;
app.run(h(App, {{"onQuit", Callback([&app] { app.quit(); })}}));
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

Flex-пропсы (как в CSS): `direction` (`row`, `column`, `row-reverse`, `column-reverse`), `justifyContent` (`start`, `center`, `end`, `space-between`, `space-around`, `space-evenly`), `alignItems` / `alignSelf` (`start`, `center`, `end`, `stretch`), `flexGrow`, `flexShrink`, `flexBasis`, `flexWrap`, `width` / `height` / `min*` / `max*` (число или `"50%"`), `padding*` и `margin*` (`X`, `Y`, `Left`, …; `margin: "auto"`), `gap` / `rowGap` / `columnGap`, `position: "absolute"` + `top` / `left` / `right` / `bottom`, `display: "none"`, `overflow: "hidden"`.

Оформление: `border`, `title`, `color`, `bg`, `bold`, `dim`, `underline`, `focusable`.
Цвета: `black red green yellow blue magenta cyan white gray`, `bright-<цвет>`.

Отличия от браузера: корень занимает весь терминал; `flexShrink` по умолчанию 1 в строках и 0 в столбцах (что не влезло по высоте — обрезается), кнопки не сжимаются; рамка всегда обрезает содержимое, без рамки по умолчанию `overflow: visible`.

## Компоненты `rx::ui`

Готовые компоненты ([`reactx/ui.hpp`](include/reactx/ui.hpp)) с темой через контекст — **один и тот же код работает в терминале и в браузере**: в терминале они рисуются рамками и символами ([`ui_terminal.cpp`](src/ui_terminal.cpp)), в браузере — HTML с классами `rx-*` и CSS, который сам добавляется на страницу ([`ui_web.cpp`](src/ui_web.cpp)). Свойства задаются назначенными инициализаторами C++20, поэтому опечатку в имени поймает компилятор:

```cpp
#include "reactx/ui.hpp"   // или import reactx;

ui::Theme theme;
theme.accent = "magenta";

ui::themeProvider(theme, {
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

Тема (`ui::Theme`): `accent`, `text`, `muted`, `success`, `warning`, `danger`, `surface`. Каждый компонент есть и как обычная функция с `Props` (`h(ui::Button, {{"label", "OK"}})`).

Для этого в терминальный бэкенд добавлены: всплытие `onKey` от элемента в фокусе к родителям (как в DOM), `trapFocus` (Tab не выходит из окна) и `opaque` (коробка стирает то, что под ней).

Демо: `./build/debug/ui_demo` (терминал) и `web_ui_demo.html` (браузер) — вкладки, список задач, настройки, смена темы и окно подтверждения.

В браузере:
- тема превращается в CSS-переменные (`--rx-accent`, `--rx-danger`, …); имена цветов терминала (`"cyan"`, `"magenta"`) переводятся в аккуратную палитру, можно передавать и CSS-цвета (`"#ff6600"`);
- свои стили — обычным CSS по классам `rx-btn`, `rx-btn--primary`, `rx-input`, `rx-tab--active`, `rx-modal__dialog`, `rx-item--selected` и т.д.;
- `modal` — затемнённый фон, фокус внутри окна (Tab не выходит), закрытие по Escape и клику мимо окна, фокус возвращается на кнопку, которая его открыла; `width` задаётся в символах (`ch`);
- светлая и тёмная тема браузера подхватываются сами.

Клавиши по умолчанию: Tab/↓ и Shift+Tab/↑ — фокус; любая клавиша уходит в `onKey` элемента в фокусе;
в `input` символы и Backspace меняют `value` через `onInput` (поле управляемое: держите `value` в `useState`);
Enter/Space — `onClick`; Ctrl+C — выход. Фокус получают `input`, `button` и узлы с `focusable=true`; элемент в фокусе рисуется инверсией.

Демо: `./build/debug/todo_tui` (нужен настоящий терминал).

## В браузере (WebAssembly)

Тот же код компонентов работает в браузере: бэкенд [`DomRenderer`](include/reactx/dom.hpp) рисует в настоящий DOM, поэтому теги — обычный HTML, а оформление — обычный CSS.

```cpp
#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"
using namespace rx;

Element Counter(const Props&) {
    auto [count, setCount] = useState(0);
    return h("div", {{"className", "card"}}, {
        h("h1", {}, {"Count: " + std::to_string(count)}),
        h("button", {{"className", "primary"}, {"onClick", [setCount] { setCount([](int c) { return c + 1; }); }}}, {"+1"}),
    });
}

int main() { mountDom(h(Counter)); }   // в <div id="app"> страницы
```

| Пропс / событие | В браузере |
|---|---|
| `className`, `style` (строка CSS), `htmlFor` | `class`, `style`, `for` |
| `value` | значение поля; поле управляемое — показывает то, что в состоянии |
| `checked`, `disabled`, `selected` | свойства DOM |
| остальное | атрибуты (`true` — есть, `false` — убрать) |
| `onClick`, `onInput(text)`, `onChange`, `onKey(KeyEvent)`, `onSubmit` | события; всплывают как в React — срабатывают обработчики элемента и всех его родителей; `onSubmit` не перезагружает страницу |
| `data-rx-self` | элемент получает только клики по себе, не по детям (фон окна) |
| `data-rx-trap` | Tab не выходит из элемента; при появлении он забирает фокус, при удалении — возвращает |

Сборка и запуск (нужен [Emscripten](https://emscripten.org), переменная `EMSDK`):

```sh
cmake --preset web && cmake --build --preset web
ctest --preset web                                   # тесты DOM-бэкенда под Node
./build/debug/reactx_serve build/web --port 8090       # наш сервер на C++; открыть http://localhost:8090/
```

Результат — обычные файлы (`.html`, `.js`, `.wasm`, счётчик весит ~135 КБ), их можно выложить на любой статический хостинг.

### Сервер на C++ (`reactx_serve`, `rx::net::HttpServer`)

Для разработки не нужен Python: свой HTTP/1.1-сервер на C++20 ([`reactx/http.hpp`](include/reactx/http.hpp), POSIX-сокеты, поток на соединение).

```sh
./build/debug/reactx_serve [ПАПКА] [--port 8090] [--host 127.0.0.1] [--spa] [--quiet]
```

- отдаёт файлы с правильными типами (`.wasm` → `application/wasm`), `/` → `index.html`;
- не выпускает за пределы папки (`/../секрет` → 403);
- `--spa`: неизвестные пути без расширения получают `index.html` (для будущего роутера);
- как библиотека — свои маршруты, например API, которое страница загрузит через `fetch`:

```cpp
rx::net::HttpServer server("127.0.0.1", 8090);
server.get("/api/todos", [](const rx::net::Request& req) {
    return rx::net::Response::json(R"([{"title": "Buy milk"}])");
});
server.serveDirectory("build/web");
server.start();
server.run();   // до Ctrl+C
```

Сервер для разработки на своём компьютере (слушает `127.0.0.1`), не для открытого интернета: нет HTTPS, keep-alive и защиты от перегрузки.

## Сайт из нескольких страниц: роутер, данные, API

Полноценное веб-приложение, где и интерфейс, и сервер — C++20 ([examples/web_app.cpp](examples/web_app.cpp) + [examples/api_server.cpp](examples/api_server.cpp)):

```sh
cmake --build --preset web && cmake --build --preset debug
./build/debug/api_server build/web/app --port 8091 --data todos.json   # http://127.0.0.1:8091/
```

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

**Данные** ([`reactx/fetch.hpp`](include/reactx/fetch.hpp), пока в браузерной сборке):

```cpp
auto todos = useFetch("/api/todos");            // заново при смене url; todos.reload()
if (todos.loading) ...; if (!todos.ok()) ... todos.errorText();
for (const auto& t : todos.json().asArray()) ...
fetch({.method = "POST", .url = "/api/todos", .body = json::object({{"title", "Молоко"}}).dump()},
      [](const FetchResponse& r) { ... });
```

**`setLocalFetch`** — «сервер внутри приложения»: запросы сначала идут в обработчик на C++ (например, `todo_api::handle`), сеть не нужна; так работают тесты API без сокетов.

**JSON** ([`reactx/json.hpp`](include/reactx/json.hpp)) — свой, без зависимостей: `json::parse`, `v["key"]`, `v.get<int>("n", 0)`, `json::object({...})`, `json::array({...})`, `dump()` / `dump(2)`; ошибки с позицией; `\uXXXX` и суррогатные пары; `for (auto& x : json::parse(t).asArray())` безопасен (на временном значении возвращается копия).

**API на сервере** ([examples/todo_api.hpp](examples/todo_api.hpp)): маршруты `rx::net::HttpServer` с параметрами и `*`:

| Запрос | Ответ |
|---|---|
| `GET /api/todos` | список |
| `POST /api/todos` `{"title": "..."}` | 201 + новая задача |
| `GET /api/todos/:id` | задача или 404 |
| `PATCH /api/todos/:id` `{"done": true}` | обновлённая задача |
| `DELETE /api/todos/:id` | 204 |
| любой другой `/api/...` | JSON 404 (а не страница сайта) |

Ошибки — `{"error": "..."}` (400 при неверном JSON или пустом `title`). С `--data файл.json` задачи сохраняются на диск (атомарно, через временный файл) и переживают перезапуск сервера. Сайт отдаётся с SPA-fallback: прямой заход на `/todos/7` получает приложение, дальше страницу выбирает роутер.

## Публикация сайта (GitHub Pages)

Сайт собирается и публикуется автоматически ([.github/workflows/pages.yml](.github/workflows/pages.yml)) при каждом `push` в `main`: нативные тесты (GCC 14), сборка Emscripten, веб-тесты под Node, затем выкладка `build/pages/site`. Один раз нужно включить в репозитории **Settings → Pages → Source: GitHub Actions**.

Что внутри сайта ([examples/site.cpp](examples/site.cpp), тоже на ReactX):
- главная с описанием и примером кода;
- **документация** (`/docs`, тексты — [examples/site_docs.hpp](examples/site_docs.hpp)): быстрый старт, элементы, хуки, контекст и memo, события, терминал, браузер, `rx::ui`, роутер, данные, сервер;
- **демо**: счётчик и компоненты `rx::ui`;
- базовый путь `/ReactX/` берётся из `<base href>`; маршруты и ссылки в коде пишутся без него;
- `404.html` — копия сайта, поэтому прямые ссылки вида `/ReactX/docs/hooks` открываются (у GitHub Pages нет другого способа отдать SPA).

Пример полного стека с сервером (todo-приложение [examples/web_app.cpp](examples/web_app.cpp) + `api_server`) на сайт не выкладывается — он запускается локально.

Собрать и проверить локально точно как на GitHub Pages:

```sh
cmake --preset pages && cmake --build --preset pages && ctest --preset pages
./build/debug/reactx_serve build/pages/site --base /ReactX/ --not-found 404.html   # http://127.0.0.1:8090/ReactX/
```

`reactx_serve --base` отдаёт папку по URL-префиксу, `--not-found` — свой файл с кодом 404 (как GitHub Pages).

## Сборка

Зависимость одна — [Yoga](https://github.com/facebook/yoga) v3.2.1 (MIT). Если её нет в системе, CMake скачает её сам при первой настройке (нужен интернет; без интернета: `-DFETCHCONTENT_SOURCE_DIR_YOGA=/путь/к/yoga`).

С модулями C++20 (`import reactx;`) — нужны CMake 3.28+, Ninja и GCC 14+ / Clang 17+ / MSVC 17.4+:

```sh
cmake --preset debug            # или release
cmake --build --preset debug
ctest --preset debug
./build/debug/counter
./build/debug/todo
./build/debug/todo_tui          # интерактивный todo в терминале
./build/debug/ui_demo           # демо компонентов rx::ui
```

Без Ninja (генератор Makefiles) модуль отключается сам и всё собирается через заголовки:

```sh
cmake -S . -B build
cmake --build build -j
./build/reactx_tests
```

Модуль можно выключить вручную: `-DREACTX_MODULES=OFF`.

### `import` или `#include`

- `import reactx;` подключает весь публичный API, включая `TextRenderer`. Стандартные заголовки (`<string>`, `<vector>`, …) нужно подключать самим: модуль их не протаскивает. `import std;` не используется — это C++23.
- Заголовки (`reactx/reactx.hpp`) работают как раньше; модуль ([modules/reactx.cppm](modules/reactx.cppm)) лишь реэкспортирует их.
- GCC 15.2 падает при сборке модулей с `-fsanitize=undefined`, поэтому с этим флагом модуль отключается автоматически.

## Структура

```
include/reactx/   публичный API (element, hooks, renderer, root, text_renderer)
modules/          reactx.cppm — C++20-модуль (import reactx;)
src/              реализация; reconciler.cpp — ядро, terminal*.cpp — терминальный бэкенд
examples/         counter, todo, todo_tui, ui_demo (терминал); web_counter, web_todo, web_ui_demo (браузер);
                  site.cpp + site_docs.hpp (сайт с документацией); web_app + api_server + todo_api.hpp (полный стек)
web/              shell.html — HTML-шаблон и CSS для веб-примеров; app_shell.html — для web_app
tests/            тесты ядра, test_terminal.cpp — терминал, test_ui.cpp — rx::ui, test_module.cpp — import reactx;
                  test_dom.cpp, test_ui_web.cpp, test_web_app.cpp, test_web_base.cpp + web/fake_dom.js — браузер под Node;
                  test_http.cpp, test_todo_api.cpp — сервер; test_json.cpp; test_router.cpp
tools/            serve.cpp — reactx_serve, сервер для разработки
.github/workflows pages.yml — тесты, сборка и публикация сайта
```

Для обычной (быстрой) сборки без отладки:

```sh
cmake --preset release && cmake --build --preset release
```

## Как это работает

1. `h(...)` строит дерево **элементов** — неизменяемое описание UI.
2. `Root::render` передаёт его в **reconciler** ([src/reconciler.cpp](src/reconciler.cpp)). Он хранит дерево **fiber'ов**: смонтированные элементы со состоянием хуков и узлами бэкенда.
3. При перерисовке reconciler сравнивает новые элементы со старыми fiber'ами:
   - совпали тип и ключ (или позиция) → fiber обновляется, бэкенду уходят только изменённые пропсы и текст;
   - не совпали → старый fiber удаляется, новый создаётся.
4. Затем `sync` приводит порядок детей в бэкенде к нужному с минимумом `insertChild`/`removeChild`.
5. После commit запускаются эффекты (`useEffect`). Если в них меняется состояние, сразу идёт следующий проход.

Что видно в примерах:

| Действие | Операции бэкенда |
|---|---|
| клик `+1` в counter | одна: `text #4 = "Count: 6"` |
| отметить задачу в todo | две: счётчик «N left» и `[x]` |
| reverse списка с ключами | две перестановки, ничего не создаётся |
| ввести текст и нажать Enter | новая задача в конце списка, поле очищается |
| удалить задачу | `remove` одного узла + `destroy` его поддерева |

## Ограничения сейчас

- Числа вроде `size_t` в props нужно явно приводить к `int64_t`.
- После `setState` вне `dispatch` нужно самому вызвать `root.flush()` (в `TerminalApp` это делается автоматически).
- Терминал: ширина символа считается за 1 колонку (эмодзи и иероглифы шириной 2 могут сдвигать строку); курсор в `input` только в конце текста; нет прокрутки — что не влезло по высоте, обрезается; только POSIX (Linux/macOS).
- `rx::ui`: переключение вкладок стрелками и выбор «по фокусу» пока не сделаны (компонент не может сам переместить фокус); нет прокрутки длинных списков.
- Браузер: модули (`import reactx;`) в веб-сборке пока выключены; страница строится в браузере (для поисковиков нужен будет серверный рендеринг).
- `useFetch` в терминальной сборке ходит только в `setLocalFetch`-обработчик (сети там нет). Сервер на C++ пока не выложен в интернет: на GitHub Pages только статический сайт.
- Раскладка пересчитывается целиком на каждую перерисовку (для терминала это быстро, но для тысяч узлов понадобится кэш).
- Дети перерисовываются вместе с родителем, если не обёрнуты в `memo<...>`; `memo` не сравнивает детей (`props.children()`).
- Хуки нельзя вызывать в цикле с разным числом итераций (как и в React), поэтому `useCallback` для каждого элемента списка не сделать — нужен отдельный компонент.

## План

- [x] Ядро: элементы, компоненты, props, ключи, фрагменты
- [x] Хуки: `useState`, `useEffect`, `useRef`
- [x] Reconciler с переиспользованием узлов по ключам
- [x] Абстрактный бэкенд `rx::Renderer` + `TextRenderer`
- [x] Примеры (counter, todo) и тесты
- [x] Шаг 1 — события с данными: `onKey(const KeyEvent&)`, `onInput(text)`,
      `Root::dispatch(node, event, data)`
- [x] Модуль C++20: `import reactx;`
- [x] Шаг 2 — бэкенд для терминала: ANSI-отрисовка, рамки, цвета, фокус,
      ввод с клавиатуры; демо — интерактивный todo
- [x] Шаг 3 — `useContext`, `useMemo`/`useCallback`, `memo` (пропуск ререндера при тех же props)
- [x] Шаг 4 — раскладка Flexbox на Yoga (`flexGrow`, `justifyContent`, `alignItems`, …)
- [x] Шаг 5 — библиотека компонентов `rx::ui` с темой (Button, Input, Checkbox, Tabs, Modal, List)
- [x] Шаг 6 — браузер: WebAssembly + DOM-бэкенд, веб-примеры counter и todo
- [x] Шаг 7 — стили и веб-версия компонентов `rx::ui` (тот же код, что в терминале)
- [x] Шаг 8 — несколько страниц (роутер), загрузка данных (`useFetch`), свой JSON, REST API на C++
- [x] Шаг 9 — сайт с документацией на GitHub Pages: автосборка, базовый путь, прямые ссылки на страницы
- [ ] Свой пакетный менеджер и CLI (аналог `npm`): создание проекта, зависимости, сборка;
      команда для разработки (как `npm run dev`) собирает веб-версию и сама запускает `reactx_serve`
- [ ] Позже — окна на рабочем столе: SDL3 + Skia/ThorVG
