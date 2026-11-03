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
                 "как npm. Нужны curl, git, python3, tar и xz (xz-utils в Debian/Ubuntu), они есть почти в любом Linux.\n\n"
                 "reactx install --check показывает, что уже есть. Терминальным приложениям нужен ещё GCC 16+ из пакетного "
                 "менеджера. Другие системы — разделы ниже или Docker-образ.",
                 "curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh"},
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
                 "- GCC 16 или новее для нативной сборки (Apple Clang пока не собирает ReactX; проверьте g++-16 --version)\n"
                 "- Emscripten для сборки веб-приложений (через brew или emsdk)",
                 "# 1. Установите инструменты и GCC через Homebrew:\n"
                 "brew install cmake ninja git gcc emscripten\n\n"
                 "# 2. Соберите CLI reactx из исходников:\n"
                 "git clone https://github.com/sargisis/ReactX.git && cd ReactX\n"
                 "cmake --preset release -DCMAKE_CXX_COMPILER=g++-16 && cmake --build --preset release\n"
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
                                 "сохранении. Откройте http://127.0.0.1:8090/ и правьте src/app.cpp.",
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
                {"Справка", "Каждая команда объясняет себя сама.", "reactx help\nreactx add --help"},
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
                                 "CMakePresets.json, src/ (app.cpp, main.cpp) и tests/. ReactX подключается с GitHub по тегу релиза "
                                 "(например, v1.0.0), а не по ветке, поэтому проект не сломается от новых изменений; --local — из исходников, "
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
                {"reactx test", "Собирает все tests/*.cpp нативно (с исходниками приложения, кроме main.cpp) и запускает их. "
                                "Тест рисует приложение на терминале в памяти и действует как пользователь: нажимает на "
                                "надпись, печатает в поле с фокусом, жмёт клавиши, а потом проверяет, что видно на экране. "
                                "reactx new кладёт компоненты в src/app.cpp, а пример теста — в tests/app_tests.cpp. "
                                "reactx test <имя> запускает тесты, в имени которых есть это слово.",
                 "#include \"app.hpp\"\n"
                 "#include \"reactx/testing.hpp\"\n\n"
                 "RX_TEST(the_button_counts_clicks) {\n"
                 "    rx::testing::Screen s(rx::h(App));\n"
                 "    CHECK(s.click(\"Clicked 0 times\"));\n"
                 "    s.type(\"Аня\");          // в поле с фокусом\n"
                 "    s.press(\"Ctrl+s\");      // \"Enter\", \"Tab\", \"Up\", ...\n"
                 "    CHECK(s.shows(\"Clicked 1 times\"));\n"
                 "}\n\n"
                 "// reactx test           -> OK, 2 tests passed"},
                {"reactx build", "Сборка для публикации: build/web/dist — обычные файлы для любого статического хостинга "
                                 "(404.html уже лежит рядом с index.html для маршрутов). Для терминального проекта — "
                                 "build/native/<имя>. Веб-сборка ещё печатает размер .wasm и его самые большие части "
                                 "и называет код, который тянет std::format (~100 КБ). set(REACTX_WEB_LTO ON) выше FetchContent в "
                                 "CMakeLists.txt включает оптимизацию при компоновке: .wasm на ~5% меньше, но каждая "
                                 "пересборка в reactx dev дольше на пару секунд.",
                 "reactx build\n# index.wasm: 122 KB (the page shows nothing until it is downloaded)\n"
                 "#   biggest parts: src/app.cpp 23%, ReactX reconciler.cpp 15%, the C++ library 12%, ..."},
                {"Зависимости", "reactx add записывает git-репозиторий в reactx.json; CMake проекта подключает его через "
                                "FetchContent, а --target сразу связывает библиотеку с приложением.",
                 "reactx add fmt https://github.com/fmtlib/fmt.git --rev 11.0.2 --target fmt::fmt\nreactx remove fmt"},
                {"reactx update", "Переводит проект на новейший релиз ReactX: находит теги в репозитории из reactx.json, "
                                  "выбирает самый новый по SemVer (без -rc) и записывает его в rev. --check только проверяет, "
                                  "--to — конкретная версия. Следующий reactx build скачает её сам.",
                 "reactx update --check   # ReactX v1.0.0 → v1.0.1 is available\n"
                 "reactx update           # записывает v1.0.1 в reactx.json\n"
                 "reactx update --to v0.3.0\n"
                 "reactx build"},
                {"reactx.json", "Всё, что нужно проекту, в одном файле.",
                 "{\n"
                 "  \"name\": \"my-app\",\n"
                 "  \"template\": \"web\",\n"
                 "  \"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v1.0.1\" },\n"
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
                           "До 1.0 несовместимые изменения поднимали MINOR; с 1.0 — только MAJOR. "
                           "Что изменилось в каждой версии — в CHANGELOG.ru.md.", ""},
                {"Стабильность API", "Публичный API — то, что объявлено в include/reactx/ вне пространств detail, команды и "
                                     "опции reactx, reactx.json и CMake-функции и цели. С 1.0 он меняется только в MAJOR-"
                                     "релизах. Ничего не переименовывается на месте: добавляется лучшее имя, старое помечается "
                                     "устаревшим (комментарий в коде, документация, раздел CHANGELOG «Устарело») и работает "
                                     "до 2.0. Устарели пока: ui::ListProps::items (используйте .children) и "
                                     "ui::RadioGroupProps::row (используйте .direction = ui::Direction::Row). Рефлексия C++26 "
                                     "остаётся опциональной (reactx::reflection, нативные сборки) и пока вне этого обещания.", ""},
                {"Версия в коде", "Номер версии записан в одном месте — project(ReactX VERSION ...) в CMakeLists.txt; "
                                  "из него генерируется reactx/version.hpp.",
                 "#include \"reactx/version.hpp\"\n\nstd::println(\"{}\", rx::version());   // \"1.0.1\"\n"
                 "static_assert(REACTX_VERSION_MAJOR == 1);"},
                {"Обновить reactx", "Два шага: сам инструмент, потом каждый проект. reactx upgrade заменяет reactx новейшим релизом "
                                    "(проверяя SHA-256); до v0.14.0 — запустите установщик ещё раз. Раз в день reactx сам проверяет "
                                    "в фоне, не вышла ли новая версия, и говорит об этом (выключить: REACTX_NO_UPDATE_CHECK=1).",
                 "reactx upgrade      # v0.14.0 и новее\n"
                 "# старее: curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh\n"
                 "cd my-app && reactx update && reactx build"},
                {"Обновить проект", "reactx update находит новейший релиз и записывает его в reactx.json; reactx build скачивает его. "
                               "Вручную — поменяйте rev на новый тег.",
                 "reactx update\n# или в reactx.json:\n\"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v1.0.1\" }"},
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
                             "Равное значение не вызывает перерисовку. Подходит любой тип, и std::vector своих структур тоже: "
                             "если есть == (свой или у всех частей), равное значение пропускается, если нет — каждый set перерисовывает.",
                 "auto [name, setName] = useState(std::string{});\nsetName(\"Аня\");\nsetCount([](int c) { return c + 1; });"},
                {"useEffect", "Запускается после отрисовки. deps(...) — когда перезапускать; deps() — один раз. "
                              "Может вернуть функцию очистки.",
                 "useEffect([id] {\n    subscribe(id);\n    return [id] { unsubscribe(id); };\n}, deps(id));"},
                {"useRef", "Изменяемое значение, которое живёт между перерисовками и не вызывает их.",
                 "int& renders = useRef(0);\n++renders;"},
                {"useMemo и useCallback", "useMemo пересчитывает значение только при смене deps. useCallback сохраняет "
                                          "«личность» обработчика, чтобы memo-компоненты не перерисовывались. Подходит любая сигнатура, "
                                          "и onOpen(id) строки тоже.",
                 "const auto& sorted = useMemo([&] { return sortItems(items); }, deps(items));\n"
                 "Callback onSave = useCallback([setSaved] { setSaved(true); }, deps());\n"
                 "std::function<void(int)> onOpen = useCallback([setOpen](int id) { setOpen(id); }, deps());"},
                {"useDebounce", "Значение, но только после того, как оно перестало меняться на заданное время (std::chrono): "
                                "для поиска и ввода текста. Таймер работает в потоке интерфейса и отменяется сам.",
                 "auto [query, setQuery] = useState(std::string{});\n"
                 "std::string debounced = useDebounce(query, 300ms);"},
                {"useLocalStorage", "Состояние, которое переживает перезагрузку страницы: хранится в localStorage. "
                                    "Нативно — в памяти, пока работает программа. Поддерживает строки, числа, bool. Третий аргумент "
                                    "узнаёт о неудачном сохранении (квота, приватный режим); rx::storageRead / storageWrite "
                                    "возвращают ошибки как std::expected.",
                 "auto [theme, setTheme] = useLocalStorage(\"rx-theme\", std::string(\"dark\"));"},
                {"useTitle", "Заголовок вкладки браузера, пока компонент смонтирован; при размонтировании возвращается прежний, "
                             "так что каждая страница роутера называет себя сама; у заранее отрисованных страниц он попадает в <title>. "
                             "Нативно только хранит текст.",
                 "useTitle(\"Заказы (\" + std::to_string(count) + \")\");"},
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
                                  "вместо них показывается запасной вариант, а остальное приложение работает. retry() пробует снова; до этого "
                                  "граница показывает запасной вариант, даже когда меняются параметры маршрута, поэтому на странице "
                                  "маршрута дайте ей путь как ключ: errorBoundary(fallback, {h(Page)}, router::useLocation()). "
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
                {"Прокрутка", "onScroll получает ScrollEvent: top, scrollHeight, clientHeight и др. (в браузере пиксели, в "
                              "терминале строки у box с overflow: \"scroll\"); nearBottom() — пора подгружать ещё. Не всплывает: "
                              "срабатывает только у прокручиваемого элемента. ref.scrollIntoView() показывает элемент ({\"scrollIntoView\", selected} "
                              "без ref); ref.scrollMetrics(), scrollTo(top) и scrollBy(delta) читают и двигают прокрутку — например, "
                              "чтобы читатель не потерял место, когда сверху приходят старые сообщения.",
                 "h(\"div\", {{\"style\", \"height: 400px; overflow: auto\"},\n"
                 "            {\"onScroll\", [loadMore](const ScrollEvent& e) { if (e.nearBottom()) loadMore(); }}}, rows)"},
            },
        },
        {
            "terminal", "Терминал", "TerminalApp, теги и Flexbox на Yoga.",
            {
                {"Запуск", "TerminalApp включает raw-режим, рисует дерево и обрабатывает клавиши до Ctrl+C.",
                 "TerminalApp app;\napp.run(h(App, {{\"onQuit\", Callback([&app] { app.quit(); })}}));"},
                {"Теги", "box — flex-контейнер, text — текст с переносом, input — поле ввода, textarea — поле в несколько "
                         "строк (rows), button — кнопка. Tab и стрелки переключают фокус, Enter/пробел нажимают.", ""},
                {"Поля ввода", "input и textarea редактируют там, где стоит курсор: ← →, Home / End, Ctrl+← → по словам, "
                               "Backspace и Delete. textarea ещё принимает Enter (новая строка) и ↑ ↓ (между строками), "
                               "переносит длинные строки и прокручивается к курсору. Эмодзи и китайские, японские, корейские "
                               "символы занимают две колонки, поэтому рамки и колонки таблиц не съезжают; 👍🏽 или флаг для "
                               "курсора и Backspace — один символ.",
                 "ui::textarea({.value = note, .onChange = setNoteFn, .label = \"Заметка\", .rows = 6})\n"
                 "// или сам тег:\n"
                 "h(\"textarea\", {{\"value\", note}, {\"rows\", 6}, {\"onInput\", setNoteFn}})"},
                {"Flexbox", "Раскладку считает Yoga (движок React Native): direction, justifyContent, alignItems, flexGrow, "
                            "width/height в клетках или процентах, padding, margin, gap, position: absolute.",
                 "h(\"box\", {{\"flexGrow\", 1}, {\"justifyContent\", \"center\"}, {\"alignItems\", \"center\"}}, {\n"
                 "    h(\"box\", {{\"border\", true}, {\"title\", \"Окно\"}, {\"padding\", 1}}, {h(\"text\", {}, {\"Привет\"})}),\n"
                 "})"},
                {"Прокрутка", "Что выше терминала, обрезается, поэтому длинные списки кладут в box с overflow: \"scroll\". "
                              "В колонке он высотой со свои строки, а если они не помещаются — занимает оставшиеся; колёсико и PageUp/PageDown прокручивают его, элемент в "
                              "фокусе остаётся на виду, рамка показывает положение, onScroll сообщает его в строках. "
                              "ui::list({.children = items, .scroll = true}) прокручивается внутри своей рамки. Окна и уведомления — "
                              "position: \"fixed\", они видны на экране, какой бы длинной ни была страница.",
                 "h(\"box\", {{\"flexGrow\", 1}}, {\n"
                 "    h(\"text\", {}, {\"Логи\"}),\n"
                 "    h(\"box\", {{\"overflow\", \"scroll\"}, {\"border\", true}}, std::move(lines)),\n"
                 "})"},
            },
        },
        {
            "browser", "Браузер", "WebAssembly, DOM, CSS.",
            {
                {"mountDom", "Монтирует приложение в <main id=\"app\"> страницы (<div id=\"app\"> в старых шаблонах) (заменяя то, что там было: текст загрузки, пререндер) "
                             "и живёт после main(). Повторный mountDom в тот же контейнер сначала размонтирует старое приложение "
                             "(его таймеры и сокеты останавливаются).",
                 "int main() { mountDom(h(App)); }"},
                {"Пререндер", "Перечислите маршруты в reactx.json — и страница видна до загрузки .wasm: reactx build запускает ту же "
                              "программу под Node, где mountDom пишет HTML каждого маршрута (renderToString) в dist/index.html, "
                              "dist/docs/faq.html, … и завершается. Код менять не нужно; эффекты там не запускаются. В браузере mountDom "
                              "заменяет его живым приложением (без гидратации). Остальные маршруты получают 404.html / 200.html — пустую страницу.",
                 "\"web\": { \"base\": \"/\", \"prerender\": [\"/\", \"/docs/faq\"] }"},
                {"Мышь, касания и перетаскивание", "onPointerDown, onPointerMove и onPointerUp получают PointerEvent: x и y в окне, "
                                          "localX и localY от угла элемента, button, buttons и type (mouse, touch, pen). "
                                          "onMouseEnter и onMouseLeave следят за указателем и не всплывают. Те же обработчики работают в "
                                          "терминале (в клетках). {\"pointerCapture\", true} держит движения и отпускание на элементе после "
                                          "нажатия, даже за его пределами (слайдеры); без него их получает элемент под указателем (куда "
                                          "бросают). ref.focus() даёт фокус элементу из кода. Демо-канбан — один исходник для обоих.",
                 "h(\"div\", {{\"pointerCapture\", true},\n"
                 "          {\"onPointerDown\", [setGrab](const PointerEvent& e) { setGrab(e.localX); }},\n"
                 "          {\"onPointerMove\", [grab, setPos](const PointerEvent& e) {\n"
                 "              if (e.buttons) setPos(e.x - grab);\n"
                 "          }}})"},
                {"Ref и JavaScript", "useNodeRef() даёт настоящий элемент: передайте его в prop ref и пользуйтесь в useEffect "
                                     "(он заполнен до эффектов и снова 0, когда элемент убран). rx::domElement (reactx/js.hpp) "
                                     "превращает его в emscripten::val — объект самой страницы: вызывайте его методы, читайте "
                                     "размеры или отдайте JavaScript-библиотеке. rx::jsValue делает JS-объект из JSON. "
                                     "Приложение линкуется с reactx::js (подключает embind; остальные приложения его не несут). "
                                     "Всё вместе — в демо с Chart.js.",
                 "auto canvas = useNodeRef();\n"
                 "auto& chart = useRef(emscripten::val::undefined());\n"
                 "useEffect([canvas, &chart] {\n"
                 "    chart = emscripten::val::global(\"Chart\").new_(domElement(canvas), jsValue(config));\n"
                 "    return [&chart] { chart.call<void>(\"destroy\"); };\n"
                 "}, deps());\n"
                 "return h(\"canvas\", {{\"ref\", canvas}});"},
                {"Props в DOM", "className → class, style — строка CSS, value — управляемое поле ввода, checked/disabled — "
                                "свойства, остальное — атрибуты (true — есть, false — убрать). onChange флажка получает \"true\" или "
                                "\"false\"; rx::boolHandler превращает это в bool.",
                 "h(\"input\", {{\"type\", \"checkbox\"}, {\"checked\", done},\n"
                 "              {\"onChange\", boolHandler([setDone](bool on) { setDone(on); })}})"},
                {"События", "onClick, onInput(text), onChange, onKey(KeyEvent), onSubmit (форма не перезагружает страницу), "
                            "onFocus, onBlur, onScroll(ScrollEvent).", ""},
                {"CSS", "Свои стили кладите в public/style.css проекта: он копируется рядом со страницей.", ""},
                {"Безопасность", "Текст — всегда текст, а не HTML. Ссылки с адресом javascript: блокируются.", ""},
                {"SVG", "Теги SVG (svg, rect, circle, path, line, g, ...) создаются в пространстве имён SVG сами. "
                        "Текст и ссылки внутри SVG — «svg:text», «svg:a» (такие теги есть и в HTML).",
                 "h(\"svg\", {{\"viewBox\", \"0 0 100 40\"}}, {\n"
                 "    h(\"rect\", {{\"width\", \"60\"}, {\"height\", \"40\"}, {\"fill\", \"orange\"}}),\n"
                 "    h(\"svg:text\", {{\"x\", \"64\"}, {\"y\", \"24\"}}, {\"60%\"}),\n"
                 "})"},
            },
        },
        {
            "ui", "Компоненты rx::ui", "Готовые кнопки, поля, списки выбора, окна, уведомления, подсказки и списки с темой.",
            {
                {"Компоненты", "button, input, checkbox, select, tabs, modal, confirm, list и listItem, tooltip, card, stack и text. "
                               "Один и тот же код работает в терминале и в браузере. Свойства — назначенные инициализаторы C++20. "
                               "В формах .labelAbove = true ставит подпись поля или списка на отдельную строку, и поля выравниваются; "
                               ".onChange у checkbox получает новое состояние. .type = ui::InputType::Password скрывает "
                               "набранное (точки в терминале); Email, Number, Tel и Url выбирают клавиатуру телефона.",
                 "ui::button({.label = \"Сохранить\", .onClick = save, .variant = ui::Variant::Primary})\n"
                 "ui::input({.value = name, .onChange = setNameFn, .onSubmit = addFn, .label = \"Имя\", .labelAbove = true})\n"
                 "ui::input({.value = pass, .onChange = setPassFn, .label = \"Пароль\", .type = ui::InputType::Password})\n"
                 "ui::checkbox({.label = \"Согласен\", .checked = on, .onChange = [setOn](bool v) { setOn(v); }})\n"
                 "ui::modal({.title = \"Удалить?\", .open = asking, .onClose = closeFn, .children = {...}})"},
                {"Длинные списки", "ui::useVirtualList рисует только строки на экране: 100 000 строк стоят как 40, в обоих бэкендах. "
                                   "Строки first..last рисуете сами и отдаёте в view(). follow держит конец на виду, пока "
                                   "приходят строки (логи, чаты).",
                 "auto list = ui::useVirtualList({.count = n, .follow = atEnd,\n"
                 "    .onScroll = [setAtEnd](const ScrollEvent& e) { setAtEnd(e.nearBottom(1)); }});\n"
                 "std::vector<Element> rows;\n"
                 "for (int i = list.first; i < list.last; ++i) rows.push_back(h(Line, {.n = i}));\n"
                 "return list.view(std::move(rows));"},
                {"Формы: useForm", "ui::useForm хранит значения формы в вашей структуре и проверяет их вашей функцией. "
                                   "form.set(\"name\", &T::name) — onChange для поля; form.error(\"name\") — его сообщение, "
                                   "оно видно после изменения поля или попытки отправить. submit() вызывает onSubmit, только "
                                   "когда ошибок нет. У каждого поля есть .error.",
                 "struct Signup { std::string name; bool agree = false; };\n"
                 "auto form = ui::useForm<Signup>({.validate = [](const Signup& s) {\n"
                 "                                     ui::FormErrors e;\n"
                 "                                     if (s.name.empty()) e[\"name\"] = \"Обязательно\";\n"
                 "                                     return e;\n"
                 "                                 },\n"
                 "                                 .onSubmit = [](const Signup& s) { save(s); }});\n"
                 "ui::input({.value = form.values.name, .onChange = form.set(\"name\", &Signup::name),\n"
                 "           .label = \"Имя\", .error = form.error(\"name\")})\n"
                 "ui::button({.label = \"Зарегистрироваться\", .onClick = form.submit})"},
                {"Ещё поля", "textarea — несколько строк (и в терминале), radioGroup — когда все варианты должны быть "
                             "на виду (одна остановка Tab, выбор стрелками — и в терминале), slider — число в диапазоне, progress — сколько уже сделано.",
                 "ui::textarea({.value = note, .onChange = setNoteFn, .label = \"Комментарий\", .rows = 3})\n"
                 "ui::radioGroup({.label = \"Тариф\", .value = plan, .options = {{\"free\", \"Free\"}, {\"pro\", \"Pro\"}},\n"
                 "                .onChange = setPlanFn, .direction = ui::Direction::Row})\n"
                 "ui::slider({.label = \"Места\", .value = seats, .min = 1, .max = 10, .onChange = setSeatsFn})\n"
                 "ui::progress({.value = done, .max = total, .label = \"Загрузка\"})"},
                {"Таблицы", "ui::table выравнивает колонки в браузере и в терминале. Строки ваши: сортируйте их в onSort, "
                            "заголовок покажет стрелку. Строки с onSelect откликаются на клик и Enter. Для длинных таблиц "
                            "передайте useVirtualList в .list и стройте только строки на экране.",
                 "ui::table({.columns = {{.title = \"Имя\", .sortable = true},\n"
                 "                       {.title = \"Кол-во\", .width = 6, .align = ui::Align::End, .sortable = true}},\n"
                 "           .rows = rows,  // {.cells = {name, qty}, .key = id, .onSelect = pick}\n"
                 "           .sortColumn = sortBy, .sortDescending = desc, .onSort = sortFn})"},
                {"Подтверждение и уведомления", "ui::confirm — готовое окно «Удалить?». ui::toast показывает короткое сообщение в углу "
                                                "из любого обработчика, эффекта или ответа fetch; оно исчезает само. Один ui::toaster() "
                                                "в приложении.",
                 "ui::confirm({.title = \"Удалить задачу?\", .open = asking, .danger = true,\n"
                 "             .onConfirm = removeFn, .onCancel = closeFn})\n"
                 "ui::toast({.text = \"Сохранено\", .variant = ui::Variant::Success});"},
                {"Выбор, подсказка, раскладка", "ui::select — нативный <select> в браузере и переключатель ‹ › в терминале. "
                                                "ui::tooltip показывает текст, пока указатель над детьми. ui::stack и ui::card "
                                                "раскладывают элементы без CSS.",
                 "ui::stack({.direction = ui::Direction::Row, .justify = ui::Justify::Between, .children = {\n"
                 "    ui::select({.label = \"Сортировка\", .value = sort, .options = {{\"name\", \"По имени\"}, {\"date\", \"По дате\"}},\n"
                 "                .onChange = setSortFn}),\n"
                 "    ui::tooltip({.text = \"Спросит заранее\", .children = {ui::button({.label = \"Очистить\"})}}),\n"
                 "}})"},
                {"Тема", "ui::themeProvider передаёт ui::Theme через контекст; в браузере она превращается в CSS-переменные. "
                         "У каждого имени цвета есть светлый и тёмный оттенок (prefers-color-scheme) с контрастом текста 4.5:1 "
                         "(WCAG AA); \"#rrggbb\" используется как есть. .level у ui::card задаёт уровень заголовка (по умолчанию h3).",
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
                {"Вложенные маршруты", "Маршрут, который кончается на /*, совпадает со своим путём и всем, что под ним; "
                                       "routes() внутри его элемента разбирают остаток. Внешний элемент — это разметка "
                                       "раздела: она остаётся смонтированной, пока меняются страницы, и видит параметры "
                                       "внешнего маршрута.",
                 "router::routes({{\"/teams/:team/*\", h(Team)}, {\"*\", h(NotFound)}});\n\n"
                 "Element Team(const Props&) {   // /teams/a, /teams/a/members/7\n"
                 "    return h(\"div\", {}, {\n"
                 "        router::link({.to = \"/teams/\" + router::useParam(\"team\"), .label = \"Команда\", .exact = false}),\n"
                 "        router::routes({{\"/\", h(Overview)}, {\"/members/:id\", h(Member)}}),\n"
                 "    });\n"
                 "}"},
                {"Guards и редиректы", "Маршрут с .allow = false пропускается, а с .redirect — уводит туда (заменяя запись "
                                       "в истории). router::redirect уводит сразу. .allow — обычный bool, его считают там, "
                                       "где вызывают routes(), поэтому маршруты следуют за состоянием. "
                                       "router::redirectedFrom() — путь, с которого guard увёл пользователя: страница "
                                       "входа возвращает его туда после входа.",
                 "router::routes({\n"
                 "    {\"/\", router::redirect(\"/inbox\")},\n"
                 "    {.path = \"/admin/*\", .element = h(Admin), .allow = user.isAdmin, .redirect = \"/login\"},\n"
                 "    {\"*\", h(NotFound)},\n"
                 "})\n"
                 "// в обработчике страницы входа, после входа:\n"
                 "std::string from = router::redirectedFrom();\n"
                 "router::navigate(from.empty() ? \"/\" : from, /*replace=*/true);"},
                {"Ссылки и переходы", "link — ссылка без перезагрузки (Ctrl+клик — новая вкладка), на текущем пути получает "
                                      ".activeClassName (\"active\") (ссылка с query, \"/?tag=work\", — только пока в query те же "
                                      "значения); navigate и back — из кода. useParams() — все параметры "
                                      "маршрута, currentPath() — путь с query (не хук), href(path) — URL с базовым путём.",
                 "router::link({.to = \"/todos/7\", .label = \"Открыть\"});\nrouter::navigate(\"/todos\");\nrouter::navigate(\"/login\", /*replace=*/true);\nrouter::back();\n"
                 "const auto& params = router::useParams();   // {\"id\": \"7\"}\nstd::string url = router::href(\"/todos/7\");  // \"/my-app/todos/7\""},
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
                {"Живые данные: useWebSocket", "Сообщения от сервера сразу, без опроса (reactx/websocket.hpp). Обработчики "
                                               "работают в UI-потоке; после обрыва подключается снова. В браузере относительный "
                                               "URL идёт на хост страницы (reactx dev --proxy его пропускает); терминальным "
                                               "приложениям нужен ws://host:port/path.",
                 "auto live = useWebSocket(\"/api/ws\", {\n"
                 "    .onMessage = [dispatch](const std::string& text) { dispatch(parse(text)); },\n"
                 "});\n"
                 "if (!live.open()) return h(\"p\", {}, {\"подключаемся... \" + live.error});\n"
                 "live.send(R\"({\"text\":\"привет\"})\");"},
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
                {"HttpServer", "HTTP/1.1 на POSIX-сокетах: файлы сайта, маршруты с параметрами, SPA-режим. server.onFile(fn) "
                               "меняет каждый файл (тело или заголовки) перед отправкой: заголовки кэша, вставленный скрипт.",
                 "rx::net::HttpServer server(\"127.0.0.1\", 8090);\n"
                 "server.get(\"/api/hello/:name\", [](const rx::net::Request& req) {\n"
                 "    return rx::net::Response::json(json::object({{\"hello\", req.pathParam(\"name\")}}).dump());\n"
                 "});\n"
                 "server.post(\"/api/todos\", [](const rx::net::Request& req) {  // а также put, patch, del; req.body, req.header(\"x-token\"), req.param(\"q\")\n"
                 "    return rx::net::Response::json(req.body, 201);\n"
                 "});\n"
                 "server.serveDirectory(\"build/web\", /*spaFallback=*/true);\n"
                 "server.setNotFoundPage(\"404.html\");   // unknown GET paths\n"
                 "server.setLogging(true);                // a line per request\n"
                 "server.start();\nserver.run();"},
                {"Готовые программы", "reactx_serve — раздаёт папку для разработки (с --base и --not-found ведёт себя как "
                                      "GitHub Pages). api_server — пример полного стека: веб-приложение задач и его REST API "
                                      "в одном процессе; с --data задачи сохраняются в файл.",
                 "./build/debug/reactx_serve build/web --port 8090\n"
                 "./build/debug/reactx_serve build/pages/site --base /ReactX/ --not-found 404.html\n"
                 "./build/debug/api_server build/web/app --port 8091 --data todos.json"},
                {"WebSocket", "server.webSocket(path, обработчики): обработчики работают в потоке соединения (общие данные — "
                              "под мьютексом); server.webSockets(path) — открытые соединения для рассылки.",
                 "server.webSocket(\"/api/ws\", {.onMessage = [&server](const rx::net::WebSocketPtr&, const std::string& text) {\n"
                 "    for (const auto& peer : server.webSockets(\"/api/ws\")) peer->send(text);\n"
                 "}});"},
                {"Прокси", "server.proxy пересылает запросы под префиксом на другой сервер (и WebSocket тоже); rx::net::forward отправляет один "
                           "запрос и возвращает std::expected<Response, HttpError> (502 — не дозвонились, 504 — не ответил).",
                 "server.proxy(\"/api\", *rx::net::parseUpstream(\"http://127.0.0.1:5000\"));\n\n"
                 "auto reply = rx::net::forward(req, {.host = \"127.0.0.1\", .port = 5000});\n"
                 "if (!reply) std::println(\"{}: {}\", reply.error().status, reply.error().message);"},
                {"Безопасность", "Отвечает только на localhost, *.localhost и IP-адреса (чужой сайт, направивший своё имя на "
                                 "127.0.0.1, получит 403: DNS rebinding); другие имена — allowHosts или reactx dev --allow-host. "
                                 "WebSocket и POST/PUT/PATCH/DELETE со страницы другого сайта (по Origin) получают 403. Файлы "
                                 "с точкой (.env, .git) и симлинки за пределы папки не отдаются. Запрос должен прийти за 30 с, "
                                 "не больше 512 соединений сразу.",
                 "server.allowHosts({\"mypc.local\", \".example.test\"});   // \".x\": x и поддомены, \"*\": любые\n"
                 "server.setRequestTimeout(std::chrono::seconds(10));\n"
                 "server.setMaxConnections(128);"},
                {"Важно", "Сервер рассчитан на разработку на своём компьютере: нет HTTPS и keep-alive.", ""},
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
                {"Чего не хватает",
                 "Чего пока нет в этом релизе:\n\n"
                 "- Терминальные приложения ходят только по http:// (без HTTPS); в браузере fetch — браузерный, HTTPS работает.\n"
                 "- Server-Sent Events (SSE) не обёрнуты; для живых данных — useWebSocket, или опрос через useInterval.\n"
                 "- Прокси reactx dev --proxy и HttpServer — для разработки: в продакшене API и файлы раздаёт Flask, nginx "
                 "или другой боевой сервер.\n\n"
                 "Уже есть (раньше этого не хватало): заголовки запроса (Authorization: Bearer ...), заголовки ответа "
                 "(r.header(\"etag\")), таймаут и cancel() — см. «Данные: fetch и JSON»; WebSocket — useWebSocket.",
                 ""},
            },
        },
    };
    return all;
}

}  // namespace site_docs
