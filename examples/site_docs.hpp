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
                {"Что это", "ReactX — UI-фреймворк в духе React на C++20. Компоненты — обычные функции, состояние — хуки, "
                            "изменения в дерево вносит reconciler.\n\nОдин и тот же код компонентов работает в терминале "
                            "(бэкенд TerminalRenderer) и в браузере (WebAssembly + DOM).", ""},
                {"Свой проект", "Проще всего начать с CLI: reactx new my-app && cd my-app && reactx dev. Подробнее — в разделе «CLI reactx».", ""},
                {"Сборка ReactX", "Нужны CMake 3.28+, Ninja и GCC 14+ / Clang 17+. Для браузера — Emscripten (переменная EMSDK).",
                 "cmake --preset debug && cmake --build --preset debug\n"
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
                {"Модуль C++20", "Вместо заголовков можно подключить модуль (в нативной сборке с Ninja).", "import reactx;"},
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
                                 "(например, v0.1.0), а не по ветке, поэтому проект не сломается от новых изменений; --local — из исходников, "
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
                {"reactx.json", "Всё, что нужно проекту, в одном файле.",
                 "{\n"
                 "  \"name\": \"my-app\",\n"
                 "  \"template\": \"web\",\n"
                 "  \"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v0.1.0\" },\n"
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
                 "#include \"reactx/version.hpp\"\n\nstd::cout << rx::version();   // \"0.1.0\"\n"
                 "static_assert(REACTX_VERSION_MAJOR == 0);"},
                {"Обновиться", "Поменяйте rev в reactx.json на новый тег и пересоберите проект.",
                 "\"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v0.2.0\" }"},
            },
        },
        {
            "elements", "Элементы и props", "h(), дети, фрагменты, ключи.",
            {
                {"h()", "h(тег, props, дети) описывает узел; h(Компонент, props, дети) — компонент. Строка в детях — текст.",
                 "h(\"div\", {{\"className\", \"card\"}}, {\n"
                 "    h(\"h1\", {}, {\"Заголовок\"}),\n"
                 "    \"просто текст\",\n"
                 "    h(Counter, {{\"start\", 5}}),\n"
                 "})"},
                {"Props", "Значения: bool, числа, строки и обработчики. В компоненте читаются через props.get<T>(имя, запасное).",
                 "Element Hello(const Props& p) {\n"
                 "    return h(\"p\", {}, {\"Привет, \" + p.get<std::string>(\"name\", \"мир\")});\n"
                 "}"},
                {"Дети компонента", "Дети, переданные компоненту, доступны через props.children().",
                 "Element Card(const Props& p) { return h(\"div\", {{\"className\", \"card\"}}, p.children()); }"},
                {"Ключи и фрагменты", "key помогает reconciler'у узнавать элементы списка: при перестановке узлы переносятся, "
                                      "а не создаются заново. fragment() группирует детей без лишнего узла.",
                 "for (const auto& t : todos) items.push_back(h(TodoItem, {{\"key\", t.id}, {\"title\", t.title}}));\n"
                 "return fragment(std::move(items));"},
            },
        },
        {
            "hooks", "Хуки", "useState, useEffect, useRef, useMemo, useCallback.",
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
                {"Обработчики", "Callback — void(), KeyHandler — void(const KeyEvent&), InputHandler — void(const std::string&).",
                 "h(\"input\", {{\"value\", text},\n"
                 "             {\"onInput\", [setText](const std::string& t) { setText(t); }},\n"
                 "             {\"onKey\", [](const KeyEvent& e) { if (e.key == \"Enter\") submit(); }}})"},
                {"KeyEvent", "key — символ («a», «я») или имя: Enter, Escape, Tab, Backspace, Delete, Up, Down, Left, Right, "
                             "Home, End. Флаги ctrl, alt, shift.", ""},
                {"Всплытие", "Как в React: сначала обработчик элемента, затем всех его родителей. "
                             "Так Escape из кнопки внутри окна доходит до самого окна.", ""},
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
                {"События", "onClick, onInput(text), onChange, onKey(KeyEvent), onSubmit (форма не перезагружает страницу).", ""},
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
