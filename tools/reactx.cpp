// reactx — the ReactX command line tool (C++26).
//
//   reactx new <name> [--terminal] [--local]   create a project
//   reactx dev [--port N] [--host H] [--allow-host NAME] [--proxy [PREFIX=]URL] [--native]
//                                              build, serve, rebuild on changes
//   reactx build [--native]                    build (web: build/web/dist)
//   reactx test [name]                         build and run tests/*.cpp
//   reactx add <name> <git-url> [--rev R] [--target T]
//   reactx remove <name>
//   reactx update [--check] [--to vX.Y.Z]
//   reactx help | --version
//
// A project is a folder with reactx.json (the manifest), CMakeLists.txt that
// reads it, CMakePresets.json and src/. `reactx dev` serves the web build with
// rx::net::HttpServer (the same server as reactx_serve) and reloads the page
// after every successful rebuild.
#include <fcntl.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <unistd.h>
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

#include "explain.hpp"
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

// Whether EMSDK came from the user's shell (emsdk_env.sh), not from reactx
// finding ~/.reactx/emsdk: an editor started from that shell sees it too.
bool userSetEmsdk = false;

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
    std::fflush(stdout);  // our lines first, then the program's (matters when output goes to a file)
    std::fflush(stderr);
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

// --verbose (or REACTX_VERBOSE=1): show the build's own output as it comes,
// instead of a progress line and, on failure, a short report.
bool gVerbose = std::getenv("REACTX_VERBOSE") != nullptr;

// Runs a build step and keeps all it prints (stdout and stderr) in `log`.
// Shows a one-line progress ("building 12/40") on a terminal, everything
// with --verbose.
int runLogged(const std::vector<std::string>& args, const fs::path& cwd, std::string& log) {
    std::fflush(stdout);
    std::fflush(stderr);
    int fds[2];
    if (::pipe(fds) != 0) fail("pipe failed");
    pid_t pid = ::fork();
    if (pid < 0) fail("fork failed");
    if (pid == 0) {
        ::dup2(fds[1], STDOUT_FILENO);
        ::dup2(fds[1], STDERR_FILENO);
        ::close(fds[0]);
        ::close(fds[1]);
        if (::chdir(cwd.c_str()) != 0) ::_exit(127);
        // The compiler in English, whatever the system's language: the report
        // reads its messages (and explains them in the user's language).
        ::setenv("LC_ALL", "C", 1);
        std::vector<char*> argv;
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        std::println(stderr, "reactx: can't run {}", args[0]);
        ::_exit(127);
    }
    ::close(fds[1]);
    const bool tty = ::isatty(STDOUT_FILENO) != 0;
    const bool ru = explain::russianLocale();
    bool progressShown = false;
    std::string pending;
    char buf[8192];
    for (ssize_t n; (n = ::read(fds[0], buf, sizeof buf)) != 0;) {
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        std::string_view chunk(buf, static_cast<std::size_t>(n));
        log.append(chunk);
        if (gVerbose) {
            std::fwrite(chunk.data(), 1, chunk.size(), stdout);
            std::fflush(stdout);
            continue;
        }
        pending.append(chunk);
        for (auto nl = pending.find('\n'); nl != std::string::npos; nl = pending.find('\n')) {
            std::string line = pending.substr(0, nl);
            pending.erase(0, nl + 1);
            // What the build made that the user wants to know: the prerendered pages.
            if (line.starts_with("reactx: prerendered ")) {
                if (progressShown) std::print("\r{}\r", std::string(40, ' '));
                std::println("{}", line);
                std::fflush(stdout);
                progressShown = false;
                continue;
            }
            // Ninja's "[12/40] Building CXX object ..." -> one line that updates.
            if (tty && line.starts_with("[") && line.find('/') != std::string::npos && line.find(']') != std::string::npos) {
                std::print("\r  {} {}   ", ru ? "сборка" : "building", line.substr(1, line.find(']') - 1));
                std::fflush(stdout);
                progressShown = true;
            }
        }
    }
    ::close(fds[0]);
    if (progressShown) std::print("\r{}\r", std::string(40, ' '));
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0)
        if (errno != EINTR) return 127;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

// After a failed step: a short report in plain words (explain.cpp), or the
// output's last lines when it has nothing it understands. The full output
// goes to build/<preset>/reactx-build.log.
void showFailure(const fs::path& root, const std::string& preset, const std::string& log) {
    std::error_code ec;
    fs::path file = root / "build" / preset / "reactx-build.log";
    fs::create_directories(file.parent_path(), ec);
    std::ofstream(file, std::ios::binary) << log;
    if (gVerbose) return;  // it has been shown already
    const bool ru = explain::russianLocale();
    std::string text = explain::report(log, {.root = root, .russian = ru});
    if (text.empty()) {  // nothing recognized: the end of the output, where the reason usually is
        std::vector<std::string_view> lines;
        for (auto part : std::views::split(std::string_view(log), '\n')) lines.emplace_back(part.begin(), part.end());
        std::size_t from = lines.size() > 30 ? lines.size() - 30 : 0;
        for (std::size_t i = from; i < lines.size(); ++i) std::println(stderr, "{}", lines[i]);
    } else {
        std::print(stderr, "{}", text);
    }
    const std::string where = fs::relative(file, root, ec).string();
    if (ru) std::println(stderr, "\n  Весь вывод: {}  (reactx build --verbose показывает его сразу)", where);
    else std::println(stderr, "\n  All of it: {}  (reactx build --verbose shows it as it comes)", where);
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

// Release tags in the output of `git ls-remote --tags`, newest first.
std::vector<SemVer> parseTags(std::string_view out) {
    std::vector<SemVer> versions;
    for (auto line : std::views::split(out, '\n')) {  // "<sha>\trefs/tags/v0.4.0"
        std::string_view l(line.begin(), line.end());
        auto at = l.find("refs/tags/");
        if (at == std::string_view::npos) continue;
        if (auto v = parseVersion(l.substr(at + 10))) versions.push_back(std::move(*v));
    }
    std::ranges::sort(versions, std::greater{});
    return versions;
}

// Release tags of a git repository, newest first (asks the remote; no clone).
std::expected<std::vector<SemVer>, std::string> remoteVersions(const std::string& url) {
    return capture({"git", "ls-remote", "--tags", "--refs", url}).transform(
        [](const std::string& out) { return parseTags(out); });
}

std::optional<SemVer> newestStable(const std::vector<SemVer>& versions) {
    auto it = std::ranges::find_if(versions, [](const SemVer& v) { return v.pre.empty(); });
    if (it == versions.end()) return std::nullopt;
    return *it;
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

reactx_add_app({{target}} SOURCES src/main.cpp src/app.cpp PUBLIC public BASE ${web_base})
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

constexpr std::string_view kWebAppHpp = R"cpp(// {{name}}: the app's components. main.cpp mounts App; tests/ test it (`reactx test`).
#pragma once

#include "reactx/reactx.hpp"

struct AppProps {};

rx::Element App(const AppProps& props);
)cpp";

constexpr std::string_view kWebApp = R"cpp(// {{name}}: a ReactX web app. `reactx dev` rebuilds and reloads on every save.
#include "app.hpp"

#include <string>

#include "reactx/router.hpp"
#include "reactx/ui.hpp"

using namespace rx;

namespace {

struct HomeProps {};

Element Home(const HomeProps&) {
    auto [count, setCount] = useState(0);
    return h("section", {{"className", "page"}}, {
        h("h1", {}, {"Hello, {{name}}!"}),
        h("p", {}, {"Edit src/app.cpp and save: the page reloads by itself."}),
        ui::button({.label = "Clicked " + std::to_string(count) + " times",
                    .onClick = [setCount] { setCount([](int c) { return c + 1; }); },
                    .variant = ui::Variant::Primary}),
    });
}

struct AboutProps {};

Element About(const AboutProps&) {
    useTitle("About");  // the tab's title on this page (prerendered pages get it too)
    return h("section", {{"className", "page"}}, {
        h("h1", {}, {"About"}),
        h("p", {}, {"Made with ReactX: React-like UI in C++, compiled to WebAssembly."}),
    });
}

struct NotFoundProps {};

Element NotFound(const NotFoundProps&) {
    return h("section", {{"className", "page"}}, {h("h1", {}, {"404"}), h("p", {}, {"No page at " + router::useLocation()})});
}

}  // namespace

Element App(const AppProps&) {
    return h("div", {}, {
        h("nav", {{"className", "nav"}}, {
            router::link({.to = "/", .label = "Home"}),
            router::link({.to = "/about", .label = "About"}),
        }),
        router::routes({{"/", h(Home)}, {"/about", h(About)}, {"*", h(NotFound)}}),
    });
}
)cpp";

