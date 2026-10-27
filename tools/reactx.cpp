// reactx — the ReactX command line tool (C++26).
//
//   reactx new <name> [--terminal] [--local]   create a project
//   reactx dev [--port N] [--host H] [--proxy [PREFIX=]URL] [--native]
//                                              build, serve, rebuild on changes
//   reactx build [--native]                    build (web: build/web/dist)
//   reactx add <name> <git-url> [--rev R] [--target T]
//   reactx remove <name>
//   reactx update [--check] [--to vX.Y.Z]
//   reactx help | --version
//
// A project is a folder with reactx.json (the manifest), CMakeLists.txt that
// reads it, CMakePresets.json and src/. `reactx dev` serves the web build with
// rx::net::HttpServer (the same server as reactx_serve) and reloads the page
// after every successful rebuild.
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <compare>
#include <format>
#include <print>
#include <ranges>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <thread>
#include <vector>
#include <version>
#if defined(__cpp_lib_generator)
#include <generator>
#endif

#include "reactx/http.hpp"
#include "reactx/json.hpp"
#include "reactx/parse.hpp"
#include "reactx/version.hpp"

// reactx.json is checked with C++26 reflection (rx::json::from).
#if !defined(__cpp_lib_reflection)
#error "the reactx CLI needs C++26 reflection: GCC 16+ (ReactX adds -freflection)"
#endif

namespace fs = std::filesystem;
using rx::json::Value;

namespace {

constexpr std::string_view kVersion = REACTX_VERSION;  // from project(ReactX VERSION ...)
// New projects pin ReactX to the release tag of this CLI, not to a branch.
const std::string kReactXRev = std::string("v") + REACTX_VERSION;
constexpr std::string_view kReactXGit = "https://github.com/sargisis/ReactX.git";

#ifndef REACTX_SOURCE_DIR
#define REACTX_SOURCE_DIR ""
#endif
#ifndef REACTX_CXX_COMPILER
#define REACTX_CXX_COMPILER ""
#endif

// ---- small helpers ----

[[noreturn]] void fail(std::string_view message) {
    std::println(stderr, "reactx: {}", message);
    std::exit(1);
}

// Whole file, or why it can't be read.
std::expected<std::string, std::string> readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return std::unexpected("can't read " + p.string());
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Creates the folders and writes the file, or says why it couldn't.
std::expected<void, std::string> writeFile(const fs::path& p, std::string_view text) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    if (ec) return std::unexpected("can't create " + p.parent_path().string() + ": " + ec.message());
    std::ofstream out(p, std::ios::binary);
    out << text;
    if (!out) return std::unexpected("can't write " + p.string());
    return {};
}

void writeOrFail(const fs::path& p, std::string_view text) {
    if (auto written = writeFile(p, text); !written) fail(written.error());
}

// Replaces every {{key}} in `text`.
std::string fill(std::string text, const std::map<std::string, std::string>& vars) {
    for (const auto& [key, value] : vars) {
        std::string mark = "{{" + key + "}}";
        for (auto pos = text.find(mark); pos != std::string::npos; pos = text.find(mark, pos + value.size()))
            text.replace(pos, mark.size(), value);
    }
    return text;
}

// Runs a program (no shell) in `cwd`; returns its exit code.
int run(const std::vector<std::string>& args, const fs::path& cwd) {
    pid_t pid = ::fork();
    if (pid < 0) fail("fork failed");
    if (pid == 0) {
        if (::chdir(cwd.c_str()) != 0) ::_exit(127);
        std::vector<char*> argv;
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        std::println(stderr, "reactx: can't run {}", args[0]);
        ::_exit(127);
    }
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0)
        if (errno != EINTR) return 127;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

// Runs a program (no shell) and returns what it printed to stdout, or why it
// failed. Its stderr goes to ours, so the user sees git's own messages.
std::expected<std::string, std::string> capture(const std::vector<std::string>& args) {
    int fds[2];
    if (::pipe(fds) != 0) return std::unexpected(std::string("pipe failed"));
    pid_t pid = ::fork();
    if (pid < 0) return std::unexpected(std::string("fork failed"));
    if (pid == 0) {
        ::dup2(fds[1], STDOUT_FILENO);
        ::close(fds[0]);
        ::close(fds[1]);
        std::vector<char*> argv;
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        ::_exit(127);
    }
    ::close(fds[1]);
    std::string out;
    char buf[4096];
    for (ssize_t n; (n = ::read(fds[0], buf, sizeof buf)) != 0;) {
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        out.append(buf, static_cast<std::size_t>(n));
    }
    ::close(fds[0]);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0)
        if (errno != EINTR) return std::unexpected(std::string("waitpid failed"));
    if (!WIFEXITED(status)) return std::unexpected(args[0] + " was stopped");
    if (int code = WEXITSTATUS(status); code != 0)
        return std::unexpected(code == 127 ? "can't run " + args[0] + " (is it installed?)"
                                           : args[0] + " failed (exit code " + std::to_string(code) + ")");
    return out;
}

// ---- versions (Semantic Versioning: vMAJOR.MINOR.PATCH[-pre]) ----

struct SemVer {
    int major = 0, minor = 0, patch = 0;
    std::string pre;  // "rc.1" in v1.0.0-rc.1; empty for a release

