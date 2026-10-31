#pragma once

// Documentation pages of the ReactX site (/docs, /docs/<slug>). The texts are
// plain data; examples/web_app.cpp renders them with ReactX itself.

#include <string_view>
#include <vector>

namespace site_docs {

// string_view, not std::string: the texts stay data in the .wasm instead of
// code that builds hundreds of strings at startup.
struct Section {
    std::string_view heading{};
    std::string_view text{};  // paragraphs separated by "\n\n"
    std::string_view code{};  // C++ (or shell) example, may be empty
};

struct Page {
    std::string_view slug;
    std::string_view title;
    std::string_view summary;
    std::vector<Section> sections;
};

inline const std::vector<Page>& pages() {
    static const std::vector<Page> all = {
        {
            "start", "Быстрый старт", "Что такое ReactX и первое приложение за три команды.",
            {
                {"Что это", "ReactX позволяет делать интерфейсы так же, как в React — компоненты, состояние, хуки, — но на C++. "
                            "Вы описываете, как должен выглядеть экран; когда состояние меняется, ReactX обновляет только то, "
                            "что действительно изменилось.\n\n"
                            "Один и тот же код компонентов работает в двух местах: в браузере (через WebAssembly) и в терминале "
                            "(текстовый интерфейс). JavaScript, Node и npm не нужны.", ""},
                {"1. Установка (одна строка)",
                 "На Linux (x86-64) всё ставится одной командой. Она скачивает reactx в ~/.local/bin, а затем reactx install "
                 "скачивает компилятор для браузера (Emscripten) и, если в системе их нет, CMake и Ninja в ~/.reactx — без sudo, "
                 "как npm. Нужны curl, git, python3 и tar, они есть почти в любом Linux.\n\n"
                 "reactx install --check показывает, что уже есть. Терминальным приложениям нужен ещё GCC 16+ из пакетного "
                 "менеджера. Другие системы — разделы ниже или Docker-образ.",
                 "curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh\n\n"
                 "# потом — проверить или доустановить:\n"
                 "reactx install --check\n"
                 "reactx install"},
                {"Установка вручную: Arch Linux",
                 "В Arch Linux компилятор и инструменты сборки ставятся через pacman одной командой:\n\n"
                 "- Компилятор GCC 16, CMake, Ninja и Git\n"
                 "- Emscripten для сборки в WebAssembly\n"
                 "- Готовый reactx из GitHub Releases либо сборка из исходников",
                 "# Установка инструментов\n"
                 "sudo pacman -S gcc cmake ninja git emscripten\n\n"
                 "# Скачать готовый CLI reactx:\n"
                 "# github.com/sargisis/ReactX/releases -> reactx-X.Y.Z-linux-x86_64.tar.gz\n"
                 "tar xzf reactx-*-linux-x86_64.tar.gz && cp reactx-*/bin/* ~/.local/bin/\n"
                 "reactx --version"},
                {"Установка: macOS",
                 "На macOS (Apple Silicon M1/M2/M3/M4 и Intel) всё устанавливается через Homebrew:\n\n"
                 "- brew install cmake ninja git\n"
                 "- GCC для нативных C++26 приложений или Apple Clang\n"
                 "- Emscripten для сборки веб-приложений (через brew или emsdk)",
                 "# 1. Установите инструменты и GCC через Homebrew:\n"
                 "brew install cmake ninja git gcc emscripten\n\n"
                 "# 2. Соберите CLI reactx из исходников:\n"
                 "git clone https://github.com/sargisis/ReactX.git && cd ReactX\n"
                 "cmake --preset release -DCMAKE_CXX_COMPILER=g++-14 && cmake --build --preset release\n"
                 "cmake --install build/release --prefix ~/.local     # ~/.local/bin/reactx"},
                {"Установка: Windows",
                 "На Windows для нативной поддержки C++26 и POSIX-терминала рекомендуется использовать WSL 2 (Windows Subsystem for Linux):\n\n"
                 "- WSL 2 обеспечивает 100% совместимость с Linux-тулчейном\n"
                 "- Браузер в Windows открывает http://127.0.0.1:8090/ автоматически\n"
                 "- Также можно работать через VS Code с расширением WSL или Dev Containers",
                 "# 1. В PowerShell (от администратора) установите WSL:\n"
                 "wsl --install\n\n"
                 "# 2. Внутри терминала WSL (Ubuntu/Arch) установите инструменты:\n"
                 "sudo apt update && sudo apt install -y cmake ninja-build gcc-16 git\n\n"
                 "# 3. Emscripten для веб-проектов:\n"
                 "git clone https://github.com/emscripten-core/emsdk.git ~/emsdk\n"
                 "~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest && source ~/emsdk/emsdk_env.sh\n\n"
                 "# 4. Скачайте reactx из Releases или соберите из исходников"},
                {"Установка: Ubuntu / Debian и Docker",
                 "Для Linux x86-64 доступен готовый архив из GitHub Releases, а также полностью настроенный официальный Docker-контейнер:",
                 "# Вариант А: бинарник из GitHub Releases\n"
                 "sudo apt update && sudo apt install -y cmake ninja-build git\n"
                 "tar xzf reactx-*-linux-x86_64.tar.gz && cp reactx-*/bin/* ~/.local/bin/\n\n"
                 "# Вариант Б: запуск через Docker (внутри уже есть GCC 16, Emscripten и reactx):\n"
                 "docker run --rm -it -u \"$(id -u):$(id -g)\" -v \"$PWD:/app\" -p 8090:8090 ghcr.io/sargisis/reactx\n"
                 "reactx new my-app && cd my-app && reactx dev --host 0.0.0.0"},
                {"2. Создайте приложение", "Появится папка с работающим примером: веб-приложение со счётчиком и двумя страницами. "
                                           "С --terminal — программа для терминала.",
                 "reactx new my-app\ncd my-app"},
                {"3. Запустите", "reactx dev собирает приложение, запускает локальный сервер и пересобирает страницу при каждом "
                                 "сохранении. Откройте http://127.0.0.1:8090/ и правьте src/main.cpp.",
                 "reactx dev"},
                {"Первый компонент", "Компонент — это функция: получает props (структуру) и возвращает элементы. useState хранит "
                                     "значение между отрисовками; когда оно меняется, компонент перерисовывается.",
                 "#include \"reactx/dom.hpp\"\n#include \"reactx/reactx.hpp\"\nusing namespace rx;\n\n"
                 "struct CounterProps {\n    int start = 0;\n};\n\n"
                 "Element Counter(const CounterProps& p) {\n"
                 "    auto [count, setCount] = useState(p.start);\n"
                 "    return h(\"button\", {{\"onClick\", [setCount] { setCount([](int c) { return c + 1; }); }}},\n"
                 "             {\"Нажато: \" + std::to_string(count)});\n"
                 "}\n\n"
                 "int main() { mountDom(h(Counter, {.start = 0})); }"},
                {"С ИИ-помощником", "Нейросети ReactX пока не знают: он новый. В каждом проекте из reactx new есть AGENTS.md — "
                                    "Codex, Cursor и Claude Code читают его сами. В обычном чате начните с этой фразы:",
                 "Пишем на ReactX — это C++26-фреймворк (не JavaScript): https://github.com/sargisis/ReactX.\n"
                 "Сначала прочитай https://sargisis.github.io/ReactX/llms.txt — там весь API.\n"
                 "Ничего, чего там нет, не придумывай."},
                {"Сборка самого ReactX", "Только если хотите менять фреймворк: нужны GCC 16, CMake 3.28+ и Ninja.",
                 "cmake --preset debug -DCMAKE_CXX_COMPILER=g++-16 && cmake --build --preset debug\n"
                 "ctest --preset debug -LE slow"},
            },
        },
        {
            "cli", "CLI reactx", "Новый проект, разработка с автообновлением, сборка, зависимости.",
            {
                {"Установка", "Одной строкой на Linux x86-64: готовый reactx, затем reactx install скачивает Emscripten (и CMake с Ninja, "
                              "если нужно) в ~/.reactx; reactx build и dev находят их сами. "
                              "Вручную: готовый reactx для Linux x86-64 лежит в GitHub Releases — распакуйте архив и добавьте bin/ в PATH. "
                              "Для веб-проектов хватит Emscripten, CMake и Ninja, для терминальных нужен ещё GCC 16. "
                              "Можно и собрать из исходников: cmake --install кладёт reactx и reactx_serve в bin.",
                 "curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh\n\n"
                 "# вручную: github.com/sargisis/ReactX/releases → reactx-X.Y.Z-linux-x86_64.tar.gz\n"
                 "tar xzf reactx-*-linux-x86_64.tar.gz && cp reactx-*/bin/* ~/.local/bin/\n\n"
                 "# или из исходников\n"
                 "cmake --preset release && cmake --build --preset release\n"
                 "cmake --install build/release --prefix ~/.local     # ~/.local/bin/reactx"},
                {"Docker и devcontainer", "Образ ghcr.io/sargisis/reactx содержит GCC 16, Emscripten и reactx — ставить ничего не нужно. "
                                          "В контейнере запускайте reactx dev --host 0.0.0.0, иначе страница не откроется снаружи. "
                                          "reactx new кладёт в проект .devcontainer/ с образом той же версии: VS Code "
                                          "(«Reopen in Container») и GitHub Codespaces откроют проект сразу со всеми инструментами.",
                 "docker run --rm -it -u \"$(id -u):$(id -g)\" -v \"$PWD:/app\" -p 8090:8090 ghcr.io/sargisis/reactx\n"
                 "reactx new my-app && cd my-app && reactx dev --host 0.0.0.0"},
                {"Редактор", "Ошибки в редакторе показывает clangd: он понимает C++26 как компилятор и берёт флаги из настоящей "
                             "сборки. IntelliSense расширения C/C++ от Microsoft C++26 не знает и подчёркивает правильный код. "
                             "reactx new кладёт в проект .clangd и .vscode/: поставьте расширение clangd и один раз выполните "
                             "reactx build. Веб-проекты используют clangd из Emscripten — ставить ничего не нужно.",
                 "code --install-extension llvm-vs-code-extensions.vscode-clangd\nreactx build   # пишет compile_commands.json для clangd"},
                {"Новый проект", "reactx new создаёт папку с reactx.json (манифест, как package.json), CMakeLists.txt, "
                                 "CMakePresets.json и src/main.cpp. ReactX подключается с GitHub по тегу релиза "
                                 "(например, v0.8.3), а не по ветке, поэтому проект не сломается от новых изменений; --local — из исходников, "
                                 "из которых собран сам CLI. --terminal — терминальное приложение вместо веба.",
                 "reactx new my-app\ncd my-app"},
                {"reactx dev", "Собирает веб-версию, запускает встроенный сервер на C++ (тот же, что reactx_serve) и следит за "
                               "src/, public/ и манифестом. После каждой удачной пересборки страница в браузере обновляется "
                               "сама; если код не собрался — остаётся прошлая версия, а ошибка видна в терминале.",
                 "reactx dev              # http://127.0.0.1:8090/\nreactx dev --port 3000\n"
                 "reactx dev --host 0.0.0.0   # доступен с других устройств и из контейнера"},
                {"Свой бэкенд: --proxy", "Если API пишется на Flask, Django или Express, reactx dev перешлёт ему запросы под "
                                         "префиксом (по умолчанию /api) и вернёт ответ как есть — как server.proxy в Vite. "
                                         "Страница и API тогда на одном адресе, CORS не нужен. Только http://; если бэкенд "
                                         "не запущен — ответ 502 с причиной.",
                 "reactx dev --proxy http://127.0.0.1:5000             # /api → Flask\n"
                 "reactx dev --proxy /graphql=http://127.0.0.1:4000    # свой префикс\n\n"
                 "# или в reactx.json:\n"
                 "\"web\": { \"base\": \"/\", \"proxy\": { \"/api\": \"http://127.0.0.1:5000\" } }"},
                {"Браузер и терминал", "reactx build --native собирает те же компоненты для терминала. mountDom есть только в "
                                       "браузерной сборке (нативно — понятная ошибка компиляции), поэтому main выбирает точку входа.",
                 "int main() {\n#ifdef __EMSCRIPTEN__\n    mountDom(h(App));\n#else\n    TerminalApp app;   // reactx/terminal.hpp\n"
                 "    app.run(h(App));\n#endif\n}"},
                {"reactx build", "Сборка для публикации: build/web/dist — обычные файлы для любого статического хостинга "
                                 "(404.html уже лежит рядом с index.html для маршрутов). Для терминального проекта — "
                                 "build/native/<имя>.", "reactx build"},
                {"Зависимости", "reactx add записывает git-репозиторий в reactx.json; CMake проекта подключает его через "
                                "FetchContent, а --target сразу связывает библиотеку с приложением.",
                 "reactx add fmt https://github.com/fmtlib/fmt.git --rev 11.0.2 --target fmt::fmt\nreactx remove fmt"},
                {"reactx update", "Переводит проект на новейший релиз ReactX: находит теги в репозитории из reactx.json, "
                                  "выбирает самый новый по SemVer (без -rc) и записывает его в rev. --check только проверяет, "
                                  "--to — конкретная версия. Следующий reactx build скачает её сам.",
                 "reactx update --check   # ReactX v0.8.2 → v0.8.3 is available\n"
                 "reactx update           # записывает v0.8.3 в reactx.json\n"
                 "reactx update --to v0.3.0\n"
                 "reactx build"},
                {"reactx.json", "Всё, что нужно проекту, в одном файле.",
                 "{\n"
                 "  \"name\": \"my-app\",\n"
                 "  \"template\": \"web\",\n"
                 "  \"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v0.8.3\" },\n"
                 "  \"dependencies\": { \"fmt\": { \"git\": \"...\", \"rev\": \"11.0.2\", \"target\": \"fmt::fmt\" } },\n"
                 "  \"web\": { \"base\": \"/\", \"proxy\": { \"/api\": \"http://127.0.0.1:5000\" } }\n"
                 "}"},
            },
        },
        {
            "versions", "Версии и релизы", "Как нумеруются версии ReactX и как обновиться.",
            {
                {"SemVer", "Версии — MAJOR.MINOR.PATCH (Semantic Versioning), теги в git — vMAJOR.MINOR.PATCH. "
                           "PATCH — исправления, MINOR — новые возможности, MAJOR — несовместимые изменения. "
                           "Пока MAJOR = 0, API ещё может меняться: несовместимые изменения поднимают MINOR. "
                           "Что изменилось в каждой версии — в CHANGELOG.ru.md.", ""},
                {"Версия в коде", "Номер версии записан в одном месте — project(ReactX VERSION ...) в CMakeLists.txt; "
                                  "из него генерируется reactx/version.hpp.",
                 "#include \"reactx/version.hpp\"\n\nstd::println(\"{}\", rx::version());   // \"0.8.3\"\n"
                 "static_assert(REACTX_VERSION_MAJOR == 0);"},
                {"Обновиться", "reactx update находит новейший релиз и записывает его в reactx.json; reactx build скачивает его. "
                               "Вручную — поменяйте rev на новый тег.",
                 "reactx update\n# или в reactx.json:\n\"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v0.8.3\" }"},
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
                {"Лямбда-компонент", "Лямбда без захватов — такой же компонент, как функция: тип props берётся из её "
                                     "параметра. Данные передавайте через props, а не захватом.",
                 "const auto Badge = [](const BadgeProps& p) -> Element {\n"
                 "    return h(\"span\", {{\"className\", \"badge\"}}, {p.label});\n};\n\n"
                 "h(Badge, {.label = \"new\"})"},
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
            "hooks", "Хуки", "useState, useEffect, useReducer, useStore, useMemo, таймеры, клавиши, хранилище.",
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
                                    "Нативно — в памяти, пока работает программа. Поддерживает строки, числа, bool. Третий аргумент "
                                    "узнаёт о неудачном сохранении (квота, приватный режим); rx::storageRead / storageWrite "
                                    "возвращают ошибки как std::expected.",
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
                {"createStore и useStore", "Состояние вне дерева, общее для любых компонентов: корзина, пользователь, настройки "
                                           "(как Zustand). useStore(store) перерисовывает при каждом изменении; с функцией-селектором — "
                                           "только когда меняется выбранная часть. Менять — в потоке интерфейса: в обработчиках, эффектах, "
                                           "ответах fetch.",
                 "const auto Cart = createStore<std::vector<Item>>({});\n\n"
                 "Cart.update([](auto items) { items.push_back(milk); return items; });\n"
                 "const auto& items = useStore(Cart);\n"
                 "auto count = useStore(Cart, [](const auto& items) { return items.size(); });"},
                {"useId", "Id, уникальный в приложении и одинаковый при каждой отрисовке: связать подпись с полем, aria-атрибуты.",
                 "std::string id = useId();\n"
                 "h(\"label\", {{\"for\", id}}, {\"Имя\"}), h(\"input\", {{\"id\", id}})"},
                {"errorBoundary", "Если дети бросили исключение при отрисовке — сразу или позже, при обновлении состояния, — "
                                  "вместо них показывается запасной вариант, а остальное приложение работает. retry() пробует снова. "
                                  "Исключения в обработчиках событий и эффектах не ловятся, как в React.",
                 "errorBoundary([](const RenderError& e) {\n"
                 "    return h(\"div\", {}, {\"Ошибка: \" + e.message,\n"
                 "                           h(\"button\", {{\"onClick\", e.retry}}, {\"Ещё раз\"})});\n"
                 "}, {h(Chart, {.rows = rows})})"},
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
                {"mountDom", "Монтирует приложение в <div id=\"app\"> страницы (заменяя то, что там было: текст загрузки, пререндер) "
                             "и живёт после main().",
                 "int main() { mountDom(h(App)); }"},
                {"Пререндер", "rx::renderToString (reactx/html.hpp) даёт HTML компонентов без браузера; эффекты не запускаются. "
                              "Вставьте его в <div id=\"app\"> при сборке — и страница видна до загрузки .wasm; потом mountDom "
                              "заменяет его живым приложением, так что первый рендер должен рисовать то же. Так сделана главная этого сайта.",
                 "std::string html = renderToString(h(Home));\n// \"<main class=\\\"home\\\"><h1>Hello</h1>...</main>\""},
                {"Props в DOM", "className → class, style — строка CSS, value — управляемое поле ввода, checked/disabled — "
                                "свойства, остальное — атрибуты (true — есть, false — убрать). onChange флажка получает \"true\" или "
                                "\"false\"; rx::boolHandler превращает это в bool.",
                 "h(\"input\", {{\"type\", \"checkbox\"}, {\"checked\", done},\n"
                 "              {\"onChange\", boolHandler([setDone](bool on) { setDone(on); })}})"},
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
                {"В терминале", "Терминальные приложения тоже ходят в сеть: нужен полный URL с http:// (HTTPS нет). Запрос идёт "
                                "в своём потоке, ответ приходит в потоке интерфейса — TerminalApp перерисует экран сам.",
                 "auto weather = useFetch(\"http://127.0.0.1:8091/api/todos\");"},
                {"fetch", "Любой метод и тело; ответ приходит в колбэк.",
                 "fetch({.method = \"POST\", .url = \"/api/todos\", .body = json::object({{\"title\", \"Молоко\"}}).dump()},\n"
                 "      [](const FetchResponse& r) { if (r.ok()) ... });"},
                {"Заголовки, таймаут, отмена", "У запроса бывают заголовки и срок ожидания (тогда статус 0 и error \"timeout after N ms\"); "
                                               "заголовки ответа — r.header(имя). fetch возвращает FetchHandle: после cancel() колбэк не "
                                               "вызовется. useFetch принимает и целый запрос и отменяет те, что заменил.",
                 "auto me = useFetch({.url = \"/api/me\", .headers = {{\"Authorization\", \"Bearer \" + token}}, .timeout = 5s});\n"
                 "FetchHandle req = fetch({.url = \"/api/items\"}, [](const FetchResponse& r) { r.header(\"etag\"); });\n"
                 "req.cancel();"},
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
                {"Строгое чтение", "get<T> подставляет запасное значение; для данных извне (импорт, сервер) require<T> "
                                   "возвращает std::expected: нет поля, другой тип или 2.9 для int — ошибка с именем поля.",
                 "auto id = task.require<int>(\"id\");\n"
                 "if (!id) return showError(id.error().what());   // field \"id\": expected an integer that fits"},
                {"Бэкенд (Flask)", "ReactX работает с Flask и другими REST API через fetch / useFetch. "
                                   "В разработке запросы проксируются через reactx dev --proxy. "
                                   "Подробно о проверенных возможностях и текущих ограничениях — в разделе «Бэкенд и Flask».", ""},
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
                {"Прокси", "server.proxy пересылает запросы под префиксом на другой сервер; rx::net::forward отправляет один "
                           "запрос и возвращает std::expected<Response, HttpError> (502 — не дозвонились, 504 — не ответил).",
                 "server.proxy(\"/api\", *rx::net::parseUpstream(\"http://127.0.0.1:5000\"));\n\n"
                 "auto reply = rx::net::forward(req, {.host = \"127.0.0.1\", .port = 5000});\n"
                 "if (!reply) std::println(\"{}: {}\", reply.error().status, reply.error().message);"},
                {"Важно", "Сервер рассчитан на разработку на своём компьютере: нет HTTPS и защиты от перегрузки.", ""},
            },
        },
        {
            "backend", "Бэкенд и Flask", "Интеграция с Flask и REST API: возможности, проверенные сценарии и ограничения.",
            {
                {"Связка с Flask",
                 "ReactX компилируется в WebAssembly и работает в браузере как клиентское приложение. Вы можете подключать его к любому бэкенду — Flask, FastAPI, Django, Express или Go.\n\n"
                 "При разработке встроенный dev-сервер проксирует запросы к API на ваш локальный бэкенд: reactx dev --proxy http://127.0.0.1:5000. В продакшене отдачу файлов клиента (HTML, JS, WASM) и обработку API выполняет сам бэкенд (Flask), Nginx или внешний обратный прокси.",
                 "# app.py — пример Flask API\n"
                 "from flask import Flask, jsonify, request\n\n"
                 "app = Flask(__name__)\n\n"
                 "@app.route(\"/api/todos\", methods=[\"GET\", \"POST\"])\n"
                 "def todos():\n"
                 "    if request.method == \"POST\":\n"
                 "        item = request.get_json()\n"
                 "        return jsonify({\"id\": 1, \"title\": item.get(\"title\")}), 201\n"
                 "    return jsonify([{\"id\": 1, \"title\": \"Купить хлеб\"}])\n\n"
                 "if __name__ == \"__main__\":\n"
                 "    app.run(port=5000)"},
                {"Пример клиента на ReactX",
                 "На стороне клиента обращение к API выполняется через useFetch (загрузка данных при монтировании) или fetch (отправка данных по действиям пользователя).",
                 "// Получение списка задач из Flask\n"
                 "auto res = useFetch(\"/api/todos\");\n"
                 "if (res.loading) return h(\"p\", {}, {\"Загрузка...\"});\n"
                 "if (!res.ok()) return h(\"p\", {}, {\"Ошибка: \" + res.errorText()});\n\n"
                 "// Отправка новой задачи (POST)\n"
                 "fetch({\n"
                 "    .method = \"POST\",\n"
                 "    .url = \"/api/todos\",\n"
                 "    .body = json::object({{\"title\", \"Новая задача\"}}).dump()\n"
                 "}, [](const FetchResponse& r) {\n"
                 "    if (r.ok()) std::println(\"Задача добавлена!\");\n"
                 "});"},
                {"Что работает и проверено",
                 "Взаимодействие с внешними бэкендами стабильно и проверено на практике:\n\n"
                 "- useFetch / fetch в браузере. Под капотом обычный fetch() браузера, так что надёжность на уровне браузера. Это проверено тестами и реальными проектами: TaskFlow и PulseBoard работали с бэкендами через API.\n"
                 "- GET, POST, PUT, PATCH, DELETE с JSON-телом, статусы, ошибки сети, разбор JSON через std::expected, без падений на кривом ответе.\n"
                 "- Прокси reactx dev --proxy проверен сквозными тестами с настоящим бэкендом и фаззингом. Нашли и исправили DoS-уязвимость.",
                 ""},
                {"Чего не хватает (реальные пробелы)",
                 "В текущей реализации есть архитектурные ограничения, которые важно учитывать:\n\n"
                 "- Своих заголовков запроса. Нельзя передать Authorization: Bearer <token>. Для API с JWT это главная проблема. Авторизация через cookie работает (браузер отправляет их сам на тот же адрес), а через токен в заголовке — нет.\n"
                 "- Заголовков ответа: нельзя прочитать, например, Location или заголовки пагинации.\n"
                 "- Отмены запроса и своего таймаута.\n"
                 "- WebSocket / потоковых ответов (SSE): чат в реальном времени можно сделать только опросом через useInterval.\n"
                 "- В терминальных приложениях только http://, без HTTPS.\n"
                 "- Прокси reactx dev — только для разработки. В продакшене API раздаёт Flask, nginx и т. п. (как в примере выше), а не ReactX.",
                 ""},
            },
        },
    };
    return all;
}

}  // namespace site_docs