constexpr std::string_view kWebMain = R"cpp(// {{name}}: puts the app into the page (<main id="app">). The components are in app.cpp.
#include "app.hpp"
#include "reactx/dom.hpp"

int main() { rx::mountDom(rx::h(App)); }
)cpp";

constexpr std::string_view kWebTests = R"cpp(// Tests of the app: `reactx test` builds and runs every tests/*.cpp natively.
// A Screen draws the app on an in-memory terminal (ui:: components in their
// terminal form); press, type and click like a user, then check what it shows.
#include "app.hpp"
#include "reactx/testing.hpp"

using namespace rx;

RX_TEST(home_says_hello) {
    testing::Screen s(h(App));
    CHECK(s.shows("Hello, {{name}}!"));
}

RX_TEST(the_button_counts_clicks) {
    testing::Screen s(h(App));
    CHECK(s.click("Clicked 0 times"));
    CHECK(s.shows("Clicked 1 times"));
}
)cpp";

constexpr std::string_view kTerminalAppHpp = R"cpp(// {{name}}: the app's components. main.cpp runs App; tests/ test it (`reactx test`).
#pragma once

#include "reactx/reactx.hpp"

struct AppProps {
    rx::Callback onQuit{};
};

rx::Element App(const AppProps& props);
)cpp";

constexpr std::string_view kTerminalApp = R"cpp(// {{name}}: a ReactX terminal app. Run it with `reactx dev`; Ctrl+C quits.
#include "app.hpp"

#include <string>

#include "reactx/ui.hpp"

using namespace rx;

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
)cpp";

constexpr std::string_view kTerminalMain = R"cpp(// {{name}}: runs the app in the terminal. The components are in app.cpp.
#include "app.hpp"
#include "reactx/terminal.hpp"

int main() {
    if (!rx::Terminal::isInteractive()) return 1;
    rx::TerminalApp app;
    app.run(rx::h(App, {.onQuit = rx::Callback([&app] { app.quit(); })}));
}
)cpp";

constexpr std::string_view kTerminalTests = R"cpp(// Tests of the app: `reactx test` builds and runs every tests/*.cpp.
// A Screen draws the app on an in-memory terminal; press, type and click
// like a user, then check what it shows.
#include "app.hpp"
#include "reactx/testing.hpp"

using namespace rx;

RX_TEST(plus_one_counts) {
    testing::Screen s(h(App));
    CHECK(s.shows("Clicked 0 times"));
    CHECK(s.click("+1"));
    s.press("Enter");  // the focus is on +1 now
    CHECK(s.shows("Clicked 2 times"));
}

RX_TEST(quit_calls_on_quit) {
    bool quit = false;
    testing::Screen s(h(App, {.onQuit = [&quit] { quit = true; }}));
    s.click("quit");
    CHECK(quit);
}
)cpp";