    std::string tag() const { return std::format("v{}.{}.{}{}{}", major, minor, patch, pre.empty() ? "" : "-", pre); }
    bool operator==(const SemVer&) const = default;
    // A pre-release comes before its release: v1.0.0-rc.1 < v1.0.0.
    std::strong_ordering operator<=>(const SemVer& o) const {
        if (auto c = std::tie(major, minor, patch) <=> std::tie(o.major, o.minor, o.patch); c != 0) return c;
        if (pre.empty() || o.pre.empty()) return pre.empty() <=> o.pre.empty();  // release > pre-release
        return pre <=> o.pre;
    }
};

// "v0.4.0" / "0.4.0" / "v1.0.0-rc.1"; nothing for "main" or a commit hash.
std::optional<SemVer> parseVersion(std::string_view text) {
    if (text.starts_with('v')) text.remove_prefix(1);
    SemVer v;
    if (auto dash = text.find('-'); dash != std::string_view::npos) {
        v.pre = std::string(text.substr(dash + 1));
        text = text.substr(0, dash);
        if (v.pre.empty()) return std::nullopt;
    }
    int* parts[] = {&v.major, &v.minor, &v.patch};
    for (std::size_t i = 0; i < 3; ++i) {
        auto dot = text.find('.');
        if ((i < 2) == (dot == std::string_view::npos)) return std::nullopt;  // exactly two dots
        auto n = rx::parseNumber<int>(text.substr(0, dot));
        if (!n || *n < 0) return std::nullopt;
        *parts[i] = *n;
        text = i < 2 ? text.substr(dot + 1) : std::string_view{};
    }
    return v;
}

// Release tags of a git repository, newest first (asks the remote; no clone).
std::expected<std::vector<SemVer>, std::string> remoteVersions(const std::string& url) {
    return capture({"git", "ls-remote", "--tags", "--refs", url}).transform([](const std::string& out) {
        std::vector<SemVer> versions;
        for (auto line : std::views::split(out, '\n')) {  // "<sha>\trefs/tags/v0.4.0"
            std::string_view l(line.begin(), line.end());
            auto at = l.find("refs/tags/");
            if (at == std::string_view::npos) continue;
            if (auto v = parseVersion(l.substr(at + 10))) versions.push_back(std::move(*v));
        }
        std::ranges::sort(versions, std::greater{});
        return versions;
    });
}

// The folder with reactx.json, from the current one upwards.
fs::path projectRoot() {
    for (fs::path dir = fs::current_path(); ; dir = dir.parent_path()) {
        if (fs::exists(dir / "reactx.json")) return dir;
        if (dir == dir.root_path()) fail("no reactx.json here or in any parent folder (create a project: reactx new <name>)");
    }
}

// ---- reactx.json ----
//
// The manifest as C++ types. rx::json::from checks every field by C++26
// reflection — the struct is the schema (what the project's CMakeLists.txt
// reads), and a mistake is reported with its place:
//   reactx.json: field "dependencies": "fmt": missing field "rev"

struct Source {  // "reactx": {"path": ...} or {"git": ..., "rev": ...}
    std::optional<std::string> path;
    std::optional<std::string> git;
    std::optional<std::string> rev;
};

struct Dependency {
    std::string git;
    std::string rev;
    std::optional<std::string> target;  // CMake target linked to the app
};

struct WebOptions {
    std::optional<std::string> base;  // <base href>, "/" by default
    // `reactx dev` forwards these URL prefixes to another server: {"/api": "http://127.0.0.1:5000"}
    std::optional<std::map<std::string, std::string>> proxy;
};

struct Manifest {
    std::string name;
    std::optional<std::string> version;
    std::optional<std::string> template_;  // key "template": "web" (default) or "terminal"
    std::optional<Source> reactx;
    std::optional<std::map<std::string, Dependency>> dependencies;
    std::optional<WebOptions> web;
};

// The JSON (kept as it is, so `reactx add` doesn't drop fields it doesn't
// know) and what it says.
struct Project {
    Value json;
    Manifest manifest;
};

// read -> parse -> check, each step std::expected (C++23 and_then / transform_error).
std::expected<Project, std::string> loadManifest(const fs::path& root) {
    auto parse = [](const std::string& text) { return rx::json::tryParse(text); };
    auto check = [](Value json) {
        return rx::json::from<Manifest>(json).transform([&](Manifest m) { return Project{std::move(json), std::move(m)}; });
    };
    return readFile(root / "reactx.json").and_then([&](const std::string& text) {
        return parse(text).and_then(check).transform_error(
            [](const rx::json::Error& e) { return "reactx.json: " + std::string(e.what()); });
    });
}

Project readManifest(const fs::path& root) {
    auto project = loadManifest(root);
    if (!project) fail(project.error());
    return std::move(*project);
}

void writeManifest(const fs::path& root, const Value& manifest) { writeOrFail(root / "reactx.json", manifest.dump(2) + "\n"); }

bool validName(std::string_view name) {
    if (name.empty() || name.size() > 64 || !(std::isalpha(static_cast<unsigned char>(name[0])))) return false;
    return std::ranges::all_of(name, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; });
}

std::string cmakeName(std::string name) {  // my-app -> my_app (a valid CMake target)
    for (auto& c : name)
        if (c == '-') c = '_';
    return name;
}

// ---- templates ----

