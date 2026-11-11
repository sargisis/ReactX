#pragma once

// English documentation pages of the ReactX site: the same pages (slugs) as
// site_docs.hpp, written for people who meet ReactX for the first time.

#include <vector>

#include "site_docs.hpp"

namespace site_docs {

inline const std::vector<Page>& pagesEn() {
    static const std::vector<Page> all = {
        {
            "start", "Getting started", "What ReactX is, and your first app in three commands.",
            {
                {"What is ReactX?", "ReactX lets you build user interfaces the way React does — components, state and hooks — "
                                    "but in C++. You describe what the screen should look like; when the state changes, "
                                    "ReactX updates only what actually changed.\n\n"
                                    "The same component code runs in two places: in the browser (compiled to WebAssembly) "
                                    "and in the terminal (as a text UI). No JavaScript, Node or npm is involved.", ""},
                {"1. Install (one line)",
                 "On Linux (x86-64) one command installs everything. It downloads the reactx tool into ~/.local/bin, then "
                 "reactx install downloads the compiler for the browser (Emscripten) and, if your system lacks them, CMake and "
                 "Ninja into ~/.reactx — no sudo, like npm. It needs curl, git, python3, tar and xz (xz-utils on Debian/Ubuntu), which almost every Linux has.\n\n"
                 "reactx install --check shows what is there. Terminal apps also need GCC 16+ from your package manager. "
                 "Other systems: the sections below or the Docker image.",
                 "curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh"},
                {"Install by hand: Arch Linux",
                 "On Arch Linux, compiler and build tools can be installed via pacman in a single command:\n\n"
                 "- GCC 16, CMake, Ninja, and Git\n"
                 "- Emscripten for WebAssembly builds\n"
                 "- Prebuilt reactx from GitHub Releases or build from source",
                 "# Install tools via pacman:\n"
                 "sudo pacman -S gcc cmake ninja git emscripten\n\n"
                 "# Download prebuilt reactx CLI:\n"
                 "curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh\n\n"
                 "# by hand: github.com/sargisis/ReactX/releases -> reactx-X.Y.Z-linux-x86_64.tar.gz\n"
                 "tar xzf reactx-*-linux-x86_64.tar.gz && cp reactx-*/bin/* ~/.local/bin/\n"
                 "reactx --version"},
                {"Install: macOS",
                 "On macOS (Apple Silicon M1/M2/M3/M4 and Intel), tools are installed using Homebrew:\n\n"
                 "- brew install cmake ninja git\n"
                 "- GCC 16 or newer for native builds (Apple Clang can't build ReactX yet; check with g++-16 --version)\n"
                 "- Emscripten for WebAssembly web apps",
                 "# 1. Install dependencies via Homebrew:\n"
                 "brew install cmake ninja git gcc emscripten\n\n"
                 "# 2. Build the reactx CLI from source:\n"
                 "git clone https://github.com/sargisis/ReactX.git && cd ReactX\n"
                 "cmake --preset release -DCMAKE_CXX_COMPILER=g++-16 && cmake --build --preset release\n"
                 "cmake --install build/release --prefix ~/.local     # ~/.local/bin/reactx"},
                {"Install: Windows",
                 "On Windows, using WSL 2 (Windows Subsystem for Linux) is recommended for full C++26 and POSIX terminal compatibility:\n\n"
                 "- WSL 2 gives a 100% native Linux toolchain inside Windows\n"
                 "- The browser on Windows opens http://127.0.0.1:8090/ seamlessly\n"
                 "- Seamless integration with VS Code (WSL extension or Dev Containers)",
                 "# 1. In an elevated PowerShell, install WSL:\n"
                 "wsl --install\n\n"
                 "# 2. Inside the WSL terminal (Ubuntu/Arch), install tools:\n"
                 "sudo apt update && sudo apt install -y cmake ninja-build gcc-16 git\n\n"
                 "# 3. Emscripten for web projects:\n"
                 "git clone https://github.com/emscripten-core/emsdk.git ~/emsdk\n"
                 "~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest && source ~/emsdk/emsdk_env.sh\n\n"
                 "# 4. Download reactx from Releases or build from source"},
                {"Install: Ubuntu / Debian & Docker",
                 "For Linux x86-64, download the prebuilt archive from GitHub Releases or run inside the official Docker container:",
                 "# Option A: Prebuilt binary from GitHub Releases\n"
                 "sudo apt update && sudo apt install -y cmake ninja-build git\n"
                 "tar xzf reactx-*-linux-x86_64.tar.gz && cp reactx-*/bin/* ~/.local/bin/\n\n"
                 "# Option B: Run in Docker (includes GCC 16, Emscripten, and reactx):\n"
                 "docker run --rm -it -u \"$(id -u):$(id -g)\" -v \"$PWD:/app\" -p 8090:8090 ghcr.io/sargisis/reactx\n"
                 "reactx new my-app && cd my-app && reactx dev --host 0.0.0.0"},
                {"2. Create an app", "This makes a folder with a working example: a small web app with a counter and two pages. "
                                     "Add --terminal for a terminal app instead.",
                 "reactx new my-app\ncd my-app"},
                {"3. Run it", "reactx dev builds the app, opens a local server and rebuilds the page every time you save a file. "
                              "Open http://127.0.0.1:8090/ and edit src/app.cpp.",
                 "reactx dev"},
                {"Your first component", "A component is a function: it gets its props (a struct) and returns elements. "
                                         "useState keeps a value between renders; changing it redraws the component.",
                 "#include \"reactx/dom.hpp\"\n#include \"reactx/reactx.hpp\"\nusing namespace rx;\n\n"
                 "struct CounterProps {\n    int start = 0;\n};\n\n"
                 "Element Counter(const CounterProps& p) {\n"
                 "    auto [count, setCount] = useState(p.start);\n"
                 "    return h(\"button\", {{\"onClick\", [setCount] { setCount([](int c) { return c + 1; }); }}},\n"
                 "             {\"Clicked \" + std::to_string(count) + \" times\"});\n"
                 "}\n\n"
                 "int main() { mountDom(h(Counter, {.start = 0})); }"},
                {"Writing it with an AI assistant", "AI models don't know ReactX yet: it is new. Every project made by reactx new "
                                                    "has an AGENTS.md that tools like Codex, Cursor and Claude Code read on their own. "
                                                    "In a plain chat, start with this sentence:",
                 "We're using ReactX, a C++ UI framework (not JavaScript): https://github.com/sargisis/ReactX.\n"
                 "Read https://sargisis.github.io/ReactX/llms.txt first — it has the whole API.\n"
                 "Don't invent anything that isn't there."},
                {"Building ReactX itself", "Only if you want to change the framework: it needs GCC 16, CMake 3.28+ and Ninja.",
                 "cmake --preset debug -DCMAKE_CXX_COMPILER=g++-16 && cmake --build --preset debug\n"
                 "ctest --preset debug -LE slow"},
            },
        },
        {
            "cli", "The reactx tool", "Create a project, run it with live reload, build it, add libraries.",
            {
                {"Install", "One line on Linux x86-64: the prebuilt reactx, then reactx install, which downloads Emscripten (and "
                            "CMake and Ninja if needed) into ~/.reactx; reactx build and dev find them on their own. "
                            "By hand: grab the prebuilt reactx for Linux x86-64 from GitHub Releases, unpack it and put bin/ on your PATH. "
                            "Browser apps need Emscripten, CMake and Ninja; terminal apps also need GCC 16. "
                            "You can build it from source too.",
                 "# github.com/sargisis/ReactX/releases -> reactx-X.Y.Z-linux-x86_64.tar.gz\n"
                 "tar xzf reactx-*-linux-x86_64.tar.gz && cp reactx-*/bin/* ~/.local/bin/\n\n"
                 "# or from source\n"
                 "cmake --preset release && cmake --build --preset release\n"
                 "cmake --install build/release --prefix ~/.local     # ~/.local/bin/reactx"},
                {"Help", "Every command explains itself.", "reactx help\nreactx add --help"},
                {"Docker and devcontainers", "The image ghcr.io/sargisis/reactx has GCC 16, Emscripten and reactx — nothing to "
                                             "install. Inside a container, run reactx dev --host 0.0.0.0 so the page opens from outside. "
                                             "Every new project has a .devcontainer/, so VS Code (\"Reopen in Container\") and "
                                             "GitHub Codespaces open it with all the tools ready.",
                 "docker run --rm -it -u \"$(id -u):$(id -g)\" -v \"$PWD:/app\" -p 8090:8090 ghcr.io/sargisis/reactx\n"
                 "reactx new my-app && cd my-app && reactx dev --host 0.0.0.0"},
                {"Your editor", "Use the clangd extension: it understands C++26 like the compiler does. The Microsoft C/C++ "
                                "extension doesn't, and underlines correct code. New projects come with .clangd and .vscode/ "
                                "set up; run reactx build once so clangd knows the flags.",
                 "code --install-extension llvm-vs-code-extensions.vscode-clangd\nreactx build"},
                {"reactx new", "Creates a folder with reactx.json (the project file, like package.json), CMakeLists.txt, "
                               "src/ (app.cpp, main.cpp), tests/, editor settings and AGENTS.md. ReactX is pinned to a release tag, so new "
                               "ReactX versions never break your project by surprise. --terminal makes a terminal app.",
                 "reactx new my-app\ncd my-app"},
                {"reactx dev", "Builds the web app, serves it and watches src/, public/ and reactx.json. After every successful "
                               "build the page reloads by itself; if the code doesn't compile, the old version stays and the "
                               "error is in the terminal.",
                 "reactx dev                  # http://127.0.0.1:8090/\nreactx dev --port 3000\n"
                 "reactx dev --host 0.0.0.0   # reachable from other devices and containers"},
                {"Your own backend: --proxy", "If your API is in Flask, Django or Express, reactx dev forwards requests under a "
                                              "prefix (/api by default) to it and returns the answer as is — like Vite's proxy. "
                                              "The page and the API share one address, so no CORS setup is needed.",
                 "reactx dev --proxy http://127.0.0.1:5000             # /api -> Flask\n"
                 "reactx dev --proxy /graphql=http://127.0.0.1:4000    # your own prefix\n\n"
                 "# or in reactx.json:\n"
                 "\"web\": { \"base\": \"/\", \"proxy\": { \"/api\": \"http://127.0.0.1:5000\" } }"},
                {"Browser and terminal from one project", "reactx build --native builds the same components for the terminal. "
                                                          "mountDom exists only in the browser, so main() picks the entry point:",
                 "int main() {\n#ifdef __EMSCRIPTEN__\n    mountDom(h(App));\n#else\n    TerminalApp app;   // reactx/terminal.hpp\n"
                 "    app.run(h(App));\n#endif\n}"},
                {"reactx test", "Builds every tests/*.cpp natively (with the app's sources except main.cpp) and runs them. "
                                "A test draws the app on an in-memory terminal and acts like a user: click a label, type into "
                                "the focused field, press keys, then check what the screen shows. reactx new puts the "
                                "components in src/app.cpp and an example test in tests/app_tests.cpp. reactx test <name> "
                                "runs the tests whose name contains it.",
                 "#include \"app.hpp\"\n"
                 "#include \"reactx/testing.hpp\"\n\n"
                 "RX_TEST(the_button_counts_clicks) {\n"
                 "    rx::testing::Screen s(rx::h(App));\n"
                 "    CHECK(s.click(\"Clicked 0 times\"));\n"
                 "    s.type(\"Ann\");          // into the focused field\n"
                 "    s.press(\"Ctrl+s\");      // \"Enter\", \"Tab\", \"Up\", ...\n"
                 "    CHECK(s.shows(\"Clicked 1 times\"));\n"
                 "}\n\n"
                 "// reactx test           -> OK, 2 tests passed"},
                {"reactx build", "Builds for publishing: build/web/dist holds plain files for any static host "
                                 "(GitHub Pages, Netlify, nginx). A terminal app goes to build/native/<name>. A web build also prints the "
                                 "size of the .wasm and its biggest parts, and names code that pulls in std::format (~100 KB). "
                                 "set(REACTX_WEB_LTO ON) above FetchContent in CMakeLists.txt turns on link-time optimization: "
                                 "a ~5% smaller .wasm, but every rebuild in reactx dev takes a couple of seconds longer.",
                 "reactx build\n# index.wasm: 122 KB (the page shows nothing until it is downloaded)\n"
                 "#   biggest parts: src/app.cpp 23%, ReactX reconciler.cpp 15%, the C++ library 12%, ..."},
                {"Libraries", "reactx add records a git repository in reactx.json; CMake fetches it on the next build, "
                              "and --target links it to your app. (v1.2.0+) Libraries that come with ReactX, in library/, need no "
                              "download: link them in CMakeLists.txt. reactx::charts draws bar, line and pie charts (SVG in the "
                              "browser, characters in a terminal); reactx::i18n gives translations, plurals, numbers, money and "
                              "dates in 10 languages; (v1.3.0+) reactx::query loads server data with a shared cache, "
                              "like React Query, and reactx::markdown shows Markdown (HTML in the browser, colors in a terminal); "
                              "(v1.4.0+) reactx::icons draws 51 line icons; (v1.5.0+) reactx::forms validates forms and JSON, "
                              "and reactx::api builds a JSON API on HttpServer, reactx::auth adds sign-in (passwords, sessions, CSRF), "
                              "reactx::log and reactx::config give logs and settings, reactx::calendar, reactx::dnd and "
                              "reactx::motion a date picker, drag and drop and animations; (v1.7.0+) reactx::grid a data table with sorting, "
                              "search, filters and pages, reactx::realtime live messages in rooms over a WebSocket, reactx::db an embedded "
                              "store of JSON documents and reactx::jobs background jobs with retries and cron schedules. They are a preview: the API may still change in a minor release.",
                 "# CMakeLists.txt\ntarget_link_libraries(my_app PRIVATE reactx::charts reactx::i18n)\n\n"
                 "reactx add fmt https://github.com/fmtlib/fmt.git --rev 11.0.2 --target fmt::fmt\nreactx remove fmt"},
                {"reactx update", "Moves your project to the newest ReactX release. --check only tells you whether there is one.",
                 "reactx update --check   # ReactX v1.7.0 -> v1.7.1 is available\n"
                 "reactx update\n"
                 "reactx build"},
                {"reactx.json", "Everything about the project in one file.",
                 "{\n"
                 "  \"name\": \"my-app\",\n"
                 "  \"template\": \"web\",\n"
                 "  \"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v1.7.1\" },\n"
                 "  \"dependencies\": { \"fmt\": { \"git\": \"...\", \"rev\": \"11.0.2\", \"target\": \"fmt::fmt\" } },\n"
                 "  \"web\": { \"base\": \"/\", \"proxy\": { \"/api\": \"http://127.0.0.1:5000\" } }\n"
                 "}"},
            },
        },
        {
            "versions", "Versions", "How ReactX versions work and how to upgrade.",
            {
                {"Version numbers", "ReactX uses Semantic Versioning: MAJOR.MINOR.PATCH. PATCH fixes bugs, MINOR adds features, "
                                    "MAJOR may change the API — the CHANGELOG says what to adjust. Before 1.0 a MINOR "
                                    "release could change the API too.", ""},
                {"API stability", "The public API is what include/reactx/ declares outside detail namespaces, the reactx "
                                  "commands and options, reactx.json and the CMake functions and targets. Since 1.0 it "
                                  "changes only in MAJOR releases. Nothing is renamed in place: a better name is added, the "
                                  "old one is marked deprecated (code comment, docs, the CHANGELOG's Deprecated section) and "
                                  "keeps working until 2.0. Deprecated so far: ui::ListProps::items (use .children) and "
                                  "ui::RadioGroupProps::row (use .direction = ui::Direction::Row). C++26 reflection stays "
                                  "opt-in (reactx::reflection, native builds), outside this promise for now.", ""},
                {"Upgrading reactx", "Two steps: the tool, then each project. reactx upgrade replaces the reactx tool with the newest "
                                     "release (checking its SHA-256); before v0.14.0 run the installer again. Once a day reactx also "
                                     "checks for a new release in the background and tells you (REACTX_NO_UPDATE_CHECK=1 turns it off).",
                 "reactx upgrade      # v0.14.0 and newer\n"
                 "# older: curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh\n"
                 "cd my-app && reactx update && reactx build"},
                {"Upgrading a project", "reactx update finds the newest release and writes it into reactx.json; the next build downloads it. "
                              "By hand: change rev to the new tag.",
                 "reactx update\n# or in reactx.json:\n\"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v1.7.1\" }"},
                {"The version in code", "",
                 "#include \"reactx/version.hpp\"\n\nstd::println(\"{}\", rx::version());   // \"1.7.1\"\n"
                 "static_assert(REACTX_VERSION_MAJOR == 1);"},
            },
        },
        {
            "safety", "Modern C++ and safety", "Modern C++, errors as values, checks that catch bugs early.",
            {
                {"Modern C++", "ReactX uses what the C++ standard library already has instead of reinventing it: "
                               "std::expected, std::flat_map, std::format, std::print and more. Terminal builds also use "
                               "C++26 features of GCC 16.", ""},
                {"Errors are values", "Where failure is normal — user input, the network, files — functions return "
                                      "std::expected instead of throwing. You see the error and decide what to do.",
                 "if (auto v = json::tryParse(body)) use(*v);\nelse std::println(\"bad JSON: {}\", v.error().what());\n\n"
                 "auto port = rx::parsePort(arg);   // \"80x\" is an error, not 80"},
                {"Mistakes stop early", "Bounds checks are on by default: reading past the end of a vector stops the program "
                                        "instead of corrupting memory. REACTX_PRE checks preconditions with C++26 contracts.",
                 "#include \"reactx/contract.hpp\"\n\nvoid resize(int w) {\n    REACTX_PRE(w > 0);\n    ...\n}"},
                {"JSON from your structs", "With C++26 reflection (GCC 16, native builds) a plain struct turns into JSON and back; "
                                           "a wrong field gets an error that says where.",
                 "struct Todo { int id; std::string title; bool done; };\n\n"
                 "rx::json::Value v = rx::json::to(Todo{1, \"Read\", false});\n"
                 "auto todo = rx::json::from<Todo>(v);   // std::expected<Todo, json::Error>\n\n"
                 "target_link_libraries(app PRIVATE reactx::reflection)   # CMake"},
                {"How ReactX is tested", "Every change runs all tests under AddressSanitizer, UndefinedBehaviorSanitizer and "
                                         "ThreadSanitizer, fuzzes the parsers of outside data (JSON, HTTP) and runs a static "
                                         "analyzer. Found a security problem? See SECURITY.md in the repository.", ""},
            },
        },
        {
            "elements", "Elements and props", "h(), children, keys and fragments.",
            {
                {"h()", "h(tag, props, children) describes a node; h(Component, props, children) uses a component. "
                        "A string among the children is text.",
                 "h(\"div\", {{\"className\", \"card\"}}, {\n"
                 "    h(\"h1\", {}, {\"Title\"}),\n"
                 "    \"plain text\",\n"
                 "    h(Counter, {.start = 5}),\n"
                 "})"},
                {"Props are a struct", "A component's props are a plain struct. A typo in a field name or a wrong type is a "
                                       "compile error, not a bug in production. Give every field a default ({}), and you only "
                                       "write the ones you need.",
                 "struct HelloProps {\n    std::string name = \"world\";\n};\n\n"
                 "Element Hello(const HelloProps& p) {\n"
                 "    return h(\"p\", {}, {\"Hello, \" + p.name});\n"
                 "}\n\n"
                 "h(Hello, {.name = \"ReactX\"})\n"
                 "h(Hello)                       // \"Hello, world\""},
                {"Children", "A children field (std::vector<Element>) gets the elements passed as the third argument.",
                 "struct CardProps {\n    std::string title{};\n    std::vector<Element> children{};\n};\n\n"
                 "Element Card(const CardProps& p) {\n"
                 "    return h(\"div\", {{\"className\", \"card\"}}, p.children);\n}\n\n"
                 "h(Card, {.title = \"Hi\"}, {h(\"p\", {}, {\"text\"})})"},
                {"A lambda as a component", "A lambda without captures works like a function. Pass data through props.",
                 "const auto Badge = [](const BadgeProps& p) -> Element {\n"
                 "    return h(\"span\", {{\"className\", \"badge\"}}, {p.label});\n};\n\n"
                 "h(Badge, {.label = \"new\"})"},
                {"Props by name", "The older way works too: Element(const Props&), props.get<T>(name, fallback), "
                                  "props.children().",
                 "Element Hello(const Props& p) {\n"
                 "    return h(\"p\", {}, {\"Hello, \" + p.get<std::string>(\"name\", \"world\")});\n"
                 "}\n\nh(Hello, {{\"name\", \"ReactX\"}})"},
                {"Lists and keys", "Give list items a key so ReactX recognizes them when the list changes: moved items are "
                                   "moved, not recreated, and keep their state. A key field in the props struct is the key. "
                                   "fragment() groups children without an extra node.",
                 "for (const auto& t : todos) items.push_back(h(TodoItem, {.key = t.id, .title = t.title}));\n"
                 "return fragment(std::move(items));"},
            },
        },
        {
            "hooks", "Hooks", "State, effects, timers, keyboard shortcuts, shared stores.",
            {
                {"useState", "A value that survives re-renders. The setter takes a new value or a function of the old one; "
                             "setting the same value does nothing. Any type works, a std::vector of your own structs too: "
                             "with == (its own, or of all its parts) an equal value is skipped, without it every set re-renders. "
                             "A costly first value goes in a lambda: it runs once, on the first render (since v1.6.0).",
                 "auto [name, setName] = useState(std::string{});\nsetName(\"Ann\");\nsetCount([](int c) { return c + 1; });\n"
                 "auto [rows, setRows] = useState([] { return loadRows(); });  // built once"},
                {"useEffect", "Runs after the screen updates. deps(...) says when to run it again; deps() means once. "
                              "Return a function to clean up.",
                 "useEffect([id] {\n    subscribe(id);\n    return [id] { unsubscribe(id); };\n}, deps(id));"},
                {"useRef", "A value that survives re-renders without causing them.", "int& renders = useRef(0);\n++renders;"},
                {"useMemo and useCallback", "useMemo recomputes only when its deps change. useCallback keeps the same handler "
                                            "between renders, so memo() components can skip work. Any signature works: a row's onOpen(id) too.",
                 "const auto& sorted = useMemo([&] { return sortItems(items); }, deps(items));\n"
                 "Callback onSave = useCallback([setSaved] { setSaved(true); }, deps());\n"
                 "std::function<void(int)> onOpen = useCallback([setOpen](int id) { setOpen(id); }, deps());"},
                {"useReducer", "State changed by actions: a function (state, action) -> new state. Handy for many fields or undo.",
                 "enum class Op { Add, Undo };\n"
                 "auto [doc, dispatch] = useReducer([](const Doc& d, Op op) { /* ... */ return next; }, Doc{});\n"
                 "dispatch(Op::Undo);"},
                {"createStore and useStore", "State outside the component tree, shared by any component — a cart, the signed-in "
                                             "user, settings (like Zustand). With a selector, a component re-renders only when "
                                             "its part changes.",
                 "const auto Cart = createStore<std::vector<Item>>({});\n\n"
                 "Cart.update([](auto items) { items.push_back(milk); return items; });\n"
                 "const auto& items = useStore(Cart);\n"
                 "auto count = useStore(Cart, [](const auto& items) { return items.size(); });"},
                {"useTimeout and useInterval", "Run something once after a delay, or repeatedly. std::nullopt turns it off; "
                                               "it stops by itself when the component goes away.",
                 "useTimeout([setToast] { setToast(\"\"); }, toast.empty() ? std::nullopt : std::optional(5s));\n"
                 "useInterval([reload] { reload(); }, 30s);"},
                {"useKeyboard", "Shortcuts for the whole app, wherever the focus is. Return true to take the key. Keys that "
                                "type text don't reach it while the user types in a field.",
                 "useKeyboard([undo](const KeyEvent& k) {\n"
                 "    if (k.ctrl && k.key == \"z\") { undo(); return true; }\n"
                 "    return false;\n"
                 "});"},
                {"useTitle", "The browser tab's title while the component is mounted; the previous title comes back when it unmounts, "
                             "so each route page can name itself; prerendered pages get it in their <title>. Natively it only keeps the text.",
                 "useTitle(\"Orders (\" + std::to_string(count) + \")\");"},
                {"useDebounce, useLocalStorage, useWindowSize, useId",
                 "useDebounce gives a value only after it stopped changing (search boxes). useLocalStorage remembers state across "
                 "page reloads (natively it lives in memory only, until the program exits); a third argument hears about failed saves "
                 "(quota, private mode), and rx::storageRead / storageWrite give errors as std::expected. useWindowSize re-renders on resize. useId gives an id for <label for>.",
                 "std::string debounced = useDebounce(query, 300ms);\n"
                 "auto [theme, setTheme] = useLocalStorage(\"theme\", std::string(\"dark\"));\n"
                 "WindowSize size = useWindowSize();\n"
                 "std::string id = useId();"},
                {"errorBoundary", "If a component throws while rendering, show a fallback instead of crashing the whole app. "
                                  "retry() tries again; until then it shows the fallback, also when its route's parameters change, so on a route page "
                                  "give it the path as its key: errorBoundary(fallback, {h(Page)}, router::useLocation()). "
                                  "(Exceptions in event handlers and effects aren't caught, as in React.)",
                 "errorBoundary([](const RenderError& e) {\n"
                 "    return h(\"div\", {}, {\"Error: \" + e.message,\n"
                 "                           h(\"button\", {{\"onClick\", e.retry}}, {\"Try again\"})});\n"
                 "}, {h(Chart, {.rows = rows})})"},
                {"The rule of hooks", "As in React: call hooks at the top of a component, in the same order every render — "
                                      "not inside if, or loops whose length changes.", ""},
            },
        },
        {
            "context", "Context and memo", "Pass data down without props; skip needless re-renders.",
            {
                {"Context", "createContext makes a context, provide passes a value down, useContext reads the nearest one. "
                            "When the value changes, every component that reads it re-renders.",
                 "const auto Theme = createContext<std::string>(\"light\");\n\n"
                 "Theme.provide(\"dark\", {h(Toolbar)});          // in a parent\n"
                 "const std::string& theme = useContext(Theme);   // in any child"},
                {"memo", "h(memo<Item>, props) skips re-rendering when the props are equal.",
                 "h(memo<TodoItem>, {.title = t.title, .onToggle = onToggle})"},
            },
        },
        {
            "events", "Events", "Clicks, keys, typing, focus, scrolling.",
            {
                {"Handlers", "Pass a function as a prop. onClick gets nothing, onInput and onChange get the text, onKey gets a "
                             "KeyEvent, onScroll gets a ScrollEvent.",
                 "h(\"input\", {{\"value\", text},\n"
                 "             {\"onInput\", [setText](const std::string& t) { setText(t); }},\n"
                 "             {\"onKey\", [](const KeyEvent& e) { if (e.key == \"Enter\") submit(); }}})"},
                {"Keys", "KeyEvent.key is a character (\"a\") or a name: Enter, Escape, Tab, Backspace, Delete, Up, Down, Left, "
                         "Right, Home, End. Plus ctrl, alt and shift.", ""},
                {"Bubbling", "As in React, the handler of the element runs first, then those of its parents.", ""},
                {"Focus and scrolling", "onFocus and onBlur fire when an element gets or loses focus. onScroll tells you the "
                                        "position (pixels in the browser, rows on a terminal scroll box); nearBottom() says it's "
                                        "time to load more. ref.scrollIntoView() shows an element ({\"scrollIntoView\", selected} without a ref); "
                                        "ref.scrollMetrics(), scrollTo(top) and scrollBy(delta) read and move a scroll container, "
                                        "e.g. to keep the reader's place when older messages come in above.",
                 "h(\"div\", {{\"style\", \"height: 400px; overflow: auto\"},\n"
                 "            {\"onScroll\", [loadMore](const ScrollEvent& e) { if (e.nearBottom()) loadMore(); }}}, rows)"},
            },
        },
        {
            "terminal", "Terminal apps", "TerminalApp, tags and Flexbox layout.",
            {
                {"Run", "TerminalApp draws your components in the terminal and handles keys until Ctrl+C.",
                 "TerminalApp app;\napp.run(h(App));"},
                {"Tags", "box is a flex container, text is text that wraps, input is a text field, textarea a field of "
                         "several lines (rows), button is a button. Tab and the arrows move focus, Enter or Space press.", ""},
                {"Text fields", "input and textarea edit where the cursor is: Left / Right, Home / End, Ctrl+Left / Right by "
                                "words, Backspace and Delete. A textarea also takes Enter for a new line and Up / Down between "
                                "lines, wraps long lines and scrolls to the cursor. Emoji and Chinese, Japanese or Korean "
                                "characters take two columns, so borders and table columns stay in line; 👍🏽 or a flag is "
                                "one character for the cursor and Backspace.",
                 "ui::textarea({.value = note, .onChange = setNoteFn, .label = \"Note\", .rows = 6})\n"
                 "// or the tag itself:\n"
                 "h(\"textarea\", {{\"value\", note}, {\"rows\", 6}, {\"onInput\", setNoteFn}})"},
                {"Layout", "Flexbox, the same engine as React Native (Yoga): direction, justifyContent, alignItems, flexGrow, "
                           "width and height in cells or percent, padding, margin, gap, borders.",
                 "h(\"box\", {{\"flexGrow\", 1}, {\"justifyContent\", \"center\"}, {\"alignItems\", \"center\"}}, {\n"
                 "    h(\"box\", {{\"border\", true}, {\"title\", \"Window\"}, {\"padding\", 1}}, {h(\"text\", {}, {\"Hello\"})}),\n"
                 "})"},
                {"Scrolling", "What is taller than the terminal is cut off, so long lists go in a box with overflow: \"scroll\". "
                              "In a column it is as tall as its rows, or takes the rows that are left when they don't fit; the mouse wheel and PageUp/PageDown scroll it, the "
                              "focused element stays visible, a border shows the position and onScroll reports it in rows. "
                              "ui::list({.children = items, .scroll = true}) scrolls inside its border. Modals and toasts use "
                              "position: \"fixed\", so they show on the screen however tall the page is.",
                 "h(\"box\", {{\"flexGrow\", 1}}, {\n"
                 "    h(\"text\", {}, {\"Logs\"}),\n"
                 "    h(\"box\", {{\"overflow\", \"scroll\"}, {\"border\", true}}, std::move(lines)),\n"
                 "})"},
            },
        },
        {
            "browser", "Browser apps", "WebAssembly, the DOM and CSS.",
            {
                {"mountDom", "Puts your app into the page's <main id=\"app\"> (<div id=\"app\"> in older shells), replacing what it showed (a loading text, prerendered HTML). "
                             "Mounting again into the same container unmounts the old app first (its timers and sockets stop).",
                 "int main() { mountDom(h(App)); }"},
                {"Prerender", "List routes in reactx.json and the page shows before the .wasm arrives: reactx build runs the same "
                              "program under Node, where mountDom writes each route's HTML (renderToString) into dist/index.html, "
                              "dist/docs/faq.html, ... and stops. No code changes; effects don't run there. In the browser mountDom "
                              "replaces it with the live app (no hydration). Other routes get 404.html / 200.html, the empty page.",
                 "\"web\": { \"base\": \"/\", \"prerender\": [\"/\", \"/docs/faq\"] }"},
                {"Mouse, touch and drag", "onPointerDown, onPointerMove and onPointerUp get a PointerEvent: x and y in the window, "
                                          "localX and localY from the element's corner, button, buttons and type (mouse, touch, pen). "
                                          "onMouseEnter and onMouseLeave follow the pointer and don't bubble. The same handlers work in a "
                                          "terminal (in cells). {\"pointerCapture\", true} keeps moves and the release on the element after "
                                          "a press, even outside it (sliders); without it the element under the pointer gets them (drop "
                                          "targets). ref.focus() focuses an element from code. The kanban demo is one source for both.",
                 "h(\"div\", {{\"pointerCapture\", true},\n"
                 "          {\"onPointerDown\", [setGrab](const PointerEvent& e) { setGrab(e.localX); }},\n"
                 "          {\"onPointerMove\", [grab, setPos](const PointerEvent& e) {\n"
                 "              if (e.buttons) setPos(e.x - grab);\n"
                 "          }}})"},
                {"Refs and JavaScript", "useNodeRef() gives the real element: pass it as the ref prop and use it in useEffect "
                                        "(it is set before effects run, and back to 0 when the element goes). rx::domElement "
                                        "(reactx/js.hpp) turns it into an emscripten::val, the page's own object: call its methods, "
                                        "read sizes, or hand it to a JavaScript library. rx::jsValue makes a JS object from JSON. "
                                        "Link the app with reactx::js (it adds embind; other apps don't carry it). The Chart.js demo "
                                        "shows the whole thing.",
                 "auto canvas = useNodeRef();\n"
                 "auto& chart = useRef(emscripten::val::undefined());\n"
                 "useEffect([canvas, &chart] {\n"
                 "    chart = emscripten::val::global(\"Chart\").new_(domElement(canvas), jsValue(config));\n"
                 "    return [&chart] { chart.call<void>(\"destroy\"); };\n"
                 "}, deps());\n"
                 "return h(\"canvas\", {{\"ref\", canvas}});"},
                {"HTML props", "Any HTML tag works. className becomes class, style is a CSS string, value and checked keep "
                               "inputs in sync with your state, everything else becomes an attribute. A checkbox's onChange gets \"true\" or "
                               "\"false\"; rx::boolHandler turns that into a bool.",
                 "h(\"input\", {{\"type\", \"checkbox\"}, {\"checked\", done},\n"
                 "              {\"onChange\", boolHandler([setDone](bool on) { setDone(on); })}})"},
                {"Events", "onClick, onInput(text), onChange, onKey(KeyEvent), onSubmit (a form doesn't reload the page), "
                           "onFocus, onBlur, onScroll(ScrollEvent).", ""},
                {"CSS", "Put your styles in public/style.css of the project; it is copied next to the page.", ""},
                {"SVG", "SVG tags (svg, rect, circle, path, g, ...) just work. For text and links inside SVG write svg:text "
                        "and svg:a.",
                 "h(\"svg\", {{\"viewBox\", \"0 0 100 40\"}}, {\n"
                 "    h(\"rect\", {{\"width\", \"60\"}, {\"height\", \"40\"}, {\"fill\", \"orange\"}}),\n"
                 "    h(\"svg:text\", {{\"x\", \"64\"}, {\"y\", \"24\"}}, {\"60%\"}),\n"
                 "})"},
                {"Safety", "Text is always text, never HTML. Links with javascript: URLs are blocked.", ""},
            },
        },
        {
            "ui", "Ready components (rx::ui)", "Buttons, inputs, selects, dialogs, toasts, tooltips and lists with a theme.",
            {
                {"Components", "button, input, checkbox, select, tabs, modal, confirm, list and listItem, tooltip, card, stack "
                               "and text. The same code works in the terminal and in the browser. In forms, .labelAbove = true "
                               "puts the label of an input or select on its own line, so the fields line up; a checkbox's "
                               ".onChange gets the new state. .type = ui::InputType::Password hides what is typed (dots "
                               "in a terminal); Email, Number, Tel and Url pick the phone's keyboard.",
                 "ui::button({.label = \"Save\", .onClick = save, .variant = ui::Variant::Primary})\n"
                 "ui::input({.value = name, .onChange = setNameFn, .onSubmit = addFn, .label = \"Name\", .labelAbove = true})\n"
                 "ui::input({.value = pass, .onChange = setPassFn, .label = \"Password\", .type = ui::InputType::Password})\n"
                 "ui::checkbox({.label = \"Agree\", .checked = on, .onChange = [setOn](bool v) { setOn(v); }})\n"
                 "ui::modal({.title = \"Delete?\", .open = asking, .onClose = closeFn, .children = {...}})"},
                {"Long lists", "ui::useVirtualList draws only the rows on screen: 100,000 rows cost what 40 do, in both backends. "
                               "Draw rows first..last yourself and pass them to view(). follow keeps the end in view while "
                               "rows are added (logs, chats).",
                 "auto list = ui::useVirtualList({.count = n, .follow = atEnd,\n"
                 "    .onScroll = [setAtEnd](const ScrollEvent& e) { setAtEnd(e.nearBottom(1)); }});\n"
                 "std::vector<Element> rows;\n"
                 "for (int i = list.first; i < list.last; ++i) rows.push_back(h(Line, {.n = i}));\n"
                 "return list.view(std::move(rows));"},
                {"Forms: useForm", "ui::useForm keeps a form's values in your own struct and checks them with your function. "
                                   "form.set(\"name\", &T::name) is an onChange for a field; form.error(\"name\") is its message, "
                                   "shown once the field changed or after a submit was tried. submit() calls onSubmit only "
                                   "when there are no errors. Every field takes .error.",
                 "struct Signup { std::string name; bool agree = false; };\n"
                 "auto form = ui::useForm<Signup>({.validate = [](const Signup& s) {\n"
                 "                                     ui::FormErrors e;\n"
                 "                                     if (s.name.empty()) e[\"name\"] = \"Required\";\n"
                 "                                     return e;\n"
                 "                                 },\n"
                 "                                 .onSubmit = [](const Signup& s) { save(s); }});\n"
                 "ui::input({.value = form.values.name, .onChange = form.set(\"name\", &Signup::name),\n"
                 "           .label = \"Name\", .error = form.error(\"name\")})\n"
                 "ui::button({.label = \"Sign up\", .onClick = form.submit})"},
                {"More fields", "textarea for several lines (in a terminal too), radioGroup when every option "
                                "should be visible (one Tab stop; the arrows pick, in a terminal too), slider for a number "
                                "in a range, progress for how far something got.",
                 "ui::textarea({.value = note, .onChange = setNoteFn, .label = \"Note\", .rows = 3})\n"
                 "ui::radioGroup({.label = \"Plan\", .value = plan, .options = {{\"free\", \"Free\"}, {\"pro\", \"Pro\"}},\n"
                 "                .onChange = setPlanFn, .direction = ui::Direction::Row})\n"
                 "ui::slider({.label = \"Seats\", .value = seats, .min = 1, .max = 10, .onChange = setSeatsFn})\n"
                 "ui::progress({.value = done, .max = total, .label = \"Uploading\"})"},
                {"Tables", "ui::table lines up columns in the browser and in a terminal. The rows are yours: sort them in onSort, "
                           "the header shows the arrow. Rows with onSelect react to a click and Enter. For long tables pass a "
                           "useVirtualList as .list and build only the rows on screen.",
                 "ui::table({.columns = {{.title = \"Name\", .sortable = true},\n"
                 "                       {.title = \"Qty\", .width = 6, .align = ui::Align::End, .sortable = true}},\n"
                 "           .rows = rows,  // {.cells = {name, qty}, .key = id, .onSelect = pick}\n"
                 "           .sortColumn = sortBy, .sortDescending = desc, .onSort = sortFn})"},
                {"Confirm and toasts", "ui::confirm is a ready \"Delete it?\" dialog. ui::toast shows a short message in a corner "
                                       "from any handler, effect or fetch callback; it goes away by itself. Put one ui::toaster() "
                                       "in the app.",
                 "ui::confirm({.title = \"Delete the task?\", .open = asking, .danger = true,\n"
                 "             .onConfirm = removeFn, .onCancel = closeFn})\n"
                 "ui::toast({.text = \"Saved\", .variant = ui::Variant::Success});"},
                {"Select, tooltip, layout", "ui::select is a native <select> in the browser and a ‹ › switch in the terminal. "
                                            "ui::tooltip shows its text while the pointer is over the children. ui::stack and "
                                            "ui::card lay things out without CSS.",
                 "ui::stack({.direction = ui::Direction::Row, .justify = ui::Justify::Between, .children = {\n"
                 "    ui::select({.label = \"Sort\", .value = sort, .options = {{\"name\", \"By name\"}, {\"date\", \"By date\"}},\n"
                 "                .onChange = setSortFn}),\n"
                 "    ui::tooltip({.text = \"Asks first\", .children = {ui::button({.label = \"Clear\"})}}),\n"
                 "}})"},
                {"Theme", "ui::themeProvider passes colors to every ui component below it. In the browser each color name "
                          "has a light and a dark shade (prefers-color-scheme) with text at 4.5:1 contrast (WCAG AA); "
                          "\"#rrggbb\" is used as it is. ui::card's .level sets its title's heading level (h3 by default).",
                 "ui::themeProvider(ui::Theme{.accent = \"magenta\"}, {h(App)})"},
            },
        },
        {
            "router", "Router", "Several pages, links, parameters, a base path.",
            {
                {"Routes", "The first route that matches is shown. :name is a parameter, * matches anything.",
                 "router::routes({\n"
                 "    {\"/\",          h(Home)},\n"
                 "    {\"/todos/:id\", h(TodoPage)},   // router::useParam(\"id\")\n"
                 "    {\"*\",          h(NotFound)},\n"
                 "})"},
                {"Nested routes", "A route ending in /* matches its path and everything under it; the routes() inside its "
                                  "element match the rest. The outer element is a layout: it stays mounted while its pages "
                                  "change, and the parameters of the outer route are seen inside.",
                 "router::routes({{\"/teams/:team/*\", h(Team)}, {\"*\", h(NotFound)}});\n\n"
                 "Element Team(const Props&) {   // /teams/a, /teams/a/members/7\n"
                 "    return h(\"div\", {}, {\n"
                 "        router::link({.to = \"/teams/\" + router::useParam(\"team\"), .label = \"Team\", .exact = false}),\n"
                 "        router::routes({{\"/\", h(Overview)}, {\"/members/:id\", h(Member)}}),\n"
                 "    });\n"
                 "}"},
                {"Guards and redirects", "A route with .allow = false is skipped, or with .redirect goes there instead "
                                         "(replacing the history entry). router::redirect sends on at once. .allow is a "
                                         "bool computed where routes() is called, so the routes follow your state. "
                                         "router::redirectedFrom() is the path a guard turned away: the login page goes "
                                         "back there after logging in.",
                 "router::routes({\n"
                 "    {\"/\", router::redirect(\"/inbox\")},\n"
                 "    {.path = \"/admin/*\", .element = h(Admin), .allow = user.isAdmin, .redirect = \"/login\"},\n"
                 "    {\"*\", h(NotFound)},\n"
                 "})\n"
                 "// in the login page's handler, after logging in:\n"
                 "std::string from = router::redirectedFrom();\n"
                 "router::navigate(from.empty() ? \"/\" : from, /*replace=*/true);"},
                {"Links", "link changes the page without reloading (Ctrl+click still opens a new tab) and gets "
                          ".activeClassName (\"active\") on the current path (a link with a query, \"/?tag=work\", only while the query has "
                          "those values); navigate and back do it from code. "
                          "useParams() gives every route parameter, currentPath() the path with the query (not a hook), "
                          "href(path) the URL with the base path.",
                 "router::link({.to = \"/todos/7\", .label = \"Open\"});\nrouter::navigate(\"/todos\");\nrouter::navigate(\"/login\", /*replace=*/true);\nrouter::back();\n"
                 "const auto& params = router::useParams();   // {\"id\": \"7\"}\nstd::string url = router::href(\"/todos/7\");  // \"/my-app/todos/7\""},
                {"A base path", "To publish under a folder (like GitHub Pages: /my-app/), set \"web\": {\"base\": \"/my-app/\"} "
                                "in reactx.json. Routes and links are still written without it.", ""},
            },
        },
        {
            "data", "Data: fetch and JSON", "Load data, send it, parse JSON.",
            {
                {"useFetch", "Loads a URL when the component appears (and again when the URL changes).",
                 "auto todos = useFetch(\"/api/todos\");\n"
                 "if (todos.loading) return h(\"p\", {}, {\"Loading...\"});\n"
                 "if (!todos.ok()) return h(\"p\", {}, {todos.errorText()});\n"
                 "for (const auto& t : todos.json().asArray()) ..."},
                {"Live data: useWebSocket", "Messages from the server as they happen, without polling (reactx/websocket.hpp). "
                                            "The handlers run on the UI thread; it reconnects after a drop. In the browser a "
                                            "relative URL goes to the page's host (reactx dev --proxy passes it through); "
                                            "terminal apps give ws://host:port/path.",
                 "auto live = useWebSocket(\"/api/ws\", {\n"
                 "    .onMessage = [dispatch](const std::string& text) { dispatch(parse(text)); },\n"
                 "});\n"
                 "if (!live.open()) return h(\"p\", {}, {\"connecting... \" + live.error});\n"
                 "live.send(R\"({\"text\":\"hi\"})\");"},
                {"fetch", "Any method and body; the answer comes to a callback. rx::urlEncode builds query strings.",
                 "fetch({.method = \"POST\", .url = \"/api/todos\", .body = json::object({{\"title\", \"Milk\"}}).dump()},\n"
                 "      [](const FetchResponse& r) { if (r.ok()) ... });\n\n"
                 "useFetch(\"/api/search?q=\" + rx::urlEncode(query));"},
                {"Headers, timeout, cancel", "A request can carry headers and a deadline (then status 0 and error \"timeout after N ms\"); "
                                             "the answer's headers are in r.header(name). fetch returns a handle: cancel() means the "
                                             "callback never runs. useFetch takes a whole request too and cancels requests it replaces.",
                 "auto me = useFetch({.url = \"/api/me\", .headers = {{\"Authorization\", \"Bearer \" + token}}, .timeout = 5s});\n"
                 "FetchHandle req = fetch({.url = \"/api/items\"}, [](const FetchResponse& r) { r.header(\"etag\"); });\n"
                 "req.cancel();"},
                {"In the terminal", "Terminal apps use the network too: give a full http:// URL (no HTTPS there).",
                 "auto todos = useFetch(\"http://127.0.0.1:8091/api/todos\");"},
                {"An API without a server", "setLocalFetch answers requests inside the app — for static sites, offline mode and tests.",
                 "setLocalFetch([](const FetchRequest& r) -> std::optional<FetchResponse> {\n"
                 "    if (r.url == \"/api/hello\") return FetchResponse{.status = 200, .body = R\"({\"hi\": 1})\"};\n"
                 "    return std::nullopt;   // everything else goes to the network\n"
                 "});"},
                {"rx::json", "JSON with no dependencies: parse, build, read with a fallback, dump.",
                 "auto v = json::parse(R\"({\"title\": \"Milk\", \"done\": false})\");\n"
                 "std::string title = v[\"title\"].asString();\nbool done = v.get<bool>(\"done\");\n"
                 "json::Value todo = json::object({{\"id\", 1}, {\"title\", title}});\ntodo.dump();"},
                {"Strict reading", "get<T> falls back to a default; for data from outside (an import, a server) require<T> "
                                   "returns std::expected: a missing field, another type or 2.9 for an int is an error that names the field.",
                 "auto id = task.require<int>(\"id\");\n"
                 "if (!id) return showError(id.error().what());   // field \"id\": expected an integer that fits"},
                {"Backend (Flask)", "ReactX works with Flask and REST APIs via fetch and useFetch. "
                                    "During development, requests are proxied via reactx dev --proxy. "
                                    "See the «Backend and Flask» page for details and limitations.", ""},
            },
        },
        {
            "server", "C++ server", "A small HTTP server for development and your own APIs.",
            {
                {"HttpServer", "Serves your site's files and your own routes, with parameters. server.onFile(fn) can change "
                               "each file (body or headers) before it is sent: caching headers, an injected script.",
                 "rx::net::HttpServer server(\"127.0.0.1\", 8090);\n"
                 "server.get(\"/api/hello/:name\", [](const rx::net::Request& req) {\n"
                 "    return rx::net::Response::json(json::object({{\"hello\", req.pathParam(\"name\")}}).dump());\n"
                 "});\n"
                 "server.post(\"/api/todos\", [](const rx::net::Request& req) {  // also put, patch, del; req.body, req.header(\"x-token\"), req.param(\"q\")\n"
                 "    return rx::net::Response::json(req.body, 201);\n"
                 "});\n"
                 "server.serveDirectory(\"build/web/dist\", /*spaFallback=*/true);\n"
                 "server.setNotFoundPage(\"404.html\");   // unknown GET paths\n"
                 "server.setLogging(true);                // a line per request\n"
                 "server.start();\nserver.run();"},
                {"Ready programs", "reactx_serve serves a folder for development (with --base and --not-found it behaves like "
                                   "GitHub Pages). api_server is a full-stack example: the todo web app and its REST API in "
                                   "one process; with --data the todos are saved to a file.",
                 "./build/debug/reactx_serve build/web --port 8090\n"
                 "./build/debug/reactx_serve build/pages/site --base /ReactX/ --not-found 404.html\n"
                 "./build/debug/api_server build/web/app --port 8091 --data todos.json"},
                {"WebSocket", "server.webSocket(path, handlers): the handlers run on the connection's thread (guard shared "
                              "data with a mutex); server.webSockets(path) lists open connections to broadcast to.",
                 "server.webSocket(\"/api/ws\", {.onMessage = [&server](const rx::net::WebSocketPtr&, const std::string& text) {\n"
                 "    for (const auto& peer : server.webSockets(\"/api/ws\")) peer->send(text);\n"
                 "}});"},
                {"Proxy", "server.proxy forwards a prefix to another server, WebSockets too (this is what reactx dev --proxy uses).",
                 "server.proxy(\"/api\", *rx::net::parseUpstream(\"http://127.0.0.1:5000\"));"},
                {"Security", "It answers only to localhost, *.localhost and IP addresses (another site that points its "
                             "name at 127.0.0.1 gets 403: DNS rebinding); add names with allowHosts or reactx dev --allow-host. "
                             "WebSockets and POST/PUT/PATCH/DELETE from another site's page (its Origin) get 403. Dot files "
                             "(.env, .git) and symlinks out of the folder aren't served. A request must arrive in 30 s, at most "
                             "512 connections at a time.",
                 "server.allowHosts({\"mypc.local\", \".example.test\"});   // \".x\": x and its subdomains, \"*\": any\n"
                 "server.setRequestTimeout(std::chrono::seconds(10));\n"
                 "server.setMaxConnections(128);"},
                {"Important", "It is a server for development on your machine: no HTTPS, no keep-alive. "
                              "Don't put it on the open internet.", ""},
            },
        },
        {
            "backend", "Backend and Flask", "Connecting ReactX to Flask and REST APIs: capabilities and limitations.",
            {
                {"Working with Flask",
                 "ReactX compiles to WebAssembly and runs in the browser as a client-side app. You can connect it to any backend — Flask, FastAPI, Django, Express, or Go.\n\n"
                 "During development, the dev server proxies API calls to your local backend: reactx dev --proxy http://127.0.0.1:5000. In production, serving the client files (HTML, JS, WASM) and handling the API is done by Flask, Nginx, or an external reverse proxy.",
                 "# app.py — Flask API example\n"
                 "from flask import Flask, jsonify, request\n\n"
                 "app = Flask(__name__)\n\n"
                 "@app.route(\"/api/todos\", methods=[\"GET\", \"POST\"])\n"
                 "def todos():\n"
                 "    if request.method == \"POST\":\n"
                 "        item = request.get_json()\n"
                 "        return jsonify({\"id\": 1, \"title\": item.get(\"title\")}), 201\n"
                 "    return jsonify([{\"id\": 1, \"title\": \"Buy milk\"}])\n\n"
                 "if __name__ == \"__main__\":\n"
                 "    app.run(port=5000)"},
                {"Client example in ReactX",
                 "On the client side, use useFetch (to load data on component mount) or fetch (to send data on user interactions).",
                 "// Load todos from Flask API\n"
                 "auto res = useFetch(\"/api/todos\");\n"
                 "if (res.loading) return h(\"p\", {}, {\"Loading...\"});\n"
                 "if (!res.ok()) return h(\"p\", {}, {\"Error: \" + res.errorText()});\n\n"
                 "// Submit a new todo (POST)\n"
                 "fetch({\n"
                 "    .method = \"POST\",\n"
                 "    .url = \"/api/todos\",\n"
                 "    .body = json::object({{\"title\", \"New task\"}}).dump()\n"
                 "}, [](const FetchResponse& r) {\n"
                 "    if (r.ok()) std::println(\"Todo added!\");\n"
                 "});"},
                {"What works and is tested",
                 "Backend communication is stable and verified in tests and production:\n\n"
                 "- useFetch / fetch in the browser: uses the browser's standard fetch() under the hood, ensuring browser-grade network reliability. Tested in automated suites and real-world projects like TaskFlow and PulseBoard.\n"
                 "- HTTP methods: GET, POST, PUT, PATCH, DELETE with JSON payloads, status codes, network errors, and JSON parsing with std::expected without crashing on malformed responses.\n"
                 "- Proxy: reactx dev --proxy is verified with end-to-end integration tests against real backends and fuzzing; a DoS vulnerability was identified and fixed.",
                 ""},
                {"Current limitations",
                 "What is still missing, as of this release:\n\n"
                 "- Terminal apps speak http:// only (no HTTPS); in the browser fetch is the browser's own, so HTTPS works.\n"
                 "- Server-Sent Events (SSE) aren't wrapped; use useWebSocket for live data, or useInterval to poll.\n"
                 "- The dev proxy (reactx dev --proxy) and HttpServer are for development: in production let Flask, nginx or "
                 "another production server serve the API and the files.\n\n"
                 "Already there (they used to be gaps): request headers (Authorization: Bearer ...), response headers "
                 "(r.header(\"etag\")), a timeout and cancel() — see «Data: fetch and JSON»; WebSockets — useWebSocket.",
                 ""},
            },
        },
    };
    return all;
}

}  // namespace site_docs