constexpr std::string_view kStyle = R"css(:root { color-scheme: light dark; font: 16px/1.5 system-ui, sans-serif; }
body { margin: 0; background: Canvas; color: CanvasText; }  /* explicit: contrast checkers read the page's own colors */
#app { max-width: 720px; margin: 0 auto; padding: 24px 16px; }
.nav { display: flex; gap: 12px; margin-bottom: 24px; }
.nav a { color: inherit; opacity: .7; text-decoration: none; }
.nav a.active { opacity: 1; font-weight: 600; }
.page { display: flex; flex-direction: column; gap: 12px; }
.page > .rx-btn { align-self: flex-start; }  /* buttons keep their width; rows, tables and forms fill the page */
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
reactx test     # build and run tests/*.cpp
```

No GCC 16 or Emscripten here? Open the folder in VS Code and "Reopen in Container"
(or in GitHub Codespaces): `.devcontainer/` has everything.
)md";

// For AI coding agents working in the project (and for people).
constexpr std::string_view kAgents = R"md(# {{name}} — notes for coding agents

A [ReactX](https://sargisis.github.io/ReactX/) {{kind}} app in C++ (a C++26 compiler: GCC 16+, Emscripten 6+). ReactX is React for C++: function
components, hooks, a reconciler. The full API and the usual mistakes are in
https://sargisis.github.io/ReactX/llms.txt — read it before writing code.

## Commands

```sh
reactx dev      # build, serve at http://127.0.0.1:8090/, rebuild and reload on save
reactx build    # {{output}}
reactx test     # build and run tests/*.cpp (reactx test <part of a name>: some of them)
reactx add <name> <git-url> --rev <tag> --target <cmake-target>   # a library
reactx update   # move to the newest ReactX release
reactx upgrade  # get the newest reactx tool itself
```

Native builds need GCC 16+; web builds need Emscripten: `reactx install` downloads it (or set `EMSDK`).

## Files

- `src/app.cpp` / `app.hpp` — the components. Every source file includes `reactx/reactx.hpp`
  ({{backend}}) and says `using namespace rx;`. New source files go into `SOURCES` in CMakeLists.txt.
- `src/main.cpp` — only `main`: it {{starts}} `App`. Keep components out of it: tests can't include it.
- `tests/*.cpp` — tests (`reactx/testing.hpp`), run by `reactx test`.
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
- Prefer the C++ standard library to extra dependencies.
- Ready components that work in the browser and the terminal: `reactx/ui.hpp` (`ui::button`, `input`,
  `select`, `checkbox`, `list`, `modal`, `confirm`, `toast` + `toaster`, `card`, `stack`, `text`, ...).{{web_rules}}
- Test with `reactx test`: in tests/*.cpp, `RX_TEST(name) { rx::testing::Screen s(h(App)); s.click("Save");
  s.type("Ann"); s.press("Enter"); CHECK(s.shows("Saved")); }` — the app on an in-memory terminal.
)md";

// `reactx dev` puts this into every page it serves (HttpServer::onFile).
constexpr std::string_view kReloadScript = R"html(<script>
(function poll(version) {
  fetch("__reactx/version", { cache: "no-store" })
    .then(function (r) { if (!r.ok) throw 0; return r.text(); })
    .then(function (v) { if (!/^[0-9]+$/.test(v)) return; if (version && v !== version) location.reload(); else setTimeout(function () { poll(v); }, 700); })
    .catch(function () { setTimeout(function () { poll(version); }, 1500); });
})("");
</script>)html";

// ---- commands ----

// The Emscripten folder for .vscode/settings.json: ${env:EMSDK} when the
// user's shell sets it, else the folder reactx install made (the editor
// doesn't get the EMSDK that reactx sets for its own child processes).
std::string clangdEmsdk() {
    const char* e = std::getenv("EMSDK");
    return userSetEmsdk || !e || !*e ? std::string("${env:EMSDK}") : std::string(e);
}

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
        {"clangd_path", terminal ? "" : "\n  \"clangd.path\": \"" + clangdEmsdk() + "/upstream/bin/clangd\","},
        {"output", terminal ? "build/native/" + cmakeName(name) : "build/web/dist/ (static files for any host)"},
        {"kind", terminal ? "terminal" : "web"},
        {"backend", terminal ? "plus `reactx/terminal.hpp` for `TerminalApp`" : "plus `reactx/dom.hpp` for `mountDom`"},
        {"starts", terminal ? "runs" : "mounts"},
        {"web_rules", terminal ? "\n- Long lists scroll: `ui::list({..., .scroll = true})` or a `box` with `overflow: \"scroll\"`"
                                 "\n  (wheel, PageUp/PageDown, the focus stays visible). Content taller than the terminal is cut off."
                               : "\n- The browser build has no threads, sockets or files, and no C++26 reflection or"
                                 "\n  `std::generator`. Events: onClick, onInput/onChange, onKey, onSubmit, onFocus/onBlur,"
                                 "\n  onScroll, onPointerDown/Move/Up, onMouseEnter/Leave."
                                 "\n- Styles live in `public/style.css`. Pages that should show before the .wasm loads:"
                                 "\n  `\"web\": {\"prerender\": [\"/\", \"/about\"]}` in reactx.json (code before `mountDom` must not touch `document`)."
                                 "\n- Tests run natively (`reactx test`): there `ui::` components draw their terminal form,"
                                 "\n  and `mountDom`, `document` and `window` don't exist."}};
    writeManifest(dir, manifest);
    writeOrFail(dir / "CMakeLists.txt", fill(std::string(kCMakeLists), vars));
    writeOrFail(dir / "CMakePresets.json", kPresets);
    writeOrFail(dir / "src/main.cpp", fill(std::string(terminal ? kTerminalMain : kWebMain), vars));
    writeOrFail(dir / "src/app.hpp", fill(std::string(terminal ? kTerminalAppHpp : kWebAppHpp), vars));
    writeOrFail(dir / "src/app.cpp", fill(std::string(terminal ? kTerminalApp : kWebApp), vars));
    writeOrFail(dir / "tests/app_tests.cpp", fill(std::string(terminal ? kTerminalTests : kWebTests), vars));
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

// ---- tools: reactx install, and finding what it installed ----
//
// Like npm/nvm, the tools live in the user's home, no sudo:
//   ~/.reactx/emsdk          Emscripten (the compiler for the browser), via emsdk
//   ~/.reactx/emsdk-system   an emsdk-shaped link to a system Emscripten (Arch's /usr/lib/emscripten)
//   ~/.reactx/tools/cmake    a portable CMake, if the system one is missing or too old
//   ~/.reactx/tools/ninja    a portable Ninja, if missing
// REACTX_HOME moves the folder (tests).

constexpr std::string_view kEmscriptenVersion = "6.0.13";  // the same as CI and the Docker image
constexpr std::string_view kCmakeVersion = "3.31.6";
constexpr std::string_view kNinjaVersion = "1.12.1";

fs::path reactxHome() {
    if (const char* h = std::getenv("REACTX_HOME"); h && *h) return h;
    const char* home = std::getenv("HOME");
    return fs::path(home && *home ? home : ".") / ".reactx";
}

std::optional<fs::path> findOnPath(std::string_view name) {
    const char* path = std::getenv("PATH");
    if (!path) return std::nullopt;
    for (auto part : std::views::split(std::string_view(path), ':')) {
        fs::path candidate = fs::path(std::string_view(part.begin(), part.end())) / name;
        std::error_code ec;
        if (::access(candidate.c_str(), X_OK) == 0 && fs::is_regular_file(candidate, ec)) return candidate;
    }
    return std::nullopt;
}

bool isEmsdk(const fs::path& dir) {
    std::error_code ec;
    return fs::exists(dir / "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake", ec);
}

// $EMSDK if it is a real one, else what reactx install set up.
std::optional<fs::path> findEmsdk() {
    if (const char* e = std::getenv("EMSDK"); e && *e && isEmsdk(e)) return fs::path(e);
    for (const char* name : {"emsdk", "emsdk-system"})
        if (isEmsdk(reactxHome() / name)) return reactxHome() / name;
    return std::nullopt;
}

void prependPath(const fs::path& dir) {
    const char* old = std::getenv("PATH");
    std::string value = dir.string() + (old && *old ? ":" + std::string(old) : "");
    ::setenv("PATH", value.c_str(), 1);
}

// Makes the tools from `reactx install` visible to cmake, ninja and emcc
// started by this process: no `source emsdk_env.sh`, no PATH edits.
void useInstalledTools() {
    std::error_code ec;
    fs::path tools = reactxHome() / "tools";
    if (fs::exists(tools / "ninja/ninja", ec)) prependPath(tools / "ninja");
    if (fs::exists(tools / "cmake/bin/cmake", ec)) prependPath(tools / "cmake/bin");
    auto emsdk = findEmsdk();
    if (!emsdk) return;
    ::setenv("EMSDK", emsdk->c_str(), 1);
    prependPath(*emsdk / "upstream/emscripten");
    if (fs::exists(*emsdk / ".emscripten", ec) && !std::getenv("EM_CONFIG"))
        ::setenv("EM_CONFIG", (*emsdk / ".emscripten").c_str(), 1);
    for (const auto& node : fs::directory_iterator(*emsdk / "node", ec))  // emsdk's own Node.js
        if (fs::exists(node.path() / "bin/node", ec)) prependPath(node.path() / "bin");
}

// "cmake version 3.31.6" -> {3, 31}
std::optional<std::pair<int, int>> cmakeVersion(const std::string& cmake) {
    auto out = capture({cmake, "--version"});
    if (!out) return std::nullopt;
    auto at = out->find("version ");
    if (at == std::string::npos) return std::nullopt;
    std::string_view v = std::string_view(*out).substr(at + 8);
    auto dot = v.find('.');
    auto major = rx::parseNumber<int>(v.substr(0, dot));
    auto end = v.find_first_not_of("0123456789", dot + 1);
    auto minor = rx::parseNumber<int>(v.substr(dot + 1, end - dot - 1));
    if (!major || !minor) return std::nullopt;
    return std::pair{*major, *minor};
}

// Major version of g++-16 or g++, 0 if there is none.
int gccMajor() {
    for (const char* name : {"g++-16", "g++"}) {
        if (!findOnPath(name)) continue;
        auto out = capture({name, "-dumpversion"});
        if (!out) continue;
        if (auto v = rx::parseNumber<int>(std::string_view(*out).substr(0, out->find_first_not_of("0123456789")))) return *v;
    }
    return 0;
}

bool downloadTo(const std::string& url, const fs::path& file) {
    std::println("  downloading {}", url);
    return run({"curl", "-fL", "--progress-bar", "-o", file.string(), url}, file.parent_path()) == 0;
}

int cmdInstall(const std::vector<std::string>& args) {
    bool check = false;
    std::string emVersion(kEmscriptenVersion);
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--check") check = true;
        else if (args[i] == "--emscripten" && i + 1 < args.size()) emVersion = args[++i];
        else fail("install: unknown option " + args[i]);
    }
    fs::path home = reactxHome();
    fs::path tools = home / "tools";
    std::error_code ec;
    bool ready = true;
    auto ok = [](std::string_view what, std::string_view detail) { std::println("  ok       {:<11} {}", what, detail); };
    auto missing = [&ready](std::string_view what, std::string_view detail) {
        ready = false;
        std::println("  missing  {:<11} {}", what, detail);
    };

#if !defined(__linux__)
    if (!check) fail("install: downloads tools for Linux only; on macOS: brew install cmake ninja emscripten (and gcc for terminal apps)");
#endif
    if (check) std::println("ReactX tools in {} (reactx install --check):", home.string());
    else std::println("Installing the ReactX tools into {}:", home.string());

    // What emsdk and the downloads need from the system: tiny, everywhere.
    // xz: emsdk's archives are .tar.xz, and tar runs xz to unpack them (slim
    // containers don't have it, and then tar fails halfway through).
    std::vector<std::string> basics;
    for (const char* tool : {"curl", "git", "python3", "tar", "xz"})
        if (!findOnPath(tool)) basics.emplace_back(tool);
    if (!basics.empty()) {
        std::string list, apt;
        for (const auto& b : basics) {
            list += (list.empty() ? "" : " ") + b;
            apt += (apt.empty() ? "" : " ") + (b == "xz" ? std::string("xz-utils") : b);
        }
        missing("system", list + " (e.g. sudo apt install " + apt + ", or sudo pacman -S " + list + ")");
        if (!check) fail("install: install " + list + " first, then run reactx install again");
    }

    std::string arch;
    if (utsname u{}; ::uname(&u) == 0) arch = u.machine;
    bool arm = arch == "aarch64" || arch == "arm64";

    // CMake 3.28+: the system one, or a portable one.
    auto systemCmake = findOnPath("cmake");
    auto cmakeV = systemCmake ? cmakeVersion(systemCmake->string()) : std::nullopt;
    if (fs::exists(tools / "cmake/bin/cmake", ec)) {
        ok("cmake", (tools / "cmake").string());
    } else if (cmakeV && *cmakeV >= std::pair{3, 28}) {
        ok("cmake", std::format("{}.{} (system)", cmakeV->first, cmakeV->second));
    } else if (check) {
        missing("cmake", cmakeV ? "3.28+ needed, the system has " + std::to_string(cmakeV->first) + "." + std::to_string(cmakeV->second)
                                : "not found");
    } else {
        std::string name = std::format("cmake-{}-linux-{}", kCmakeVersion, arm ? "aarch64" : "x86_64");
        fs::create_directories(tools, ec);
        fs::path archive = tools / (name + ".tar.gz");
        if (!downloadTo(std::format("https://github.com/Kitware/CMake/releases/download/v{}/{}.tar.gz", kCmakeVersion, name), archive) ||
            run({"tar", "xzf", archive.string(), "-C", tools.string()}, tools) != 0)
            fail("install: couldn't download CMake");
        fs::remove_all(tools / "cmake", ec);
        fs::rename(tools / name, tools / "cmake", ec);
        fs::remove(archive, ec);
        ok("cmake", std::format("{} ({})", kCmakeVersion, (tools / "cmake").string()));
    }

    // Ninja: the system one, or a portable one.
    if (fs::exists(tools / "ninja/ninja", ec)) {
        ok("ninja", (tools / "ninja").string());
    } else if (auto ninja = findOnPath("ninja")) {
        ok("ninja", ninja->string() + " (system)");
    } else if (check) {
        missing("ninja", "not found");
    } else {
        fs::create_directories(tools / "ninja", ec);
        fs::path archive = tools / "ninja.zip";
        if (!downloadTo(std::format("https://github.com/ninja-build/ninja/releases/download/v{}/ninja-linux{}.zip",
                                    kNinjaVersion, arm ? "-aarch64" : ""), archive) ||
            run({"python3", "-m", "zipfile", "-e", archive.string(), (tools / "ninja").string()}, tools) != 0)
            fail("install: couldn't download Ninja");
        fs::permissions(tools / "ninja/ninja", fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::add, ec);
        fs::remove(archive, ec);
        ok("ninja", std::format("{} ({})", kNinjaVersion, (tools / "ninja").string()));
    }

    // Emscripten: $EMSDK, an earlier install, a system emcc, or download emsdk.
    if (auto emsdk = findEmsdk()) {
        ok("emscripten", emsdk->string());
    } else if (auto emcc = findOnPath("emcc"); emcc && fs::exists(fs::canonical(*emcc, ec).parent_path() / "cmake/Modules/Platform/Emscripten.cmake", ec)) {
        fs::path root = fs::canonical(*emcc, ec).parent_path();
        if (check) {
            ok("emscripten", root.string() + " (system; reactx install links it)");
        } else {
            fs::path shim = home / "emsdk-system/upstream";
            fs::create_directories(shim, ec);
            fs::remove(shim / "emscripten", ec);
            fs::create_directory_symlink(root, shim / "emscripten", ec);
            if (ec) fail("install: can't link " + root.string() + ": " + ec.message());
            ok("emscripten", root.string() + " (system)");
        }
    } else if (check) {
        missing("emscripten", "not found (reactx install downloads it, ~1.5 GB)");
    } else {
        fs::path emsdk = home / "emsdk";
        std::println("  Emscripten {} (the compiler for the browser, ~1.5 GB; a few minutes)", emVersion);
        if (!fs::exists(emsdk / "emsdk", ec) &&
            run({"git", "clone", "--depth", "1", "https://github.com/emscripten-core/emsdk.git", emsdk.string()}, home.parent_path()) != 0)
            fail("install: couldn't download emsdk");
        if (run({(emsdk / "emsdk").string(), "install", emVersion}, emsdk) != 0 ||
            run({(emsdk / "emsdk").string(), "activate", emVersion}, emsdk) != 0)
            fail("install: emsdk couldn't install Emscripten " + emVersion +
                 ".\n  A tar error usually means a full disk (it needs ~1.5 GB free in " + home.string() +
                 ") or a broken download: remove " + emsdk.string() + " and run reactx install again." +
                 "\n  No luck? The image ghcr.io/sargisis/reactx has everything installed.");
        std::println("  (reactx build / dev find this Emscripten on their own: no need to source emsdk_env.sh)");
        ok("emscripten", std::format("{} ({})", emVersion, emsdk.string()));
    }

    // GCC 16: only for terminal apps and `--native`; nothing portable to download.
    if (int gcc = gccMajor(); gcc >= 16) ok("gcc", std::format("{} (terminal apps)", gcc));
    else std::println("  note     {:<11} terminal apps need GCC 16+ (found {}): your package manager, or the image ghcr.io/sargisis/reactx",
                      "gcc", gcc == 0 ? std::string("none") : std::to_string(gcc));

    if (!ready) {
        std::println("\nRun `reactx install` to get what is missing.");
        return 1;
    }
    std::println("\nReady. Start a project:\n\n  reactx new my-app\n  cd my-app\n  reactx dev");
    return 0;
}

// Configures (first time) and builds a preset; returns true on success.
bool build(const fs::path& root, const std::string& preset, const std::string& target = {}) {
    if (preset == "web" && !std::getenv("EMSDK"))
        fail("the web build needs Emscripten: run `reactx install` (or set EMSDK)");
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
        std::string log;
        if (runLogged(configure, root, log) != 0) {
            showFailure(root, preset, log);
            return false;
        }
    }
    std::vector<std::string> command{"cmake", "--build", "--preset", preset};
    if (!target.empty()) command.insert(command.end(), {"--target", target});
    std::string log;
    if (runLogged(command, root, log) != 0) {
        showFailure(root, preset, log);
        return false;
    }
    return true;
}

// What makes a web build big, from the linker map reactx_add_app writes
// (wasm-ld -Map). Release builds strip names from the .wasm, the map keeps
// them: which object files instantiate std::format, and whether libc++'s
// floating-point tables (d2fixed) are in.
std::vector<std::string> sizeAdvice(std::string_view map) {
    std::vector<std::string> formatUsers;
    bool floatTables = false;
    for (auto part : std::views::split(map, '\n')) {
        std::string_view line(part.begin(), part.end());
        if (line.contains("(d2fixed.o)")) floatTables = true;
        auto open = line.find(".o:(");
        if (open == std::string_view::npos) continue;
        std::string_view symbol = line.substr(open + 4);
        if (!symbol.contains("__format") && !symbol.contains("vformat")) continue;
        std::string_view object = line.substr(0, open);  // "...  CMakeFiles/app.dir/src/main.cpp"
        object = object.substr(object.find_last_of(" \t") + 1);
        if (object.contains(".a(") || object.empty()) continue;  // a library, not the app's code
        if (auto dir = object.find(".dir/"); dir != std::string_view::npos) object = object.substr(dir + 5);
        if (!std::ranges::contains(formatUsers, object)) formatUsers.emplace_back(object);
    }
    std::vector<std::string> advice;
    if (!formatUsers.empty()) {
        std::string files;
        for (const auto& f : formatUsers) files += (files.empty() ? "" : ", ") + f;
        advice.push_back("std::format / std::print add ~100 KB to the .wasm (used in " + files +
                         "). In browser code build strings with + and std::to_string.");
    } else if (floatTables) {
        advice.push_back("printing a float or double (std::to_chars, std::format) adds ~110 KB of tables to the .wasm. "
                         "std::to_string(x) (6 decimals) or rounding to an integer costs almost nothing.");
    }
    return advice;
}

// The biggest parts of a web build, from the same linker map: the share of
// code and data each source file (the app's own, ReactX's) or library takes.
// The map is written before wasm-opt shrinks the module, so the shares are
// what is worth reading, not the bytes.
std::string biggestParts(std::string_view map, std::size_t count = 5) {
    std::vector<std::pair<std::string, std::uint64_t>> parts;
    std::uint64_t total = 0;
    auto nameOf = [](std::string_view in) -> std::string {
        if (in.starts_with("<")) return {};  // <internal>: made by the linker
        if (auto open = in.find(".a("); open != std::string_view::npos) {
            std::string_view lib = in.substr(0, open);
            lib = lib.substr(lib.find_last_of('/') + 1);
            std::string_view file = in.substr(open + 3);
            file = file.substr(0, file.find(')'));
            if (lib == "libreactx") return "ReactX " + std::string(file.substr(0, file.find(".o")));
            if (lib.starts_with("libc++")) return "the C++ library";
            if (lib.starts_with("libc") || lib.starts_with("libdlmalloc")) return "the C library";
            return std::string(lib);
        }
        if (auto dir = in.find(".dir/"); dir != std::string_view::npos) in = in.substr(dir + 5);
        if (in.ends_with(".o")) in.remove_suffix(2);
        return std::string(in);
    };
    for (auto part : std::views::split(map, '\n')) {
        std::string_view line(part.begin(), part.end());
        // "...main.cpp.o:(symbol)" or "...libreactx.a(dom.cpp.o):(symbol)"
        std::size_t end = line.find(".o:(");
        if (end != std::string_view::npos) end += 2;
        else if (end = line.find(".o):("); end != std::string_view::npos) end += 3;
        else continue;
        std::size_t at = end;
        // "   Addr   Off   Size   In:(symbol)": Size is the third column (hex).
        std::vector<std::string_view> cols;
        for (auto c : std::views::split(line.substr(0, at), ' '))
            if (!std::ranges::empty(c)) cols.emplace_back(c.begin(), c.end());
        if (cols.size() < 4) continue;
        std::uint64_t size = 0;
        if (std::from_chars(cols[2].data(), cols[2].data() + cols[2].size(), size, 16).ec != std::errc{}) continue;
        std::size_t from = line.find(cols[3]);
        std::string name = nameOf(line.substr(from, end - from));
        if (name.empty()) continue;
        total += size;
        auto it = std::ranges::find(parts, name, &std::pair<std::string, std::uint64_t>::first);
        if (it == parts.end()) parts.emplace_back(name, size);
        else it->second += size;
    }
    if (total == 0) return {};
    std::ranges::sort(parts, std::greater{}, &std::pair<std::string, std::uint64_t>::second);
    std::string out;
    for (std::size_t i = 0; i < std::min(count, parts.size()); ++i)
        out += (i ? ", " : "") + parts[i].first + " " + std::to_string((parts[i].second * 100 + total / 2) / total) + "%";
    return out;
}

// After a web build: the size of each .wasm and what to do if it is avoidably big.
void reportWebSize(const fs::path& root) {
    std::error_code ec;
    fs::path dist = root / "build/web/dist";
    if (auto size = fs::file_size(dist / "index.wasm", ec); !ec)
        std::println("index.wasm: {} KB (the page shows nothing until it is downloaded)", (size + 1023) / 1024);
    for (const auto& entry : fs::directory_iterator(root / "build/web", ec)) {
        if (!entry.path().string().ends_with(".link.map")) continue;
        std::ifstream in(entry.path(), std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (auto parts = biggestParts(text); !parts.empty()) std::println("  biggest parts: {}", parts);
        for (const auto& line : sizeAdvice(text)) std::println("reactx: size: {}", line);
    }
}

int cmdBuild(const std::vector<std::string>& args) {
    fs::path root = projectRoot();
    Manifest manifest = readManifest(root).manifest;
    bool native = isTerminal(manifest);
    for (const auto& a : args) {
        if (a == "--native") native = true;
        else if (a == "--verbose") gVerbose = true;
        else fail("build: unknown option " + a);
    }
    std::string preset = native ? "native" : "web";
    // A web project's app is left out of a plain native build (its main calls mountDom): name it.
    std::string target = native && !isTerminal(manifest) ? cmakeName(manifest.name) : std::string();
    if (!build(root, preset, target)) fail("build failed");
    std::println("Built: {}", (root / (native ? "build/native" : "build/web/dist")).string());
    if (!native) reportWebSize(root);
    return 0;
}

// reactx test [name]: builds tests/*.cpp natively (reactx_add_app makes
// <name>_tests of them, with the app's sources except main.cpp) and runs them;
// a name runs only the tests whose name contains it.
int cmdTest(const std::vector<std::string>& args) {
    fs::path root = projectRoot();
    Manifest manifest = readManifest(root).manifest;
    std::string filter;
    for (const auto& a : args) {
        if (a == "--verbose") {
            gVerbose = true;
            continue;
        }
        if (a.starts_with("-")) fail("test: unknown option " + a);
        if (!filter.empty()) fail("test: give at most one name (a part of the tests' names)");
        filter = a;
    }
    std::error_code ec;
    bool any = false;
    for (const auto& entry : fs::directory_iterator(root / "tests", ec)) any = any || entry.path().extension() == ".cpp";
    if (!any)
        fail("test: no tests/*.cpp here. A test file looks like this:\n\n"
             "  #include \"app.hpp\"\n  #include \"reactx/testing.hpp\"\n\n"
             "  RX_TEST(it_says_hello) {\n      rx::testing::Screen s(rx::h(App));\n      CHECK(s.shows(\"Hello\"));\n  }");
    std::string target = cmakeName(manifest.name) + "_tests";
    if (!build(root, "native", target))
        fail("test: the tests don't build (a project made before ReactX v0.18.0 needs `reactx update` first)");
    std::vector<std::string> command{(root / "build/native" / target).string()};
    if (!filter.empty()) command.push_back(filter);
    return run(command, root);
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
    std::vector<std::string> allowedHosts;
    std::map<std::string, rx::net::Upstream> proxies;  // reactx.json first, --proxy overrides
    if (manifest.web && manifest.web->proxy)
        for (const auto& [prefix, url] : *manifest.web->proxy) proxies.insert(proxyRule(prefix, url));
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--port" && i + 1 < args.size()) {
            auto parsed = rx::parsePort(args[++i]);
            if (!parsed) fail("dev: " + parsed.error());
            port = *parsed;
        } else if (args[i] == "--verbose") {
            gVerbose = true;
        } else if (args[i] == "--host" && i + 1 < args.size()) {
            host = args[++i];
        } else if (args[i] == "--allow-host" && i + 1 < args.size()) {
            allowedHosts.push_back(args[++i]);
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
        if (!build(root, "native", isTerminal(manifest) ? std::string() : cmakeName(manifest.name))) fail("build failed");
        return run({(root / "build/native" / cmakeName(manifest.name)).string()}, root);
    }

    std::println("reactx: building...");
    if (!build(root, "web")) fail("build failed (fix the error and run reactx dev again)");

    std::atomic<int> version{1};
    // "web": {"base": "/pulseboard/"}: the page asks for /pulseboard/style.css etc.,
    // like on GitHub Pages, so the files are served under that path too.
    std::string base = manifest.web && manifest.web->base ? *manifest.web->base : "/";
    if (!base.starts_with('/')) base.insert(0, "/");
    if (!base.ends_with('/')) base += '/';

    rx::net::HttpServer server(host, port);
    // GitHub Codespaces opens the page as https://<name>-8090.app.github.dev.
    if (const char* domain = std::getenv("GITHUB_CODESPACES_PORT_FORWARDING_DOMAIN"); domain && *domain)
        allowedHosts.push_back("." + std::string(domain));
    server.allowHosts(allowedHosts);
    rx::net::Handler versionHandler = [&version](const rx::net::Request&) {
        auto r = rx::net::Response::text(std::to_string(version.load()));
        r.headers.emplace_back("Cache-Control", "no-store");
        return r;
    };
    server.get("/__reactx/version", versionHandler);  // the reload script asks relative to <base href>
    if (base != "/") {
        server.get(base + "__reactx/version", versionHandler);
        server.setUrlPrefix(base);
        server.get("/", [base](const rx::net::Request&) {  // the bare address goes to the app
            rx::net::Response r = rx::net::Response::text("see " + base + "\n", 302);
            r.headers.emplace_back("Location", base);
            return r;
        });
    }
    for (const auto& [prefix, upstream] : proxies) server.proxy(prefix, upstream);
    server.serveDirectory(root / "build/web/dist", /*spaFallback=*/true);
    // Reload the page when the app is rebuilt. Only the dev server adds this:
    // a built site must not poll (on a host that answers every path with
    // index.html it would never stop).
    server.onFile([](const std::filesystem::path&, rx::net::Response& r) {
        if (!r.contentType.starts_with("text/html")) return;
        auto end = r.body.rfind("</body>");
        if (end == std::string::npos) end = r.body.size();
        r.body.insert(end, kReloadScript);
    });
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
    std::println("reactx: http://{}:{}{}  (Ctrl+C to stop)", host == "0.0.0.0" ? "127.0.0.1" : host, server.port(), base);
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

