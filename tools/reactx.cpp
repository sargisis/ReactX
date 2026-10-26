// reactx — the ReactX command line tool (C++20).
//
//   reactx new <name> [--terminal] [--local]   create a project
//   reactx dev [--port N] [--native]           build, serve, rebuild on changes
//   reactx build [--native]                    build (web: build/web/dist)
//   reactx add <name> <git-url> [--rev R] [--target T]
//   reactx remove <name>
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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "reactx/http.hpp"
#include "reactx/json.hpp"
#include "reactx/version.hpp"

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

// ---- small helpers ----

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "reactx: " << message << '\n';
    std::exit(1);
}

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) fail("can't read " + p.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void writeFile(const fs::path& p, std::string_view text) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out << text;
    if (!out) fail("can't write " + p.string());
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
        std::cerr << "reactx: can't run " << args[0] << '\n';
        ::_exit(127);
    }
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0)
        if (errno != EINTR) return 127;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

// The folder with reactx.json, from the current one upwards.
fs::path projectRoot() {
    for (fs::path dir = fs::current_path(); ; dir = dir.parent_path()) {
        if (fs::exists(dir / "reactx.json")) return dir;
        if (dir == dir.root_path()) fail("no reactx.json here or in any parent folder (create a project: reactx new <name>)");
    }
}

Value readManifest(const fs::path& root) {
    try {
        return rx::json::parse(readFile(root / "reactx.json"));
    } catch (const rx::json::Error& e) {
        fail("reactx.json: " + std::string(e.what()));
    }
}

void writeManifest(const fs::path& root, const Value& manifest) { writeFile(root / "reactx.json", manifest.dump(2) + "\n"); }

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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Everything comes from reactx.json: edit it with `reactx add` / `reactx remove`.
include(FetchContent)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/reactx.json REACTX_MANIFEST)

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

Element Home(const Props&) {
    auto [count, setCount] = useState(0);
    return h("section", {{"className", "page"}}, {
        h("h1", {}, {"Hello, {{name}}!"}),
        h("p", {}, {"Edit src/main.cpp and save: the page reloads by itself."}),
        ui::button({.label = "Clicked " + std::to_string(count) + " times",
                    .onClick = [setCount] { setCount([](int c) { return c + 1; }); },
                    .variant = ui::Variant::Primary}),
    });
}

Element About(const Props&) {
    return h("section", {{"className", "page"}}, {
        h("h1", {}, {"About"}),
        h("p", {}, {"Made with ReactX: React-like UI in C++20, compiled to WebAssembly."}),
    });
}

Element NotFound(const Props&) {
    return h("section", {{"className", "page"}}, {h("h1", {}, {"404"}), h("p", {}, {"No page at " + router::useLocation()})});
}

