# ReactX

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

Готовые компоненты для терминала ([`reactx/ui.hpp`](include/reactx/ui.hpp)) с темой через контекст. Свойства задаются назначенными инициализаторами C++20, поэтому опечатку в имени поймает компилятор:

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

Демо: `./build/debug/ui_demo` — вкладки, список задач, настройки, смена темы и окно подтверждения.

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
| `onClick`, `onInput(text)`, `onChange`, `onKey(KeyEvent)`, `onSubmit` | события; всплывают к ближайшему обработчику; `onSubmit` не перезагружает страницу |

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
examples/         counter, todo, todo_tui, ui_demo (терминал); web_counter, web_todo (браузер)
web/              shell.html — HTML-шаблон и CSS для веб-примеров
tests/            тесты ядра, test_terminal.cpp — терминал, test_ui.cpp — rx::ui, test_module.cpp — import reactx;
                  test_dom.cpp + web/fake_dom.js — DOM-бэкенд под Node, test_http.cpp — HTTP-сервер
tools/            serve.cpp — reactx_serve, сервер для разработки
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
- Браузер: модули (`import reactx;`) в веб-сборке пока выключены; страница строится в браузере (для поисковиков нужен будет серверный рендеринг); `rx::ui` пока только для терминала.
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
- [ ] **Шаг 7** — стили и веб-версия компонентов `rx::ui`
- [ ] Шаг 8 — несколько страниц (роутер) и загрузка данных (`fetch`)
- [ ] Шаг 9 — публикация сайта (GitHub Pages)
- [ ] Свой пакетный менеджер и CLI (аналог `npm`): создание проекта, зависимости, сборка;
      команда для разработки (как `npm run dev`) собирает веб-версию и сама запускает `reactx_serve`
- [ ] Позже — окна на рабочем столе: SDL3 + Skia/ThorVG