// ---- telling about new releases ----
//
// Like npm's update notice: at most once a day `git ls-remote` runs in the
// background (it never slows a command down) and writes the release tags to
// ~/.reactx/update-check.tags; the next command says when there is a newer
// release than this CLI. Off with REACTX_NO_UPDATE_CHECK=1 and in CI.

bool updateChecksOff() {
    for (const char* name : {"REACTX_NO_UPDATE_CHECK", "CI"})
        if (const char* v = std::getenv(name); v && *v && std::string_view(v) != "0") return true;
    return false;
}

// Starts `args` and doesn't wait for it: it outlives reactx, output goes nowhere.
void spawnDetached(const std::vector<std::string>& args) {
    pid_t pid = ::fork();
    if (pid < 0) return;
    if (pid == 0) {
        ::setsid();
        if (::fork() != 0) ::_exit(0);  // the grandchild is adopted by init: no zombie, no waiting
        int null = ::open("/dev/null", O_RDWR);
        if (null >= 0) {
            ::dup2(null, STDIN_FILENO);
            ::dup2(null, STDOUT_FILENO);
            ::dup2(null, STDERR_FILENO);
        }
        ::setenv("GIT_TERMINAL_PROMPT", "0", 1);
        std::vector<char*> argv;
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        ::_exit(127);
    }
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
}