constexpr std::string_view kCMakeLists = R"cmake(cmake_minimum_required(VERSION 3.24)
project({{target}} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)   # ReactX needs C++26 (GCC 16+; Emscripten 6+ for the web)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Everything comes from reactx.json: edit it with `reactx add` / `reactx remove`.
include(FetchContent)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/reactx.json REACTX_MANIFEST)
# Configure again when the manifest changes (reactx update / add / remove).
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/reactx.json)

# ReactX: a local folder ("path") or a git repository ("git" + "rev").
string(JSON rx_path ERROR_VARIABLE no_path GET "${REACTX_MANIFEST}" reactx path)
if(NOT no_path)
  FetchContent_Declare(reactx SOURCE_DIR ${rx_path})
else()
  string(JSON rx_git GET "${REACTX_MANIFEST}" reactx git)
  string(JSON rx_rev GET "${REACTX_MANIFEST}" reactx rev)
  FetchContent_Declare(reactx GIT_REPOSITORY ${rx_git} GIT_TAG ${rx_rev} GIT_SHALLOW TRUE)
endif()
FetchContent_MakeAvailable(reactx)

# Dependencies: {"name": {"git": "...", "rev": "...", "target": "lib::lib"}}
set(deps)
string(JSON dep_count ERROR_VARIABLE no_deps LENGTH "${REACTX_MANIFEST}" dependencies)
if(NOT no_deps AND dep_count GREATER 0)
  math(EXPR last "${dep_count} - 1")
  foreach(i RANGE ${last})
    string(JSON dep MEMBER "${REACTX_MANIFEST}" dependencies ${i})
    string(JSON dep_git GET "${REACTX_MANIFEST}" dependencies ${dep} git)
    string(JSON dep_rev GET "${REACTX_MANIFEST}" dependencies ${dep} rev)
    FetchContent_Declare(${dep} GIT_REPOSITORY ${dep_git} GIT_TAG ${dep_rev} GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(${dep})
    string(JSON dep_target ERROR_VARIABLE no_target GET "${REACTX_MANIFEST}" dependencies ${dep} target)
    if(NOT no_target)
      list(APPEND deps ${dep_target})
    endif()
  endforeach()
endif()

string(JSON web_base ERROR_VARIABLE no_base GET "${REACTX_MANIFEST}" web base)
if(no_base)
  set(web_base "/")
endif()

reactx_add_app({{target}} SOURCES src/main.cpp PUBLIC public BASE ${web_base})
target_link_libraries({{target}} PRIVATE ${deps})
)cmake";

constexpr std::string_view kPresets = R"json({
  "version": 6,
  "configurePresets": [
    {
      "name": "web",
      "displayName": "Browser (Emscripten)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/web",
      "toolchainFile": "$env{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    },
    {
      "name": "native",
      "displayName": "Native (terminal)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/native",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    }
  ],
  "buildPresets": [
    { "name": "web", "configurePreset": "web" },
    { "name": "native", "configurePreset": "native" }
  ]
}
)json";

constexpr std::string_view kWebMain = R"cpp(// {{name}}: a ReactX web app. `reactx dev` rebuilds and reloads on every save.
#include <string>

#include "reactx/dom.hpp"
#include "reactx/reactx.hpp"
#include "reactx/router.hpp"
#include "reactx/ui.hpp"

using namespace rx;

struct HomeProps {};

Element Home(const HomeProps&) {
    auto [count, setCount] = useState(0);
    return h("section", {{"className", "page"}}, {
        h("h1", {}, {"Hello, {{name}}!"}),
        h("p", {}, {"Edit src/main.cpp and save: the page reloads by itself."}),
        ui::button({.label = "Clicked " + std::to_string(count) + " times",
                    .onClick = [setCount] { setCount([](int c) { return c + 1; }); },
                    .variant = ui::Variant::Primary}),
    });
}

struct AboutProps {};

Element About(const AboutProps&) {
    return h("section", {{"className", "page"}}, {
        h("h1", {}, {"About"}),
        h("p", {}, {"Made with ReactX: React-like UI in C++26, compiled to WebAssembly."}),
    });
}

struct NotFoundProps {};

Element NotFound(const NotFoundProps&) {
    return h("section", {{"className", "page"}}, {h("h1", {}, {"404"}), h("p", {}, {"No page at " + router::useLocation()})});
}

struct AppProps {};

Element App(const AppProps&) {
    return h("div", {}, {
        h("nav", {{"className", "nav"}}, {
            router::link({.to = "/", .label = "Home"}),
            router::link({.to = "/about", .label = "About"}),
        }),
        router::routes({{"/", h(Home)}, {"/about", h(About)}, {"*", h(NotFound)}}),
    });
}

int main() { mountDom(h(App)); }
)cpp";

constexpr std::string_view kTerminalMain = R"cpp(// {{name}}: a ReactX terminal app. Run it with `reactx dev`; Ctrl+C quits.
#include <string>

#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"
#include "reactx/ui.hpp"

using namespace rx;

struct AppProps {
    Callback onQuit{};
};

Element App(const AppProps& props) {
    auto [count, setCount] = useState(0);
    return h("box", {{"flexGrow", 1}, {"justifyContent", "center"}, {"alignItems", "center"}}, {
        h("box", {{"border", true}, {"title", "{{name}}"}, {"padding", 1}, {"gap", 1}, {"color", "cyan"}}, {
            h("text", {}, {"Clicked " + std::to_string(count) + " times"}),
            h("box", {{"direction", "row"}, {"gap", 2}}, {
                ui::button({.label = "+1", .onClick = [setCount] { setCount([](int c) { return c + 1; }); },
                            .variant = ui::Variant::Primary}),
                ui::button({.label = "quit", .onClick = props.onQuit}),
            }),
        }),
    });
}

