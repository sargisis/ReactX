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
                {"1. Install: Arch Linux",
                 "On Arch Linux, compiler and build tools can be installed via pacman in a single command:\n\n"
                 "- GCC 16, CMake, Ninja, and Git\n"
                 "- Emscripten for WebAssembly builds\n"
                 "- Prebuilt reactx from GitHub Releases or build from source",
                 "# Install tools via pacman:\n"
                 "sudo pacman -S gcc cmake ninja git emscripten\n\n"
                 "# Download prebuilt reactx CLI:\n"
                 "# github.com/sargisis/ReactX/releases -> reactx-X.Y.Z-linux-x86_64.tar.gz\n"
                 "tar xzf reactx-*-linux-x86_64.tar.gz && cp reactx-*/bin/* ~/.local/bin/\n"
                 "reactx --version"},
                {"Install: macOS",
                 "On macOS (Apple Silicon M1/M2/M3/M4 and Intel), tools are installed using Homebrew:\n\n"
                 "- brew install cmake ninja git\n"
                 "- GCC with C++26 support for native builds\n"
                 "- Emscripten for WebAssembly web apps",
                 "# 1. Install dependencies via Homebrew:\n"
                 "brew install cmake ninja git gcc emscripten\n\n"
                 "# 2. Build the reactx CLI from source:\n"
                 "git clone https://github.com/sargisis/ReactX.git && cd ReactX\n"
                 "cmake --preset release -DCMAKE_CXX_COMPILER=g++-14 && cmake --build --preset release\n"
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
                              "Open http://127.0.0.1:8090/ and edit src/main.cpp.",
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
                 "We're using ReactX, a C++26 UI framework (not JavaScript): https://github.com/sargisis/ReactX.\n"
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
                {"Install", "Grab the prebuilt reactx for Linux x86-64 from GitHub Releases, unpack it and put bin/ on your PATH. "
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
                               "src/main.cpp, editor settings and AGENTS.md. ReactX is pinned to a release tag, so new "
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
                {"reactx build", "Builds for publishing: build/web/dist holds plain files for any static host "
                                 "(GitHub Pages, Netlify, nginx). A terminal app goes to build/native/<name>.", "reactx build"},
                {"Libraries", "reactx add records a git repository in reactx.json; CMake fetches it on the next build, "
                              "and --target links it to your app.",
                 "reactx add fmt https://github.com/fmtlib/fmt.git --rev 11.0.2 --target fmt::fmt\nreactx remove fmt"},
                {"reactx update", "Moves your project to the newest ReactX release. --check only tells you whether there is one.",
                 "reactx update --check   # ReactX v0.8.0 -> v0.8.1 is available\n"
                 "reactx update\n"
                 "reactx build"},
                {"reactx.json", "Everything about the project in one file.",
                 "{\n"
                 "  \"name\": \"my-app\",\n"
                 "  \"template\": \"web\",\n"
                 "  \"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v0.8.1\" },\n"
                 "  \"dependencies\": { \"fmt\": { \"git\": \"...\", \"rev\": \"11.0.2\", \"target\": \"fmt::fmt\" } },\n"
                 "  \"web\": { \"base\": \"/\", \"proxy\": { \"/api\": \"http://127.0.0.1:5000\" } }\n"
                 "}"},
            },
        },
        {
            "versions", "Versions", "How ReactX versions work and how to upgrade.",
            {
                {"Version numbers", "ReactX uses Semantic Versioning: MAJOR.MINOR.PATCH. PATCH fixes bugs, MINOR adds features. "
                                    "While MAJOR is 0, a MINOR release may also change the API — the CHANGELOG says what "
                                    "to adjust.", ""},
                {"Upgrading", "reactx update finds the newest release and writes it into reactx.json; the next build downloads it. "
                              "By hand: change rev to the new tag.",
                 "reactx update\n# or in reactx.json:\n\"reactx\": { \"git\": \"https://github.com/sargisis/ReactX.git\", \"rev\": \"v0.8.1\" }"},
                {"The version in code", "",
                 "#include \"reactx/version.hpp\"\n\nstd::println(\"{}\", rx::version());   // \"0.8.1\"\n"
                 "static_assert(REACTX_VERSION_MAJOR == 0);"},
            },
        },
        {
            "safety", "C++26 and safety", "Modern C++, errors as values, checks that catch bugs early.",
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
                             "setting the same value does nothing.",
                 "auto [name, setName] = useState(std::string{});\nsetName(\"Ann\");\nsetCount([](int c) { return c + 1; });"},
                {"useEffect", "Runs after the screen updates. deps(...) says when to run it again; deps() means once. "
                              "Return a function to clean up.",
                 "useEffect([id] {\n    subscribe(id);\n    return [id] { unsubscribe(id); };\n}, deps(id));"},
                {"useRef", "A value that survives re-renders without causing them.", "int& renders = useRef(0);\n++renders;"},
                {"useMemo and useCallback", "useMemo recomputes only when its deps change. useCallback keeps the same handler "
                                            "between renders, so memo() components can skip work.",
                 "const auto& sorted = useMemo([&] { return sortItems(items); }, deps(items));\n"
                 "Callback onSave = useCallback([setSaved] { setSaved(true); }, deps());"},
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
                {"useDebounce, useLocalStorage, useWindowSize, useId",
                 "useDebounce gives a value only after it stopped changing (search boxes). useLocalStorage remembers state across "
                 "page reloads (natively it lives in memory only, until the program exits). useWindowSize re-renders on resize. useId gives an id for <label for>.",
                 "std::string debounced = useDebounce(query, 300ms);\n"
                 "auto [theme, setTheme] = useLocalStorage(\"theme\", std::string(\"dark\"));\n"
                 "WindowSize size = useWindowSize();\n"
                 "std::string id = useId();"},
                {"errorBoundary", "If a component throws while rendering, show a fallback instead of crashing the whole app. "
                                  "retry() tries again. (Exceptions in event handlers and effects aren't caught, as in React.)",
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
                {"Focus and scrolling", "onFocus and onBlur fire when an element gets or loses focus. onScroll (browser) tells "
                                        "you the position; nearBottom() says it's time to load more.",
                 "h(\"div\", {{\"style\", \"height: 400px; overflow: auto\"},\n"
                 "            {\"onScroll\", [loadMore](const ScrollEvent& e) { if (e.nearBottom()) loadMore(); }}}, rows)"},
            },
        },
        {
            "terminal", "Terminal apps", "TerminalApp, tags and Flexbox layout.",
            {
                {"Run", "TerminalApp draws your components in the terminal and handles keys until Ctrl+C.",
                 "TerminalApp app;\napp.run(h(App));"},
                {"Tags", "box is a flex container, text is text that wraps, input is a text field, button is a button. "
                         "Tab and the arrows move focus, Enter or Space press.", ""},
                {"Layout", "Flexbox, the same engine as React Native (Yoga): direction, justifyContent, alignItems, flexGrow, "
                           "width and height in cells or percent, padding, margin, gap, borders.",
                 "h(\"box\", {{\"flexGrow\", 1}, {\"justifyContent\", \"center\"}, {\"alignItems\", \"center\"}}, {\n"
                 "    h(\"box\", {{\"border\", true}, {\"title\", \"Window\"}, {\"padding\", 1}}, {h(\"text\", {}, {\"Hello\"})}),\n"
                 "})"},
            },
        },
        {
            "browser", "Browser apps", "WebAssembly, the DOM and CSS.",
            {
                {"mountDom", "Puts your app into <div id=\"app\"> of the page.", "int main() { mountDom(h(App)); }"},
                {"HTML props", "Any HTML tag works. className becomes class, style is a CSS string, value and checked keep "
                               "inputs in sync with your state, everything else becomes an attribute. A checkbox's onChange gets \"true\" or "
                               "\"false\"; rx::boolHandler turns that into a bool.",
                 "h(\"input\", {{\"type\", \"checkbox\"}, {\"checked\", done},\n"
                 "              {\"onChange\", boolHandler([setDone](bool on) { setDone(on); })}})"},
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
            "ui", "Ready components (rx::ui)", "Buttons, inputs, tabs, dialogs and lists with a theme.",
            {
                {"Components", "button, input, checkbox, tabs, modal, list and listItem. The same code works in the terminal "
                               "and in the browser.",
                 "ui::button({.label = \"Save\", .onClick = save, .variant = ui::Variant::Primary})\n"
                 "ui::input({.value = name, .onChange = setNameFn, .onSubmit = addFn, .label = \"Name\"})\n"
                 "ui::modal({.title = \"Delete?\", .open = asking, .onClose = closeFn, .children = {...}})"},
                {"Theme", "ui::themeProvider passes colors to every ui component below it.",
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
                {"Links", "link changes the page without reloading (Ctrl+click still opens a new tab); navigate and back "
                          "do it from code.",
                 "router::link({.to = \"/todos/7\", .label = \"Open\"});\nrouter::navigate(\"/todos\");\nrouter::back();"},
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
                {"fetch", "Any method and body; the answer comes to a callback. rx::urlEncode builds query strings.",
                 "fetch({.method = \"POST\", .url = \"/api/todos\", .body = json::object({{\"title\", \"Milk\"}}).dump()},\n"
                 "      [](const FetchResponse& r) { if (r.ok()) ... });\n\n"
                 "useFetch(\"/api/search?q=\" + rx::urlEncode(query));"},
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
                {"HttpServer", "Serves your site's files and your own routes, with parameters.",
                 "rx::net::HttpServer server(\"127.0.0.1\", 8090);\n"
                 "server.get(\"/api/hello/:name\", [](const rx::net::Request& req) {\n"
                 "    return rx::net::Response::json(json::object({{\"hello\", req.pathParam(\"name\")}}).dump());\n"
                 "});\n"
                 "server.serveDirectory(\"build/web/dist\", /*spaFallback=*/true);\n"
                 "server.start();\nserver.run();"},
                {"Proxy", "server.proxy forwards a prefix to another server (this is what reactx dev --proxy uses).",
                 "server.proxy(\"/api\", *rx::net::parseUpstream(\"http://127.0.0.1:5000\"));"},
                {"Important", "It is a server for development on your machine: no HTTPS, no protection against heavy load. "
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
                 "Current gaps to keep in mind when designing your application:\n\n"
                 "- Custom request headers: custom headers like Authorization: Bearer <token> cannot be sent yet. This is the main limitation for JWT APIs. Cookie-based authentication works out of the box (the browser sends cookies automatically to the same origin).\n"
                 "- Response headers: response headers (such as Location or pagination headers) cannot be inspected.\n"
                 "- Cancellation and timeouts: AbortController and custom request timeouts are not yet supported.\n"
                 "- WebSockets / Streaming (SSE): streaming responses are not supported; real-time updates require polling via useInterval.\n"
                 "- Terminal applications: console apps support http:// only (no HTTPS).\n"
                 "- Dev proxy: reactx dev --proxy is for local development only. In production, API and static files should be served by Flask, Nginx, or a production web server.",
                 ""},
            },
        },
    };
    return all;
}

}  // namespace site_docs