std::string releaseNotesUrl(const SemVer& v) {
    return "https://github.com/sargisis/ReactX/releases/tag/" + v.tag();
}

void updateNotice() {
    if (updateChecksOff()) return;
    fs::path home = reactxHome();
    fs::path stamp = home / "update-check", tags = home / "update-check.tags";
    if (auto text = readFile(tags)) {
        auto newest = newestStable(parseTags(*text));
        auto cli = parseVersion(kVersion);
        if (newest && cli && *cli < *newest) {
            std::println(stderr, "reactx: ReactX {} is out (this reactx is {}). Update: reactx upgrade", newest->tag(),
                         cli->tag());
            std::println(stderr, "        then reactx update in each project. What's new: {}", releaseNotesUrl(*newest));
        }
    }
    std::error_code ec;
    auto checked = fs::last_write_time(stamp, ec);
    if (!ec && fs::file_time_type::clock::now() - checked < std::chrono::hours(24)) return;
    fs::create_directories(home, ec);
    if (!writeFile(stamp, "")) return;  // can't remember the check: don't run one every time
    spawnDetached({"sh", "-c", R"(git ls-remote --tags --refs "$1" > "$2.tmp" && mv "$2.tmp" "$2")", "sh",
                   std::string(kReactXGit), tags.string()});
}