int main() {
    if (!Terminal::isInteractive()) return 1;
    TerminalApp app;
    app.run(h(App, {.onQuit = Callback([&app] { app.quit(); })}));
}
)cpp";

constexpr std::string_view kStyle = R"css(:root { color-scheme: light dark; font: 16px/1.5 system-ui, sans-serif; }
body { margin: 0; }
#app { max-width: 720px; margin: 0 auto; padding: 24px 16px; }
.nav { display: flex; gap: 12px; margin-bottom: 24px; }
.nav a { color: inherit; opacity: .7; text-decoration: none; }
.nav a.active { opacity: 1; font-weight: 600; }
.page { display: flex; flex-direction: column; align-items: flex-start; gap: 12px; }
h1 { margin: 0; }
)css";

constexpr std::string_view kGitignore = "build/\n";

// The editor: clangd reads the build's own flags from compile_commands.json
// (written by CMake on the first `reactx build` / `reactx dev`). The VS Code
// C/C++ extension's IntelliSense doesn't know C++26 and GCC 16, and reports
// errors in correct code; clangd parses it as the compiler does.
constexpr std::string_view kClangd = R"yaml(# clangd (the editor's C++ engine) reads the flags of the real build.
# compile_commands.json appears there after the first `reactx build` or `reactx dev`.
CompileFlags:
  CompilationDatabase: {{builddir}}
  # GCC-only flags clang doesn't know, and CMake's module maps.
  Remove: [-fcontracts, -freflection, -fmodules-ts, "-fmodule-mapper=*", "-fdeps-format=*", "@*"]
)yaml";

constexpr std::string_view kVscodeSettings = R"json({
  // C++ in the editor through clangd, which understands C++26 like the
  // compiler; the C/C++ extension's IntelliSense would flag correct code.
  "C_Cpp.intelliSenseEngine": "disabled",{{clangd_path}}
  "clangd.arguments": ["--query-driver=**/em++,**/g++*,**/c++", "--header-insertion=never"],
  "cmake.configureOnOpen": false
}
)json";

constexpr std::string_view kVscodeExtensions = R"json({
  "recommendations": ["llvm-vs-code-extensions.vscode-clangd"]
}
)json";

// VS Code "Reopen in Container" / Codespaces: GCC 16, Emscripten and this
// release's reactx, from the image the release workflow publishes.
constexpr std::string_view kDevcontainer = R"json({
  "name": "{{name}}",
  "image": "ghcr.io/sargisis/reactx:{{version}}",
  "remoteUser": "dev",
  "updateRemoteUserUID": true,
  "forwardPorts": [8090],
  "customizations": {
    "vscode": { "extensions": ["ms-vscode.cmake-tools", "llvm-vs-code-extensions.vscode-clangd"] }
  }
}
)json";

constexpr std::string_view kReadme = R"md(# {{name}}

A [ReactX](https://sargisis.github.io/ReactX/) app.

```sh
reactx dev      # build, serve at http://127.0.0.1:8090/, rebuild on changes
reactx build    # {{output}}
```

No GCC 16 or Emscripten here? Open the folder in VS Code and "Reopen in Container"
(or in GitHub Codespaces): `.devcontainer/` has everything.
)md";

// For AI coding agents working in the project (and for people).
constexpr std::string_view kAgents = R"md(# {{name}} — notes for coding agents

A [ReactX](https://sargisis.github.io/ReactX/) {{kind}} app in C++26. ReactX is React for C++: function
components, hooks, a reconciler. The full API and the usual mistakes are in
https://sargisis.github.io/ReactX/llms.txt — read it before writing code.

## Commands

```sh
reactx dev      # build, serve at http://127.0.0.1:8090/, rebuild and reload on save
reactx build    # {{output}}
reactx add <name> <git-url> --rev <tag> --target <cmake-target>   # a library
reactx update   # move to the newest ReactX release
```

Native builds need GCC 16+; web builds need Emscripten (`EMSDK` set).

## Files

- `src/main.cpp` — the app. Every source file includes `reactx/reactx.hpp`
  ({{backend}}), says `using namespace rx;` and the app has a `main`.
- `reactx.json` — name, the ReactX version (`"rev"`), dependencies. Change it with
  `reactx add` / `remove` / `update`, not by editing CMakeLists.txt.
- `CMakeLists.txt` reads `reactx.json`; it rarely needs changes.

## Rules

- Components take a props struct: `Element Card(const CardProps&)`, `h(Card, {.title = "x"})`.
  Give every field a default (`{}`); a `key` field is the list key, a `children` field gets children.
- Hooks only at the top level of a component, in the same order on every render.
- Runtime errors (input, network, files) are `std::expected` values; numbers from text with
  `rx::parseNumber` (`reactx/parse.hpp`), not `std::stoi`.
- Never change component state from another thread; use `useDebounce` / effects for timers.
- Prefer the C++ standard library to extra dependencies.{{web_rules}}
- Test components without a browser or terminal: `rx::TextRenderer` + `rx::Root`, then
  `root.dispatch(node, "onClick")` and check `renderer.toString()`.
)md";

