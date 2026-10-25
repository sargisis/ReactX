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

Клавиши по умолчанию: Tab/↓ и Shift+Tab/↑ — фокус; любая клавиша уходит в `onKey` элемента в фокусе;
в `input` символы и Backspace меняют `value` через `onInput` (поле управляемое: держите `value` в `useState`);
Enter/Space — `onClick`; Ctrl+C — выход. Фокус получают `input`, `button` и узлы с `focusable=true`; элемент в фокусе рисуется инверсией.

Демо: `./build/debug/todo_tui` (нужен настоящий терминал).

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
examples/         counter, todo, todo_tui (интерактивный)
tests/            тесты ядра, test_terminal.cpp — терминал, test_module.cpp — import reactx;
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
- [ ] **Шаг 5** — библиотека компонентов `rx::ui` с темой (Button, Input, Checkbox, Tabs, Modal, List)
- [ ] Позже — другие бэкенды: SDL3 + Skia/ThorVG (окна), WebAssembly + DOM (браузер и CSS)