// ---- reactx upgrade: a new reactx from GitHub Releases ----

int cmdUpgrade(const std::vector<std::string>& args) {
    bool check = false;
    std::optional<SemVer> wanted;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--check") {
            check = true;
        } else if (args[i] == "--to" && i + 1 < args.size()) {
            wanted = parseVersion(args[++i]);
            if (!wanted) fail("upgrade: --to needs a version like v0.14.0, not " + args[i]);
        } else {
            fail("upgrade: unknown option " + args[i] + " (reactx upgrade [--check] [--to vX.Y.Z])");
        }
    }
    std::println("reactx: looking for releases at {}", kReactXGit);
    std::fflush(stdout);
    auto versions = remoteVersions(std::string(kReactXGit));
    if (!versions) fail("upgrade: can't list the releases: " + versions.error());
    std::optional<SemVer> target = wanted;
    if (target && std::ranges::find(*versions, *target) == versions->end()) fail("upgrade: there is no release " + target->tag());
    if (!target) target = newestStable(*versions);
    if (!target) fail("upgrade: no releases yet");
    auto cli = parseVersion(kVersion);
    if (cli && *cli == *target) {
        std::println("reactx {} — {}", cli->tag(), wanted ? "already this version" : "the newest version");
        return 0;
    }
    if (cli && !wanted && *target < *cli) {
        std::println("reactx {} is newer than the newest release {}: nothing to do", cli->tag(), target->tag());
        return 0;
    }
    if (check) {
        std::println("reactx {} → {} is available. Run: reactx upgrade", cli ? cli->tag() : std::string(kVersion),
                     target->tag());
        return 0;
    }