// ---- commands ----

int cmdNew(const std::vector<std::string>& args) {
    std::string name;
    bool terminal = false, local = false;
    for (const auto& a : args) {
        if (a == "--terminal") terminal = true;
        else if (a == "--local") local = true;
        else if (!a.starts_with("--") && name.empty()) name = a;
        else fail("new: unknown option " + a);
    }
    if (!validName(name)) fail("new: give a name of letters, digits, '-' or '_' (starting with a letter): reactx new my-app");
    fs::path dir = fs::current_path() / name;
    if (fs::exists(dir)) fail("new: " + dir.string() + " already exists");

    Value reactx;
    if (local) {
        if (std::string(REACTX_SOURCE_DIR).empty() || !fs::exists(fs::path(REACTX_SOURCE_DIR) / "CMakeLists.txt"))
            fail("new: --local needs the ReactX sources this tool was built from");
        reactx = rx::json::object({{"path", std::string(REACTX_SOURCE_DIR)}});
    } else {
        reactx = rx::json::object({{"git", std::string(kReactXGit)}, {"rev", kReactXRev}});
    }
    Value manifest = rx::json::object({
        {"name", name},
        {"version", "0.1.0"},
        {"template", terminal ? "terminal" : "web"},
        {"reactx", reactx},
        {"dependencies", rx::json::object()},
    });
    if (!terminal) manifest["web"] = rx::json::object({{"base", "/"}});

    std::map<std::string, std::string> vars{
        {"name", name}, {"target", cmakeName(name)}, {"version", std::string(kVersion)},
        {"builddir", terminal ? "build/native" : "build/web"},
        // Web projects have Emscripten, which ships a clangd that knows the wasm target.
        {"clangd_path", terminal ? "" : "\n  \"clangd.path\": \"${env:EMSDK}/upstream/bin/clangd\","},
        {"output", terminal ? "build/native/" + cmakeName(name) : "build/web/dist/ (static files for any host)"},
        {"kind", terminal ? "terminal" : "web"},
        {"backend", terminal ? "plus `reactx/terminal.hpp` for `TerminalApp`" : "plus `reactx/dom.hpp` for `mountDom`"},
        {"web_rules", terminal ? ""
                               : "\n- The browser build has no threads, sockets or files, and no C++26 reflection or"
                                 "\n  `std::generator`. Only click, input, change, keydown and submit events exist."
                                 "\n- Styles live in `public/style.css`."}};
    writeManifest(dir, manifest);
    writeOrFail(dir / "CMakeLists.txt", fill(std::string(kCMakeLists), vars));
    writeOrFail(dir / "CMakePresets.json", kPresets);
    writeOrFail(dir / "src/main.cpp", fill(std::string(terminal ? kTerminalMain : kWebMain), vars));
    if (!terminal) writeOrFail(dir / "public/style.css", kStyle);
    else fs::create_directories(dir / "public");
    writeOrFail(dir / ".gitignore", kGitignore);
    writeOrFail(dir / "README.md", fill(std::string(kReadme), vars));
    writeOrFail(dir / "AGENTS.md", fill(std::string(kAgents), vars));
    writeOrFail(dir / ".clangd", fill(std::string(kClangd), vars));
    writeOrFail(dir / ".vscode/settings.json", fill(std::string(kVscodeSettings), vars));
    writeOrFail(dir / ".vscode/extensions.json", kVscodeExtensions);
    if (!local) writeOrFail(dir / ".devcontainer/devcontainer.json", fill(std::string(kDevcontainer), vars));  // the image has no local sources

    std::println("Created {} ({} app)\n\n  cd {}\n  reactx dev", name, terminal ? "terminal" : "web", name);
    return 0;
}

bool isTerminal(const Manifest& manifest) { return manifest.template_ == "terminal"; }

// Configures (first time) and builds a preset; returns true on success.
bool build(const fs::path& root, const std::string& preset) {
    if (preset == "web" && !std::getenv("EMSDK"))
        fail("the web build needs Emscripten: install emsdk and `source emsdk_env.sh` (EMSDK is not set)");
    fs::path cache = root / "build" / preset / "CMakeCache.txt";
    std::error_code ec;
    bool fresh = !fs::exists(cache, ec);
    // CMake reads reactx.json while configuring: after `reactx update` / `add` /
    // `remove` configure again, so the new ReactX version and dependencies are fetched.
    bool manifestChanged = !fresh && fs::last_write_time(root / "reactx.json", ec) > fs::last_write_time(cache, ec);
    if (fresh || manifestChanged) {
        std::vector<std::string> configure{"cmake", "--preset", preset};
        // ReactX needs GCC 16+ natively; the default `c++` may be older. A downloaded
        // CLI was built elsewhere: its compiler path may not exist on this machine.
        std::string_view compiler = REACTX_CXX_COMPILER;
        if (fresh && preset == "native" && !std::getenv("CXX") && !compiler.empty() && fs::exists(compiler, ec))
            configure.push_back(std::string("-DCMAKE_CXX_COMPILER=") + std::string(compiler));
        if (run(configure, root) != 0) return false;
    }
    return run({"cmake", "--build", "--preset", preset}, root) == 0;
}

