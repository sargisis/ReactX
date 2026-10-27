#pragma once

// Documentation pages of the ReactX site (/docs, /docs/<slug>). The texts are
// plain data; examples/web_app.cpp renders them with ReactX itself.

#include <string>
#include <vector>

namespace site_docs {

struct Section {
    std::string heading{};
    std::string text{};  // paragraphs separated by "\n\n"
    std::string code{};  // C++ (or shell) example, may be empty
};

struct Page {
    std::string slug;
    std::string title;
    std::string summary;
    std::vector<Section> sections;
};

inline const std::vector<Page>& pages() {
    static const std::vector<Page> all = {
        {
            "start", "Быстрый старт", "Что такое ReactX и как собрать первый пример.",
            {
                {"Что это", "ReactX — UI-фреймворк в духе React на современном C++ (режим C++26). Компоненты — обычные функции, состояние — хуки, "
                            "изменения в дерево вносит reconciler.\n\nОдин и тот же код компонентов работает в терминале "
                            "(бэкенд TerminalRenderer) и в браузере (WebAssembly + DOM).", ""},
                {"Свой проект", "Проще всего начать с CLI: reactx new my-app && cd my-app && reactx dev. Подробнее — в разделе «CLI reactx».", ""},
                {"Сборка ReactX", "Нужны CMake 3.28+, Ninja и GCC 16+ (стандартная библиотека C++26). Для браузера — Emscripten 6+ (переменная EMSDK).",
                 "cmake --preset debug -DCMAKE_CXX_COMPILER=g++-16 && cmake --build --preset debug\n"
                 "ctest --preset debug\n\n"
                 "cmake --preset web && cmake --build --preset web    # WebAssembly\n"
                 "./build/debug/reactx_serve build/web                 # http://127.0.0.1:8090/"},
                {"Первый компонент", "Компонент получает props и возвращает дерево элементов. В браузере его монтирует mountDom.",
                 "#include \"reactx/dom.hpp\"\n#include \"reactx/reactx.hpp\"\nusing namespace rx;\n\n"
                 "Element Counter(const Props&) {\n"
                 "    auto [count, setCount] = useState(0);\n"
                 "    return h(\"button\", {{\"onClick\", [setCount] { setCount([](int c) { return c + 1; }); }}},\n"
                 "             {\"Нажато: \" + std::to_string(count)});\n"
                 "}\n\nint main() { mountDom(h(Counter)); }"},
                {"Модуль", "Вместо заголовков можно подключить модуль (в нативной сборке с Ninja).", "import reactx;"},
            },
        },
        {
            "cli", "CLI reactx", "Новый проект, разработка с автообновлением, сборка, зависимости.",
            {
                {"Установка", "CLI собирается вместе с ReactX; cmake --install кладёт reactx и reactx_serve в bin.",
                 "cmake --preset release && cmake --build --preset release\n"
                 "cmake --install build/release --prefix ~/.local     # ~/.local/bin/reactx"},
                {"Новый проект", "reactx new создаёт папку с reactx.json (манифест, как package.json), CMakeLists.txt, "
                                 "CMakePresets.json и src/main.cpp. ReactX подключается с GitHub по тегу релиза "
                                 "(например, v0.5.0), а не по ветке, поэтому проект не сломается от новых изменений; --local — из исходников, "
                                 "из которых собран сам CLI. --terminal — терминальное приложение вместо веба.",
                 "reactx new my-app\ncd my-app"},
                {"reactx dev", "Собирает веб-версию, запускает встроенный сервер на C++ (тот же, что reactx_serve) и следит за "
                               "src/, public/ и манифестом. После каждой удачной пересборки страница в браузере обновляется "
                               "сама; если код не собрался — остаётся прошлая версия, а ошибка видна в терминале.",
                 "reactx dev              # http://127.0.0.1:8090/\nreactx dev --port 3000"},
                {"reactx build", "Сборка для публикации: build/web/dist — обычные файлы для любого статического хостинга "
                                 "(404.html уже лежит рядом с index.html для маршрутов). Для терминального проекта — "
                                 "build/native/<имя>.", "reactx build"},
                {"Зависимости", "reactx add записывает git-репозиторий в reactx.json; CMake проекта подключает его через "
                                "FetchContent, а --target сразу связывает библиотеку с приложением.",
                 "reactx add fmt https://github.com/fmtlib/fmt.git --rev 11.0.2 --target fmt::fmt\nreactx remove fmt"},
                {"reactx update", "Переводит проект на новейший релиз ReactX: находит теги в репозитории из reactx.json, "
                                  "выбирает самый новый по SemVer (без -rc) и записывает его в rev. --check только проверяет, "
                                  "--to — конкретная версия. Следующий reactx build скачает её сам.",
                 "reactx update --check   # ReactX v0.4.0 → v0.5.0 is available\n"
                 "reactx update           # записывает v0.5.0 в reactx.json\n"
                 "reactx update --to v0.3.0\n"
                 "reactx build"},
                {"reactx.json", "Всё, что нужно проекту, в одном файле.",
                 "{\n"
                 "  \"name\": \"my-app\",\n"
                 "  \"template\": \"web\",\n"
                 "  \"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v0.5.0\" },\n"
                 "  \"dependencies\": { \"fmt\": { \"git\": \"...\", \"rev\": \"11.0.2\", \"target\": \"fmt::fmt\" } },\n"
                 "  \"web\": { \"base\": \"/\" }\n"
                 "}"},
            },
        },
        {
            "versions", "Версии и релизы", "Как нумеруются версии ReactX и как обновиться.",
            {
                {"SemVer", "Версии — MAJOR.MINOR.PATCH (Semantic Versioning), теги в git — vMAJOR.MINOR.PATCH. "
                           "PATCH — исправления, MINOR — новые возможности, MAJOR — несовместимые изменения. "
                           "Пока MAJOR = 0, API ещё может меняться: несовместимые изменения поднимают MINOR. "
                           "Что изменилось в каждой версии — в CHANGELOG.md.", ""},
                {"Версия в коде", "Номер версии записан в одном месте — project(ReactX VERSION ...) в CMakeLists.txt; "
                                  "из него генерируется reactx/version.hpp.",
                 "#include \"reactx/version.hpp\"\n\nstd::println(\"{}\", rx::version());   // \"0.5.0\"\n"
                 "static_assert(REACTX_VERSION_MAJOR == 0);"},
                {"Обновиться", "reactx update находит новейший релиз и записывает его в reactx.json; reactx build скачивает его. "
                               "Вручную — поменяйте rev на новый тег.",
                 "reactx update\n# или в reactx.json:\n\"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v0.5.0\" }"},
            },
        },
        {
            "safety", "Стандарт C++ и безопасность", "C++26, hardening, предусловия, std::expected, санитайзеры.",
            {
                {"Режим C++26", "Берём из стандартной библиотеки всё, что в ней есть, и не пишем замен тому, чего нет. "
                                "Везде: std::expected, std::flat_map, std::flat_set, std::format, std::print, std::from_chars. "
                                "В терминале (GCC 16): std::inplace_vector, std::mdspan, std::text_encoding. В CLI: std::generator. "
                                "Пока нет в компиляторах и не используются: <hive>, <simd>, <linalg>, <rcu>, <hazard_pointer>.", ""},
                {"Рефлексия: JSON для структур", "C++26 <meta> (GCC 16): поля структуры становятся ключами JSON, типы "
                                                 "проверяются, ошибка говорит где. Подключается к цели через reactx::reflection. "
                                                 "В браузере (Emscripten) рефлексии пока нет.",
                 "struct Todo { int id; std::string title; bool done; };\n\n"
                 "rx::json::Value v = rx::json::to(Todo{1, \"Read\", false});\n"
                 "auto todo = rx::json::from<Todo>(v);   // std::expected<Todo, json::Error>\n"
                 "// {\"id\": \"7\"} -> field \"id\": expected a number\n\n"
                 "target_link_libraries(app PRIVATE reactx::reflection)   # CMake"},
                {"Hardening", "REACTX_HARDENING (по умолчанию включено): выход за границы в v[i], s[i] и front() пустого "
                              "контейнера останавливает программу, а не портит память. Действует и на ваше приложение.",
                 "cmake -B build -DREACTX_HARDENING=OFF   # выключить, если очень нужно"},
                {"Предусловия", "REACTX_PRE — для ошибок программиста. На GCC 16 это контракт C++26 (contract_assert, -fcontracts): "
                                "нарушение печатает функцию, файл, строку и условие и останавливает программу. В браузере, "
                                "где контрактов нет, макрос делает то же сам.",
                 "#include \"reactx/contract.hpp\"\n\nvoid resize(int w) {\n    REACTX_PRE(w > 0);\n    ...\n}"},
                {"std::expected", "Там, где сбой — обычное дело (ввод, сеть, файлы), ошибка возвращается значением.",
                 "if (auto v = json::tryParse(body)) use(*v);\nelse std::println(\"bad JSON at {}: {}\", v.error().offset(), v.error().what());\n\n"
                 "if (auto started = server.tryStart(); !started) std::println(\"{}\", started.error());   // порт занят\n\n"
                 "auto port = rx::parsePort(arg);   // std::expected<int, std::string>: \"80x\" -> ошибка, а не 80"},
                {"Санитайзеры", "В CI все тесты идут под ASan + UBSan + LeakSanitizer (первая ошибка или утечка роняет сборку) "
                                "и под ThreadSanitizer. Локально:",
                 "cmake -B build -G Ninja -DCMAKE_CXX_FLAGS=\"-fsanitize=address,undefined -fno-sanitize-recover=all\"\n"
                 "cmake --build build && ctest --test-dir build -LE slow"},
            },
        },
        {
            "elements", "Элементы и props", "h(), дети, фрагменты, ключи.",
            {
                {"h()", "h(тег, props, дети) описывает узел; h(Компонент, props, дети) — компонент. Строка в детях — текст.",
                 "h(\"div\", {{\"className\", \"card\"}}, {\n"
                 "    h(\"h1\", {}, {\"Заголовок\"}),\n"
                 "    \"просто текст\",\n"
                 "    h(Counter, {.start = 5}),\n"
                 "})"},
                {"Props — структура", "Props компонента — обычная структура. Опечатку в имени поля или неверный тип ловит "
                                      "компилятор. Дайте полям значения по умолчанию ({}), тогда указываются только нужные.",
                 "struct HelloProps {\n    std::string name = \"мир\";\n};\n\n"
                 "Element Hello(const HelloProps& p) {\n"
                 "    return h(\"p\", {}, {\"Привет, \" + p.name});\n"
                 "}\n\n"
                 "h(Hello, {.name = \"ReactX\"})\n"
                 "h(Hello, {})                  // \"Привет, мир\""},
                {"Дети компонента", "Поле children (std::vector<Element>) получает детей, переданных третьим аргументом.",
                 "struct CardProps {\n    std::string title{};\n    std::vector<Element> children{};\n};\n\n"
                 "Element Card(const CardProps& p) {\n"
                 "    return h(\"div\", {{\"className\", \"card\"}}, p.children);\n}\n\n"
                 "h(Card, {.title = \"Hi\"}, {h(\"p\", {}, {\"текст\"})})"},
                {"Props по строкам", "Старый способ тоже работает: Element(const Props&), props.get<T>(имя, запасное), "
                                     "props.children().",
                 "Element Hello(const Props& p) {\n"
                 "    return h(\"p\", {}, {\"Привет, \" + p.get<std::string>(\"name\", \"мир\")});\n"
                 "}\n\nh(Hello, {{\"name\", \"ReactX\"}})"},
                {"Ключи и фрагменты", "key помогает reconciler'у узнавать элементы списка: при перестановке узлы переносятся, "
                                      "а не создаются заново. fragment() группирует детей без лишнего узла.",
                 "// поле key структуры props — ключ элемента\n"
                 "for (const auto& t : todos) items.push_back(h(TodoItem, {.key = t.id, .title = t.title}));\n"
                 "return fragment(std::move(items));"},
            },
        },
        {
            "hooks", "Хуки", "useState, useEffect, useReducer, useMemo, таймеры, клавиши, хранилище.",
            {
                {"useState", "Возвращает значение и setter. Setter принимает новое значение или функцию от старого. "
                             "Равное значение не вызывает перерисовку.",
                 "auto [name, setName] = useState(std::string{});\nsetName(\"Аня\");\nsetCount([](int c) { return c + 1; });"},
                {"useEffect", "Запускается после отрисовки. deps(...) — когда перезапускать; deps() — один раз. "
                              "Может вернуть функцию очистки.",
                 "useEffect([id] {\n    subscribe(id);\n    return [id] { unsubscribe(id); };\n}, deps(id));"},
                {"useRef", "Изменяемое значение, которое живёт между перерисовками и не вызывает их.",
                 "int& renders = useRef(0);\n++renders;"},
                {"useMemo и useCallback", "useMemo пересчитывает значение только при смене deps. useCallback сохраняет "
                                          "«личность» обработчика, чтобы memo-компоненты не перерисовывались.",
                 "const auto& sorted = useMemo([&] { return sortItems(items); }, deps(items));\n"
                 "Callback onSave = useCallback([setSaved] { setSaved(true); }, deps());"},
                {"useDebounce", "Значение, но только после того, как оно перестало меняться на заданное время (std::chrono): "
                                "для поиска и ввода текста. Таймер работает в потоке интерфейса и отменяется сам.",
                 "auto [query, setQuery] = useState(std::string{});\n"
                 "std::string debounced = useDebounce(query, 300ms);"},
                {"useLocalStorage", "Состояние, которое переживает перезагрузку страницы: хранится в localStorage. "
                                    "Нативно — в памяти, пока работает программа. Поддерживает строки, числа, bool.",
                 "auto [theme, setTheme] = useLocalStorage(\"rx-theme\", std::string(\"dark\"));"},
                {"useWindowSize", "Размер окна браузера (в пикселях) или терминала (в ячейках); компонент перерисовывается при изменении.",
                 "WindowSize size = useWindowSize();"},
                {"useReducer", "Состояние, которое меняют действия: функция (состояние, действие) → новое состояние. "
                               "Удобно, когда полей много или нужна отмена. Берётся самая свежая функция, она может читать props.",
                 "enum class Op { Add, Undo };\n"
                 "auto [doc, dispatch] = useReducer([](const Doc& d, Op op) { /* ... */ return next; }, Doc{});\n"
                 "dispatch(Op::Undo);"},
                {"useTimeout и useInterval", "Вызвать один раз через время или повторять с периодом. std::nullopt выключает; "
                                             "размонтирование отменяет. Работают в потоке интерфейса (нативно — не чаще 100 мс).",
                 "useTimeout([setToast] { setToast(\"\"); }, toast.empty() ? std::nullopt : std::optional(5s));\n"
                 "useInterval([reload] { reload(); }, 30s);"},
                {"useKeyboard", "Сочетания клавиш для всего приложения, где бы ни был фокус. Обработчик вызывается раньше onKey "
                                "элемента; верните true, чтобы забрать клавишу. Пока пользователь печатает в поле, "
                                "клавиши набора текста сюда не приходят — «/» и «n» не мешают печатать.",
                 "useKeyboard([undo](const KeyEvent& k) {\n"
                 "    if (k.ctrl && k.key == \"z\") { undo(); return true; }\n"
                 "    return false;\n"
                 "});"},
                {"Правило хуков", "Как в React: хуки вызываются в одном и том же порядке при каждой отрисовке, "
                                  "не в условиях и не в циклах с переменным числом шагов.", ""},
            },
        },
        {
            "context", "Контекст и memo", "Передача данных вниз без props; пропуск лишних перерисовок.",
            {
                {"Контекст", "createContext<T>(значение) создаёт контекст, ctx.provide(значение, дети) передаёт значение вниз, "
                             "useContext(ctx) читает ближайшее. При смене значения перерисовываются все, кто его читает.",
                 "const auto Theme = createContext<std::string>(\"light\");\n\n"
                 "Theme.provide(\"dark\", {h(Toolbar)});          // в родителе\n"
                 "const std::string& theme = useContext(Theme);   // в любом потомке"},
                {"memo", "h(memo<Item>, props) пропускает перерисовку, если props равны. Обработчики равны, "
                         "только если пришли из одного useCallback.",
                 "h(memo<TodoItem>, {{\"title\", t.title}, {\"onToggle\", onToggle}})"},
            },
        },
        {
            "events", "События", "Обработчики, клавиши, ввод текста, всплытие.",
            {
                {"Обработчики", "Callback — void(), KeyHandler — void(const KeyEvent&), InputHandler — void(const std::string&), "
                                "ScrollHandler — void(const ScrollEvent&).",
                 "h(\"input\", {{\"value\", text},\n"
                 "             {\"onInput\", [setText](const std::string& t) { setText(t); }},\n"
                 "             {\"onKey\", [](const KeyEvent& e) { if (e.key == \"Enter\") submit(); }}})"},
                {"KeyEvent", "key — символ («a», «я») или имя: Enter, Escape, Tab, Backspace, Delete, Up, Down, Left, Right, "
                             "Home, End. Флаги ctrl, alt, shift.", ""},
                {"Всплытие", "Как в React: сначала обработчик элемента, затем всех его родителей. "
                             "Так Escape из кнопки внутри окна доходит до самого окна.", ""},
                {"Фокус", "onFocus и onBlur: в браузере — при входе в элемент и уходе из него (всплывают, как в React), "
                          "в терминале — когда фокус переходит по Tab или стрелкам.",
                 "h(\"input\", {{\"value\", email}, {\"onBlur\", [validate] { validate(); }}})"},
                {"Прокрутка", "onScroll (браузер) получает ScrollEvent: top, scrollHeight, clientHeight и др.; nearBottom() "
                              "— пора подгружать ещё. Не всплывает: срабатывает только у прокручиваемого элемента.",
                 "h(\"div\", {{\"style\", \"height: 400px; overflow: auto\"},\n"
                 "            {\"onScroll\", [loadMore](const ScrollEvent& e) { if (e.nearBottom()) loadMore(); }}}, rows)"},
            },
        },
        {
            "terminal", "Терминал", "TerminalApp, теги и Flexbox на Yoga.",
            {
                {"Запуск", "TerminalApp включает raw-режим, рисует дерево и обрабатывает клавиши до Ctrl+C.",
                 "TerminalApp app;\napp.run(h(App, {{\"onQuit\", Callback([&app] { app.quit(); })}}));"},
                {"Теги", "box — flex-контейнер, text — текст с переносом, input — поле ввода, button — кнопка. "
                         "Tab и стрелки переключают фокус, Enter/пробел нажимают.", ""},
                {"Flexbox", "Раскладку считает Yoga (движок React Native): direction, justifyContent, alignItems, flexGrow, "
                            "width/height в клетках или процентах, padding, margin, gap, position: absolute.",
                 "h(\"box\", {{\"flexGrow\", 1}, {\"justifyContent\", \"center\"}, {\"alignItems\", \"center\"}}, {\n"
                 "    h(\"box\", {{\"border\", true}, {\"title\", \"Окно\"}, {\"padding\", 1}}, {h(\"text\", {}, {\"Привет\"})}),\n"
                 "})"},
            },
        },
        {
            "browser", "Браузер", "WebAssembly, DOM, CSS.",
            {
                {"mountDom", "Монтирует приложение в <div id=\"app\"> страницы и живёт после main().",
                 "int main() { mountDom(h(App)); }"},
                {"Props в DOM", "className → class, style — строка CSS, value — управляемое поле ввода, checked/disabled — "
                                "свойства, остальное — атрибуты (true — есть, false — убрать).",
                 "h(\"input\", {{\"type\", \"checkbox\"}, {\"checked\", done}, {\"onChange\", toggle}})"},
                {"События", "onClick, onInput(text), onChange, onKey(KeyEvent), onSubmit (форма не перезагружает страницу), "
                            "onFocus, onBlur, onScroll(ScrollEvent).", ""},
                {"SVG", "Теги SVG (svg, rect, circle, path, line, g, ...) создаются в пространстве имён SVG сами. "
                        "Текст и ссылки внутри SVG — «svg:text», «svg:a» (такие теги есть и в HTML).",
                 "h(\"svg\", {{\"viewBox\", \"0 0 100 40\"}}, {\n"
                 "    h(\"rect\", {{\"width\", \"60\"}, {\"height\", \"40\"}, {\"fill\", \"orange\"}}),\n"
                 "    h(\"svg:text\", {{\"x\", \"64\"}, {\"y\", \"24\"}}, {\"60%\"}),\n"
                 "})"},
            },
        },
        {
            "ui", "Компоненты rx::ui", "Готовые кнопки, поля, вкладки, окна и списки с темой.",
            {
                {"Компоненты", "button, input, checkbox, tabs, modal, list и listItem. Один и тот же код работает в терминале "
                               "и в браузере. Свойства — назначенные инициализаторы C++20.",
                 "ui::button({.label = \"Сохранить\", .onClick = save, .variant = ui::Variant::Primary})\n"
                 "ui::input({.value = name, .onChange = setNameFn, .onSubmit = addFn, .label = \"Имя\"})\n"
                 "ui::modal({.title = \"Удалить?\", .open = asking, .onClose = closeFn, .children = {...}})"},
                {"Тема", "ui::themeProvider передаёт ui::Theme через контекст; в браузере она превращается в CSS-переменные.",
                 "ui::themeProvider(ui::Theme{.accent = \"magenta\"}, {h(App)})"},
            },
        },
        {
            "router", "Роутер", "Несколько страниц, ссылки, параметры, базовый путь.",
            {
                {"routes", "Выбирает первый подходящий маршрут. :имя — параметр, * — что угодно.",
                 "router::routes({\n"
                 "    {\"/\",          h(Home)},\n"
                 "    {\"/todos/:id\", h(TodoPage)},   // router::useParam(\"id\")\n"
                 "    {\"*\",          h(NotFound)},\n"
                 "})"},
                {"Ссылки и переходы", "link — ссылка без перезагрузки (Ctrl+клик — новая вкладка); navigate и back — из кода.",
                 "router::link({.to = \"/todos/7\", .label = \"Открыть\"});\nrouter::navigate(\"/todos\");\nrouter::back();"},
                {"Базовый путь", "Если на странице есть <base href=\"/ReactX/\">, приложение живёт по этому пути (GitHub Pages), "
                                 "а маршруты и ссылки пишутся без него.", ""},
            },
        },
        {
            "data", "Данные: fetch и JSON", "useFetch, fetch, setLocalFetch, rx::json.",
            {
                {"useFetch", "Загружает url при монтировании и при его смене; reload() — загрузить заново.",
                 "auto todos = useFetch(\"/api/todos\");\n"
                 "if (todos.loading) return h(\"p\", {}, {\"Загрузка...\"});\n"
                 "if (!todos.ok()) return h(\"p\", {}, {todos.errorText()});\n"
                 "for (const auto& t : todos.json().asArray()) ..."},
                {"fetch", "Любой метод и тело; ответ приходит в колбэк.",
                 "fetch({.method = \"POST\", .url = \"/api/todos\", .body = json::object({{\"title\", \"Молоко\"}}).dump()},\n"
                 "      [](const FetchResponse& r) { if (r.ok()) ... });"},
                {"setLocalFetch", "«Сервер внутри приложения»: запросы сначала идут в обработчик на C++; если он вернул ответ, "
                                  "сеть не нужна. Подходит для статических сайтов без бэкенда, офлайн-режима и тестов.",
                 "setLocalFetch([&store](const FetchRequest& r) -> std::optional<FetchResponse> {\n"
                 "    if (!r.url.starts_with(\"/api/\")) return std::nullopt;   // остальное — в сеть\n"
                 "    auto reply = todo_api::handle(store, r.method, r.url, r.body);\n"
                 "    return FetchResponse{.status = reply.status, .body = reply.body};\n"
                 "});"},
                {"rx::json", "Свой JSON без зависимостей: parse, построение, чтение с запасным значением, dump.",
                 "auto v = json::parse(R\"({\"title\": \"Молоко\", \"done\": false})\");\n"
                 "std::string title = v[\"title\"].asString();\nbool done = v.get<bool>(\"done\");\n"
                 "json::Value todo = json::object({{\"id\", 1}, {\"title\", title}});\ntodo.dump();"},
            },
        },
        {
            "server", "Сервер на C++", "rx::net::HttpServer, reactx_serve, api_server.",
            {
                {"HttpServer", "HTTP/1.1 на POSIX-сокетах: файлы сайта, маршруты с параметрами, SPA-режим.",
                 "rx::net::HttpServer server(\"127.0.0.1\", 8090);\n"
                 "server.get(\"/api/hello/:name\", [](const rx::net::Request& req) {\n"
                 "    return rx::net::Response::json(json::object({{\"hello\", req.pathParam(\"name\")}}).dump());\n"
                 "});\n"
                 "server.serveDirectory(\"build/web\", /*spaFallback=*/true);\n"
                 "server.start();\nserver.run();"},
                {"Готовые программы", "reactx_serve — раздаёт папку для разработки (с --base и --not-found ведёт себя как "
                                      "GitHub Pages). api_server — пример полного стека: веб-приложение задач и его REST API "
                                      "в одном процессе; с --data задачи сохраняются в файл.",
                 "./build/debug/reactx_serve build/web --port 8090\n"
                 "./build/debug/reactx_serve build/pages/site --base /ReactX/ --not-found 404.html\n"
                 "./build/debug/api_server build/web/app --port 8091 --data todos.json"},
                {"Важно", "Сервер рассчитан на разработку на своём компьютере: нет HTTPS и защиты от перегрузки.", ""},
            },
        },
    };
    return all;
}

}  // namespace site_docs