#if !defined(__linux__) || !defined(__x86_64__)
    fail("upgrade: the prebuilt reactx is for Linux x86-64; build " + target->tag() + " from source");
#endif
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec) fail("upgrade: can't find this program's file");
    fs::path bin = exe.parent_path();
    if (::access(bin.c_str(), W_OK) != 0)
        fail("upgrade: can't write to " + bin.string() + "; install again: curl -fsSL https://sargisis.github.io/ReactX/install.sh | sh");

    std::string version = target->tag().substr(1);
    std::string name = "reactx-" + version + "-linux-x86_64.tar.gz";
    std::string url = "https://github.com/sargisis/ReactX/releases/download/" + target->tag() + "/" + name;
    fs::path tmp = fs::temp_directory_path() / ("reactx-upgrade-" + std::to_string(::getpid()));
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp);
    struct Cleanup {
        fs::path dir;
        ~Cleanup() {
            std::error_code e;
            fs::remove_all(dir, e);
        }
    } cleanup{tmp};

    std::println("Downloading reactx {}...", version);
    if (run({"curl", "-fL", "--progress-bar", "-o", (tmp / name).string(), url}, tmp) != 0 ||
        run({"curl", "-fsSL", "-o", (tmp / (name + ".sha256")).string(), url + ".sha256"}, tmp) != 0)
        fail("upgrade: download failed: " + url);
    // The checksum file names the archive; compare the hashes only.
    auto expected = readFile(tmp / (name + ".sha256"));
    auto actual = capture({"sha256sum", (tmp / name).string()});
    auto firstWord = [](const std::string& t) { return t.substr(0, t.find_first_of(" \t\n")); };
    if (!expected || !actual || firstWord(*expected).size() != 64 || firstWord(*expected) != firstWord(*actual))
        fail("upgrade: the download doesn't match its checksum; nothing was changed");
    if (run({"tar", "xzf", (tmp / name).string(), "-C", tmp.string()}, tmp) != 0) fail("upgrade: can't unpack " + name);

    fs::path from = tmp / ("reactx-" + version + "-linux-x86_64") / "bin";
    int replaced = 0;
    for (const auto& entry : fs::directory_iterator(from, ec)) {
        fs::path dest = bin / entry.path().filename();
        fs::path next = dest;
        next += ".new";
        fs::copy_file(entry.path(), next, fs::copy_options::overwrite_existing, ec);
        if (!ec) fs::rename(next, dest, ec);  // atomic, even for the running reactx
        if (ec) fail("upgrade: can't replace " + dest.string() + ": " + ec.message());
        ++replaced;
    }
    if (replaced == 0) fail("upgrade: " + name + " has no bin/ folder");
    std::println("Installed reactx {} into {}", version, bin.string());
    // A new release may want another compiler version: the new reactx says what is missing
    // (no surprise 1.5 GB download here).
    if (run({(bin / "reactx").string(), "install", "--check"}, tmp) != 0)
        std::println("Run reactx install to fetch what is missing.");
    std::println("\nNext, in each project: reactx update   (what's new: {})", releaseNotesUrl(*target));
    return 0;
}