int cmdBuild(const std::vector<std::string>& args) {
    fs::path root = projectRoot();
    Manifest manifest = readManifest(root).manifest;
    bool native = isTerminal(manifest);
    for (const auto& a : args) {
        if (a == "--native") native = true;
        else fail("build: unknown option " + a);
    }
    std::string preset = native ? "native" : "web";
    if (!build(root, preset)) fail("build failed");
    std::println("Built: {}", (root / (native ? "build/native" : "build/web/dist")).string());
    return 0;
}

// Every file `reactx dev` watches: the manifest, the CMake files, src/ and public/.
#if defined(__cpp_lib_generator)
std::generator<fs::path> watchedFiles(fs::path root) {  // by value: the coroutine outlives the call
    for (const char* f : {"reactx.json", "CMakeLists.txt", "CMakePresets.json"}) co_yield root / f;
    for (const char* d : {"src", "public"}) {
        std::error_code ec;
        if (!fs::is_directory(root / d, ec)) continue;
        for (auto it = fs::recursive_directory_iterator(root / d, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            co_yield it->path();
    }
}
#else  // a standard library without <generator> (libc++): the same list, collected up front
std::vector<fs::path> watchedFiles(const fs::path& root) {
    std::vector<fs::path> files;
    for (const char* f : {"reactx.json", "CMakeLists.txt", "CMakePresets.json"}) files.push_back(root / f);
    for (const char* d : {"src", "public"}) {
        std::error_code ec;
        if (!fs::is_directory(root / d, ec)) continue;
        for (auto it = fs::recursive_directory_iterator(root / d, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            files.push_back(it->path());
    }
    return files;
}
#endif

// Newest modification time of the watched files.
fs::file_time_type sourcesTime(const fs::path& root) {
    // min(), not {}: libstdc++ counts file times from 2174, so real ones are negative.
    auto newest = fs::file_time_type::min();
    for (const fs::path& p : watchedFiles(root)) {
        std::error_code ec;
        if (auto t = fs::last_write_time(p, ec); !ec) newest = std::max(newest, t);
    }
    return newest;
}

rx::net::HttpServer* devServer = nullptr;
void onSignal(int) {
    if (devServer) devServer->stop();
}

// "--proxy http://127.0.0.1:5000" (prefix /api) or "--proxy /graphql=http://..."
std::pair<std::string, rx::net::Upstream> proxyRule(std::string_view prefix, std::string_view url) {
    if (!prefix.starts_with('/')) fail(std::format("dev: proxy prefix \"{}\" must start with /", prefix));
    auto upstream = rx::net::parseUpstream(url);
    if (!upstream) fail(std::format("dev: proxy {}: {}", prefix, upstream.error()));
    return {std::string(prefix), *upstream};
}

int cmdDev(const std::vector<std::string>& args) {
    fs::path root = projectRoot();
    Manifest manifest = readManifest(root).manifest;
    bool native = isTerminal(manifest);
    int port = 8090;
    std::string host = "127.0.0.1";
    std::map<std::string, rx::net::Upstream> proxies;  // reactx.json first, --proxy overrides
    if (manifest.web && manifest.web->proxy)
        for (const auto& [prefix, url] : *manifest.web->proxy) proxies.insert(proxyRule(prefix, url));
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--port" && i + 1 < args.size()) {
            auto parsed = rx::parsePort(args[++i]);
            if (!parsed) fail("dev: " + parsed.error());
            port = *parsed;
        } else if (args[i] == "--host" && i + 1 < args.size()) {
            host = args[++i];
        } else if (args[i] == "--proxy" && i + 1 < args.size()) {
            std::string_view rule = args[++i];
            auto eq = rule.find('=');
            auto [prefix, upstream] = eq == std::string_view::npos ? proxyRule("/api", rule)
                                                                  : proxyRule(rule.substr(0, eq), rule.substr(eq + 1));
            proxies.insert_or_assign(prefix, upstream);
        } else if (args[i] == "--native") native = true;
        else fail("dev: unknown option " + args[i]);
    }

    if (native) {  // terminal app: build and run it
        if (!build(root, "native")) fail("build failed");
        return run({(root / "build/native" / cmakeName(manifest.name)).string()}, root);
    }

    std::println("reactx: building...");
    if (!build(root, "web")) fail("build failed (fix the error and run reactx dev again)");

    std::atomic<int> version{1};
    rx::net::HttpServer server(host, port);
    server.get("/__reactx/version", [&version](const rx::net::Request&) {
        auto r = rx::net::Response::text(std::to_string(version.load()));
        r.headers.emplace_back("Cache-Control", "no-store");
        return r;
    });
    for (const auto& [prefix, upstream] : proxies) server.proxy(prefix, upstream);
    server.serveDirectory(root / "build/web/dist", /*spaFallback=*/true);
    if (auto started = server.tryStart(); !started) fail(started.error());

    devServer = &server;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    // Watch the sources; rebuild and bump the version after each change.
    std::jthread watcher([&](std::stop_token stop) {
        auto seen = sourcesTime(root);
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            auto now = sourcesTime(root);
            if (now == seen) continue;
            seen = now;
            std::println("reactx: change detected, rebuilding...");
            std::fflush(stdout);
            bool ok = build(root, "web");
            if (ok) ++version;
            std::println("{}", ok ? "reactx: rebuilt, reloading the page" : "reactx: build failed, keeping the previous version");
            std::fflush(stdout);
        }
    });

    for (const auto& [prefix, upstream] : proxies)
        std::println("reactx: {} -> http://{}:{}{}", prefix, upstream.host, upstream.port, upstream.path);
    std::println("reactx: http://{}:{}/  (Ctrl+C to stop)", host == "0.0.0.0" ? "127.0.0.1" : host, server.port());
    std::fflush(stdout);
    server.run();
    devServer = nullptr;
    return 0;
}

int cmdAdd(const std::vector<std::string>& args) {
    std::vector<std::string> positional;
    std::string rev = "main", target;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--rev" && i + 1 < args.size()) rev = args[++i];
        else if (args[i] == "--target" && i + 1 < args.size()) target = args[++i];
        else if (!args[i].starts_with("--")) positional.push_back(args[i]);
        else fail("add: unknown option " + args[i]);
    }
    if (positional.size() != 2) fail("add: usage: reactx add <name> <git-url> [--rev R] [--target T]");
    const std::string& name = positional[0];
    const std::string& url = positional[1];
    if (!validName(name)) fail("add: bad dependency name " + name);
    if (!(url.starts_with("https://") || url.starts_with("git@") || url.starts_with("ssh://") || url.starts_with("file://")))
        fail("add: the git URL must start with https://, git@, ssh:// or file://");

    fs::path root = projectRoot();
    Value manifest = readManifest(root).json;
    Value dep = rx::json::object({{"git", url}, {"rev", rev}});
    if (!target.empty()) dep["target"] = target;
    bool replaced = manifest["dependencies"].contains(name);
    manifest["dependencies"][name] = dep;
    writeManifest(root, manifest);
    std::println("{} {} ({} @ {}){}", replaced ? "Updated" : "Added", name, url, rev,
                 target.empty() ? "\n  hint: --target <cmake-target> links it to the app automatically" : "");
    return 0;
}