Element App(const Props&) {
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

Element App(const Props& props) {
    auto [count, setCount] = useState(0);
    return h("box", {{"flexGrow", 1}, {"justifyContent", "center"}, {"alignItems", "center"}}, {
        h("box", {{"border", true}, {"title", "{{name}}"}, {"padding", 1}, {"gap", 1}, {"color", "cyan"}}, {
            h("text", {}, {"Clicked " + std::to_string(count) + " times"}),
            h("box", {{"direction", "row"}, {"gap", 2}}, {
                ui::button({.label = "+1", .onClick = [setCount] { setCount([](int c) { return c + 1; }); },
                            .variant = ui::Variant::Primary}),
                ui::button({.label = "quit", .onClick = props.get<Callback>("onQuit")}),
            }),
        }),
    });
}

int main() {
    if (!Terminal::isInteractive()) return 1;
    TerminalApp app;
    app.run(h(App, {{"onQuit", Callback([&app] { app.quit(); })}}));
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

constexpr std::string_view kReadme = R"md(# {{name}}

A [ReactX](https://sargisis.github.io/ReactX/) app.

```sh
reactx dev      # build, serve at http://127.0.0.1:8090/, rebuild on changes
reactx build    # {{output}}
```
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
        {"name", name}, {"target", cmakeName(name)},
        {"output", terminal ? "build/native/" + cmakeName(name) : "build/web/dist/ (static files for any host)"}};
    writeManifest(dir, manifest);
    writeFile(dir / "CMakeLists.txt", fill(std::string(kCMakeLists), vars));
    writeFile(dir / "CMakePresets.json", kPresets);
    writeFile(dir / "src/main.cpp", fill(std::string(terminal ? kTerminalMain : kWebMain), vars));
    if (!terminal) writeFile(dir / "public/style.css", kStyle);
    else fs::create_directories(dir / "public");
    writeFile(dir / ".gitignore", kGitignore);
    writeFile(dir / "README.md", fill(std::string(kReadme), vars));

    std::cout << "Created " << name << " (" << (terminal ? "terminal" : "web") << " app)\n\n"
              << "  cd " << name << "\n  reactx dev\n";
    return 0;
}

bool isTerminal(const Value& manifest) { return manifest.get<std::string>("template") == "terminal"; }

// Configures (first time) and builds a preset; returns true on success.
bool build(const fs::path& root, const std::string& preset) {
    if (preset == "web" && !std::getenv("EMSDK"))
        fail("the web build needs Emscripten: install emsdk and `source emsdk_env.sh` (EMSDK is not set)");
    if (!fs::exists(root / "build" / preset / "CMakeCache.txt"))
        if (run({"cmake", "--preset", preset}, root) != 0) return false;
    return run({"cmake", "--build", "--preset", preset}, root) == 0;
}

int cmdBuild(const std::vector<std::string>& args) {
    fs::path root = projectRoot();
    Value manifest = readManifest(root);
    bool native = isTerminal(manifest);
    for (const auto& a : args) {
        if (a == "--native") native = true;
        else fail("build: unknown option " + a);
    }
    std::string preset = native ? "native" : "web";
    if (!build(root, preset)) fail("build failed");
    std::cout << "Built: " << (native ? (root / "build/native").string() : (root / "build/web/dist").string()) << '\n';
    return 0;
}

// Newest modification time of the sources (src/, public/, the manifest, CMake).
fs::file_time_type sourcesTime(const fs::path& root) {
    // min(), not {}: libstdc++ counts file times from 2174, so real ones are negative.
    auto newest = fs::file_time_type::min();
    auto look = [&](const fs::path& p) {
        std::error_code ec;
        auto t = fs::last_write_time(p, ec);
        if (!ec && t > newest) newest = t;
    };
    for (const char* f : {"reactx.json", "CMakeLists.txt", "CMakePresets.json"}) look(root / f);
    for (const char* d : {"src", "public"}) {
        std::error_code ec;
        if (!fs::is_directory(root / d, ec)) continue;
        for (auto it = fs::recursive_directory_iterator(root / d, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            look(it->path());
    }
    return newest;
}

rx::net::HttpServer* devServer = nullptr;
void onSignal(int) {
    if (devServer) devServer->stop();
}

int cmdDev(const std::vector<std::string>& args) {
    fs::path root = projectRoot();
    Value manifest = readManifest(root);
    bool native = isTerminal(manifest);
    int port = 8090;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--port" && i + 1 < args.size()) port = std::stoi(args[++i]);
        else if (args[i] == "--native") native = true;
        else fail("dev: unknown option " + args[i]);
    }

    if (native) {  // terminal app: build and run it
        if (!build(root, "native")) fail("build failed");
        return run({(root / "build/native" / cmakeName(manifest.get<std::string>("name"))).string()}, root);
    }

    std::cout << "reactx: building...\n";
    if (!build(root, "web")) fail("build failed (fix the error and run reactx dev again)");

    std::atomic<int> version{1};
    rx::net::HttpServer server("127.0.0.1", port);
    server.get("/__reactx/version", [&version](const rx::net::Request&) {
        auto r = rx::net::Response::text(std::to_string(version.load()));
        r.headers.emplace_back("Cache-Control", "no-store");
        return r;
    });
    server.serveDirectory(root / "build/web/dist", /*spaFallback=*/true);
    server.start();

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
            std::cout << "reactx: change detected, rebuilding...\n" << std::flush;
            if (build(root, "web")) {
                ++version;
                std::cout << "reactx: rebuilt, reloading the page\n" << std::flush;
            } else {
                std::cout << "reactx: build failed, keeping the previous version\n" << std::flush;
            }
        }
    });

    std::cout << "reactx: http://127.0.0.1:" << server.port() << "/  (Ctrl+C to stop)\n" << std::flush;
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
    Value manifest = readManifest(root);
    Value dep = rx::json::object({{"git", url}, {"rev", rev}});
    if (!target.empty()) dep["target"] = target;
    bool replaced = manifest["dependencies"].contains(name);
    manifest["dependencies"][name] = dep;
    writeManifest(root, manifest);
    std::cout << (replaced ? "Updated " : "Added ") << name << " (" << url << " @ " << rev << ")";
    if (target.empty()) std::cout << "\n  hint: --target <cmake-target> links it to the app automatically";
    std::cout << '\n';
    return 0;
}

int cmdRemove(const std::vector<std::string>& args) {
    if (args.size() != 1) fail("remove: usage: reactx remove <name>");
    fs::path root = projectRoot();
    Value manifest = readManifest(root);
    if (!manifest["dependencies"].contains(args[0])) fail("remove: no dependency " + args[0]);
    manifest["dependencies"].asObject().erase(args[0]);
    writeManifest(root, manifest);
    std::cout << "Removed " << args[0] << '\n';
    return 0;
}

int usage(std::ostream& out, int code) {
    out << "reactx " << kVersion << " - the ReactX tool\n\n"
           "  reactx new <name> [--terminal] [--local]   create a project (web by default)\n"
           "  reactx dev [--port N] [--native]           build, serve, rebuild and reload on changes\n"
           "  reactx build [--native]                    build (web: build/web/dist)\n"
           "  reactx add <name> <git-url> [--rev R] [--target T]   add a dependency\n"
           "  reactx remove <name>                       remove a dependency\n"
           "  reactx help | --version\n";
    return code;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return usage(std::cerr, 2);
    std::string command = args.front();
    args.erase(args.begin());
    try {
        if (command == "new") return cmdNew(args);
        if (command == "dev") return cmdDev(args);
        if (command == "build") return cmdBuild(args);
        if (command == "add") return cmdAdd(args);
        if (command == "remove") return cmdRemove(args);
        if (command == "help" || command == "--help" || command == "-h") return usage(std::cout, 0);
        if (command == "--version") {
            std::cout << "reactx " << kVersion << '\n';
            return 0;
        }
    } catch (const std::exception& e) {
        fail(e.what());
    }
    std::cerr << "reactx: unknown command " << command << "\n\n";
    return usage(std::cerr, 2);
}