int usage(std::FILE* out, int code) {
    std::print(out, "reactx {} - the ReactX tool\n\n"
           "  reactx new <name> [--terminal] [--local]   create a project (web by default)\n"
           "  reactx dev [--port N] [--native]           build, serve, rebuild and reload on changes\n"
           "      [--host 0.0.0.0]                       listen on all interfaces (Docker, phone on Wi-Fi)\n"
           "      [--allow-host NAME]                    answer to NAME too (besides localhost and IP addresses)\n"
           "      [--proxy [PREFIX=]URL]                 forward PREFIX (default /api) to a backend\n"
           "  reactx build [--native]                    build (web: build/web/dist; --native: GCC, main without mountDom)\n"
           "  reactx test [name]                         build and run the tests in tests/*.cpp\n"
           "  reactx add <name> <git-url> [--rev R] [--target T]   add a dependency\n"
           "  reactx remove <name>                       remove a dependency\n"
           "  reactx update [--check] [--to vX.Y.Z]      move the project to the newest ReactX release\n"
           "  reactx upgrade [--check] [--to vX.Y.Z]     replace this reactx with the newest release\n"
           "  reactx install [--check]                   download the compiler and tools into ~/.reactx\n"
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
               "Creates the folder <name> with reactx.json, CMakeLists.txt, CMakePresets.json, src/ (main.cpp, app.cpp),\n"
               "tests/app_tests.cpp (reactx test), "
               "editor settings (.clangd, .vscode/) and AGENTS.md. ReactX comes from GitHub at this CLI's release tag.\n\n"
               "  --terminal   a terminal app (TerminalApp) instead of a web app\n"
               "  --local      use the ReactX sources this CLI was built from (for working on ReactX)\n";
    if (command == "dev")
        return "reactx dev [--port N] [--host H] [--allow-host NAME]... [--proxy [PREFIX=]URL]... [--native]\n\n"
               "Builds the web app, serves it and rebuilds + reloads the page on every save.\n\n"
               "  --port N                 port of the dev server (8090)\n"
               "  --host H                 address to listen on (127.0.0.1; 0.0.0.0 for containers and other devices)\n"
               "  --allow-host NAME        a host name to answer to besides localhost and IP addresses\n"
               "                           (mypc.local, .example.test for it and its subdomains); others get 403\n"
               "  --proxy [PREFIX=]URL     forward PREFIX (/api) to a backend: --proxy http://127.0.0.1:5000\n"
               "                           (or \"web\": {\"proxy\": {\"/api\": \"http://...\"}} in reactx.json)\n"
               "  --native                 build and run the native (terminal) version instead\n";
    if (command == "build")
        return "reactx build [--native]\n\n"
               "Builds for publishing: web apps into build/web/dist (static files for any host),\n"
               "terminal apps into build/native/<name>.\n\n"
               "  --native   build a web project's sources with GCC; its main() must not call mountDom\n"
               "             (use #ifdef __EMSCRIPTEN__ ... #else TerminalApp ... #endif)\n";
    if (command == "test")
        return "reactx test [name]\n\n"
               "Builds the project's tests/*.cpp natively (with the app's sources except main.cpp) and runs\n"
               "them; the exit code says if they passed. A test draws the app on an in-memory terminal:\n\n"
               "  #include \"app.hpp\"\n"
               "  #include \"reactx/testing.hpp\"\n\n"
               "  RX_TEST(saving_works) {\n"
               "      rx::testing::Screen s(rx::h(App));\n"
               "      s.type(\"Ann\");       // into the focused field\n"
               "      s.press(\"Enter\");    // also \"Tab\", \"Ctrl+s\", \"Up\", ...\n"
               "      s.click(\"Save\");     // the first place showing this text\n"
               "      CHECK(s.shows(\"Saved\"));\n"
               "  }\n\n"
               "  name   run only the tests whose name contains it: reactx test saving\n";
    if (command == "add")
        return "reactx add <name> <git-url> [--rev R] [--target T]\n\n"
               "Adds (or updates) a dependency in reactx.json; CMake fetches it on the next build.\n\n"
               "  --rev R      tag, branch or commit to use (a tag keeps builds reproducible)\n"
               "  --target T   CMake target to link the app with, e.g. fmt::fmt\n\n"
               "Example: reactx add fmt https://github.com/fmtlib/fmt.git --rev 11.0.2 --target fmt::fmt\n";
    if (command == "remove") return "reactx remove <name>\n\nRemoves a dependency from reactx.json.\n";
    if (command == "install")
        return "reactx install [--check] [--emscripten VERSION]\n\n"
               "Downloads what web projects need into ~/.reactx, like npm, without sudo: Emscripten (the compiler\n"
               "for the browser, via emsdk, ~1.5 GB) and, if the system has none or an old one, portable CMake and Ninja.\n"
               "A system Emscripten or $EMSDK is used as it is. reactx build / dev then find the tools on their own.\n"
               "Needs curl, git, python3 and tar. Terminal apps also need GCC 16+, which it can't download.\n\n"
               "  --check                only show what is there and what is missing\n"
               "  --emscripten VERSION   another Emscripten version (default 6.0.13)\n";
    if (command == "upgrade")
        return "reactx upgrade [--check] [--to vX.Y.Z]\n\n"
               "Replaces this reactx (and the tools next to it) with the newest release from GitHub Releases,\n"
               "checks the download's SHA-256 and says if the new version needs another compiler (reactx install).\n"
               "Projects keep their version until you run reactx update in them. Linux x86-64.\n"
               "Once a day the other commands check for a new release in the background and say so;\n"
               "REACTX_NO_UPDATE_CHECK=1 turns that off (it is off when CI is set).\n\n"
               "  --check       only say whether a newer release exists\n"
               "  --to vX.Y.Z   a specific version (also older)\n";
    if (command == "update")
        return "reactx update [--check] [--to vX.Y.Z]\n\n"
               "Moves the project to the newest ReactX release (by SemVer, skipping -rc) and writes it to reactx.json;\n"
               "the next build fetches it. A new MAJOR (or, before 1.0, a new MINOR) may need changes: read the CHANGELOG.\n\n"
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
    userSetEmsdk = std::getenv("EMSDK") && *std::getenv("EMSDK");
    useInstalledTools();
    try {
        if (command == "new" || command == "dev" || command == "build" || command == "update") updateNotice();
        if (command == "new") return cmdNew(args);
        if (command == "dev") return cmdDev(args);
        if (command == "build") return cmdBuild(args);
        if (command == "test") return cmdTest(args);
        if (command == "upgrade") return cmdUpgrade(args);
        if (command == "add") return cmdAdd(args);
        if (command == "remove") return cmdRemove(args);
        if (command == "update") return cmdUpdate(args);
        if (command == "install") return cmdInstall(args);
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