int cmdRemove(const std::vector<std::string>& args) {
    if (args.size() != 1) fail("remove: usage: reactx remove <name>");
    fs::path root = projectRoot();
    auto [manifest, info] = readManifest(root);
    if (!info.dependencies || !info.dependencies->contains(args[0])) fail("remove: no dependency " + args[0]);
    manifest["dependencies"].asObject().erase(args[0]);
    writeManifest(root, manifest);
    std::println("Removed {}", args[0]);
    return 0;
}

// Moves the project to the newest ReactX release (or to --to vX.Y.Z).
int cmdUpdate(const std::vector<std::string>& args) {
    bool check = false;
    std::optional<SemVer> wanted;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--check") {
            check = true;
        } else if (args[i] == "--to" && i + 1 < args.size()) {
            wanted = parseVersion(args[++i]);
            if (!wanted) fail("update: --to needs a version like v0.4.0, not " + args[i]);
        } else {
            fail("update: unknown option " + args[i] + " (reactx update [--check] [--to vX.Y.Z])");
        }
    }

    fs::path root = projectRoot();
    auto [json, manifest] = readManifest(root);
    const Source source = manifest.reactx.value_or(Source{});
    if (source.path) {
        std::println("This project uses local ReactX sources ({}): update them there (git pull).", *source.path);
        return 0;
    }
    std::string url = source.git.value_or(std::string(kReactXGit));
    std::string currentRev = source.rev.value_or("main");
    std::optional<SemVer> current = parseVersion(currentRev);

    std::println("reactx: looking for ReactX releases at {}", url);
    std::fflush(stdout);
    auto versions = remoteVersions(url);
    if (!versions) fail("update: can't list the releases: " + versions.error());

    SemVer target;
    if (wanted) {
        if (std::ranges::find(*versions, *wanted) == versions->end()) fail("update: there is no release " + wanted->tag());
        target = *wanted;
    } else {
        auto stable = std::ranges::find_if(*versions, [](const SemVer& v) { return v.pre.empty(); });
        if (stable == versions->end()) fail("update: " + url + " has no releases (tags vX.Y.Z)");
        target = *stable;
    }

    if (!wanted && current && target < *current) {  // e.g. on v0.11.0-rc.1 while the newest release is v0.10.0
        std::println("ReactX {} is newer than the newest release {}: nothing to do (--to {} goes back)", current->tag(),
                     target.tag(), target.tag());
        return 0;
    }
    if (current && *current == target) {
        std::println("ReactX {} — {}", target.tag(), wanted ? "already this version" : "the newest version, nothing to do");
        return 0;
    }
    std::string from = current ? current->tag() : "\"" + currentRev + "\" (not a release)";
    if (check) {
        std::println("ReactX {} → {} is available. Run: reactx update", from, target.tag());
        return 0;
    }

    json["reactx"]["rev"] = target.tag();
    if (!json["reactx"].contains("git")) json["reactx"]["git"] = url;
    writeManifest(root, json);

    std::println("ReactX {} → {}", from, target.tag());
    if (url.starts_with("https://github.com/")) {
        std::string repo = url.ends_with(".git") ? url.substr(0, url.size() - 4) : url;
        std::println("  what changed: {}/blob/{}/CHANGELOG.md", repo, target.tag());
    }
    // While MAJOR is 0 a new MINOR may change the API; so does a new MAJOR.
    if (current && *current < target && (target.major != current->major || (target.major == 0 && target.minor != current->minor)))
        std::println("  {} may need changes in your code: read the CHANGELOG before building", target.tag());
    if (current && target < *current) std::println("  note: {} is older than {}", target.tag(), current->tag());
    if (auto cli = parseVersion(kVersion); cli && *cli < target)
        std::println("  this reactx tool is {}; rebuild it from {} for the newest templates", cli->tag(), target.tag());
    std::println("  next: reactx build (or reactx dev) — it fetches {}", target.tag());
    return 0;
}

int usage(std::FILE* out, int code) {
    std::print(out, "reactx {} - the ReactX tool\n\n"
           "  reactx new <name> [--terminal] [--local]   create a project (web by default)\n"
           "  reactx dev [--port N] [--native]           build, serve, rebuild and reload on changes\n"
           "      [--host 0.0.0.0]                       listen on all interfaces (Docker, phone on Wi-Fi)\n"
           "      [--proxy [PREFIX=]URL]                 forward PREFIX (default /api) to a backend\n"
           "  reactx build [--native]                    build (web: build/web/dist; --native: GCC, main without mountDom)\n"
           "  reactx add <name> <git-url> [--rev R] [--target T]   add a dependency\n"
           "  reactx remove <name>                       remove a dependency\n"
           "  reactx update [--check] [--to vX.Y.Z]      move to the newest ReactX release\n"
           "  reactx help | --version\n"
           "\n<command> --help (or reactx help <command>) explains one command.\n", kVersion);
    return code;
}

}  // namespace

// `reactx <command> --help` / `reactx help <command>`: what one command does
// and its options. Empty for a command that doesn't exist.
std::string_view commandHelp(std::string_view command) {
    if (command == "new")
        return "reactx new <name> [--terminal] [--local]\n\n"
               "Creates the folder <name> with reactx.json, CMakeLists.txt, CMakePresets.json, src/main.cpp,\n"
               "editor settings (.clangd, .vscode/) and AGENTS.md. ReactX comes from GitHub at this CLI's release tag.\n\n"
               "  --terminal   a terminal app (TerminalApp) instead of a web app\n"
               "  --local      use the ReactX sources this CLI was built from (for working on ReactX)\n";
    if (command == "dev")
        return "reactx dev [--port N] [--host H] [--proxy [PREFIX=]URL]... [--native]\n\n"
               "Builds the web app, serves it and rebuilds + reloads the page on every save.\n\n"
               "  --port N                 port of the dev server (8090)\n"
               "  --host H                 address to listen on (127.0.0.1; 0.0.0.0 for containers and other devices)\n"
               "  --proxy [PREFIX=]URL     forward PREFIX (/api) to a backend: --proxy http://127.0.0.1:5000\n"
               "                           (or \"web\": {\"proxy\": {\"/api\": \"http://...\"}} in reactx.json)\n"
               "  --native                 build and run the native (terminal) version instead\n";
    if (command == "build")
        return "reactx build [--native]\n\n"
               "Builds for publishing: web apps into build/web/dist (static files for any host),\n"
               "terminal apps into build/native/<name>.\n\n"
               "  --native   build a web project's sources with GCC; its main() must not call mountDom\n"
               "             (use #ifdef __EMSCRIPTEN__ ... #else TerminalApp ... #endif)\n";
    if (command == "add")
        return "reactx add <name> <git-url> [--rev R] [--target T]\n\n"
               "Adds (or updates) a dependency in reactx.json; CMake fetches it on the next build.\n\n"
               "  --rev R      tag, branch or commit to use (a tag keeps builds reproducible)\n"
               "  --target T   CMake target to link the app with, e.g. fmt::fmt\n\n"
               "Example: reactx add fmt https://github.com/fmtlib/fmt.git --rev 11.0.2 --target fmt::fmt\n";
    if (command == "remove") return "reactx remove <name>\n\nRemoves a dependency from reactx.json.\n";
    if (command == "update")
        return "reactx update [--check] [--to vX.Y.Z]\n\n"
               "Moves the project to the newest ReactX release (by SemVer, skipping -rc) and writes it to reactx.json;\n"
               "the next build fetches it. A new MINOR while MAJOR is 0 may need changes: read the CHANGELOG.\n\n"
               "  --check       only say whether a newer release exists\n"
               "  --to vX.Y.Z   a specific version (also older)\n";
    return {};
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return usage(stderr, 2);
    std::string command = args.front();
    args.erase(args.begin());
    // `reactx add --help`, `reactx help add`
    bool wantsHelp = std::ranges::any_of(args, [](const std::string& a) { return a == "--help" || a == "-h"; });
    if (command == "help" && !args.empty()) std::swap(command, args.front()), wantsHelp = true;
    if (wantsHelp && !commandHelp(command).empty()) {
        std::print("{}", commandHelp(command));
        return 0;
    }
    try {
        if (command == "new") return cmdNew(args);
        if (command == "dev") return cmdDev(args);
        if (command == "build") return cmdBuild(args);
        if (command == "add") return cmdAdd(args);
        if (command == "remove") return cmdRemove(args);
        if (command == "update") return cmdUpdate(args);
        if (command == "help" || command == "--help" || command == "-h") return usage(stdout, 0);
        if (command == "--version") {
            std::println("reactx {}", kVersion);
            return 0;
        }
    } catch (const std::exception& e) {
        fail(e.what());
    }
    std::println(stderr, "reactx: unknown command {}\n", command);
    return usage(stderr, 2);
}
