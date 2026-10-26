// Tests for the reactx CLI (tools/reactx.cpp), run as a real program.
//   reactx_cli_tests            quick: files, manifest, errors (no compiling)
//   reactx_cli_tests --e2e      slow: new -> build (terminal, and web if EMSDK
//                               is set) -> dev with a rebuild after an edit
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "reactx/json.hpp"
#include "reactx/version.hpp"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

int failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

struct Result {
    int code = -1;
    std::string out;  // stdout + stderr
};

// Runs `reactx args...` in `cwd` and collects its output.
Result reactx(const std::vector<std::string>& args, const fs::path& cwd) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) return {};
    pid_t pid = ::fork();
    if (pid == 0) {
        ::dup2(pipefd[1], 1);
        ::dup2(pipefd[1], 2);
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        if (::chdir(cwd.c_str()) != 0) ::_exit(127);
        std::vector<char*> argv{const_cast<char*>(REACTX_CLI)};
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        ::execv(REACTX_CLI, argv.data());
        ::_exit(127);
    }
    ::close(pipefd[1]);
    Result r;
    char buf[4096];
    for (ssize_t n; (n = ::read(pipefd[0], buf, sizeof buf)) > 0;) r.out.append(buf, static_cast<std::size_t>(n));
    ::close(pipefd[0]);
    int status = 0;
    ::waitpid(pid, &status, 0);
    r.code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    return r;
}

std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

// A fresh folder, removed at the end.
struct TempDir {
    fs::path path = fs::temp_directory_path() / ("reactx_cli_test_" + std::to_string(::getpid()));
    TempDir() {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

// ---- quick tests ----

void quick(const fs::path& tmp) {
    std::cout << "new: web project files and manifest\n";
    auto made = reactx({"new", "my-app", "--local"}, tmp);
    CHECK(made.code == 0);
    CHECK(contains(made.out, "Created my-app (web app)"));
    fs::path app = tmp / "my-app";
    for (const char* f : {"reactx.json", "CMakeLists.txt", "CMakePresets.json", "src/main.cpp", "public/style.css", ".gitignore",
                          "README.md", "AGENTS.md"})
        CHECK(fs::exists(app / f));
    auto agents = read(app / "AGENTS.md");  // notes for AI agents: filled in, web rules, the API link
    CHECK(contains(agents, "# my-app — notes for coding agents") && contains(agents, "reactx/dom.hpp"));
    CHECK(contains(agents, "llms.txt") && contains(agents, "keydown") && !contains(agents, "{{"));
    auto manifest = rx::json::parse(read(app / "reactx.json"));
    CHECK(manifest["name"].asString() == "my-app");
    CHECK(manifest["template"].asString() == "web");
    CHECK(manifest["reactx"]["path"].asString() == REACTX_SOURCE_DIR);
    CHECK(manifest["web"]["base"].asString() == "/");
    CHECK(contains(read(app / "CMakeLists.txt"), "reactx_add_app(my_app SOURCES src/main.cpp"));  // '-' -> '_'
    CHECK(contains(read(app / "src/main.cpp"), "Hello, my-app!"));

    std::cout << "new: terminal project, git dependency by default\n";
    CHECK(reactx({"new", "tui", "--terminal"}, tmp).code == 0);
    auto tui = rx::json::parse(read(tmp / "tui/reactx.json"));
    CHECK(tui["template"].asString() == "terminal");
    CHECK(tui["reactx"]["git"].asString() == "https://github.com/sargisis/ReactX.git");
    CHECK(tui["reactx"]["rev"].asString() == std::string("v") + REACTX_VERSION);  // the release tag, not a branch
    CHECK(!tui.contains("web"));
    CHECK(contains(read(tmp / "tui/src/main.cpp"), "TerminalApp app;"));
    auto tuiAgents = read(tmp / "tui/AGENTS.md");  // terminal flavour: no browser rules
    CHECK(contains(tuiAgents, "reactx/terminal.hpp") && !contains(tuiAgents, "keydown") && !contains(tuiAgents, "{{"));

    std::cout << "new: bad names and existing folders\n";
    CHECK(reactx({"new", "1abc"}, tmp).code != 0);
    CHECK(reactx({"new", "a/b"}, tmp).code != 0);
    CHECK(reactx({"new", "../evil"}, tmp).code != 0);
    auto again = reactx({"new", "my-app"}, tmp);
    CHECK(again.code != 0 && contains(again.out, "already exists"));

    std::cout << "add / remove dependencies (also from a subfolder)\n";
    auto added = reactx({"add", "fmt", "https://github.com/fmtlib/fmt.git", "--rev", "11.0.2", "--target", "fmt::fmt"}, app / "src");
    CHECK(added.code == 0 && contains(added.out, "Added fmt"));
    auto withDep = rx::json::parse(read(app / "reactx.json"));
    CHECK(withDep["dependencies"]["fmt"]["rev"].asString() == "11.0.2");
    CHECK(withDep["dependencies"]["fmt"]["target"].asString() == "fmt::fmt");
    CHECK(reactx({"add", "fmt", "https://github.com/fmtlib/fmt.git", "--rev", "12.0.0"}, app).out.find("Updated fmt") !=
          std::string::npos);
    CHECK(reactx({"add", "bad", "ftp://example.com/x"}, app).code != 0);
    CHECK(reactx({"add", "onlyname"}, app).code != 0);
    CHECK(reactx({"remove", "fmt"}, app).code == 0);
    CHECK(rx::json::parse(read(app / "reactx.json"))["dependencies"].size() == 0);
    CHECK(reactx({"remove", "fmt"}, app).code != 0);

    std::cout << "broken manifest is reported, not crashed on\n";
    std::ofstream(app / "reactx.json") << "{ not json";
    auto broken = reactx({"add", "x", "https://example.com/x.git"}, app);
    CHECK(broken.code == 1 && contains(broken.out, "reactx.json: json: "));
    // Checked field by field (C++26 reflection): the message says where.
    std::ofstream(app / "reactx.json") << R"({"dependencies": {}})";
    CHECK(contains(reactx({"remove", "x"}, app).out, "reactx.json: missing field \"name\""));
    std::ofstream(app / "reactx.json") << R"({"name": "app", "dependencies": []})";
    CHECK(contains(reactx({"remove", "x"}, app).out, "reactx.json: field \"dependencies\": expected an object"));
    std::ofstream(app / "reactx.json") << R"({"name": "app", "dependencies": {"fmt": {"git": "https://x"}}})";
    CHECK(contains(reactx({"remove", "x"}, app).out, "reactx.json: field \"dependencies\": \"fmt\": missing field \"rev\""));

    std::cout << "errors and help\n";
    auto outside = reactx({"build"}, tmp);
    CHECK(outside.code != 0 && contains(outside.out, "no reactx.json"));
    auto unknown = reactx({"frobnicate"}, tmp);
    CHECK(unknown.code == 2 && contains(unknown.out, "unknown command"));
    CHECK(reactx({"help"}, tmp).code == 0);
    CHECK(contains(reactx({"--version"}, tmp).out, std::string("reactx ") + REACTX_VERSION));
}

// `reactx update` against a local git repository with release tags (no network).
void update(const fs::path& tmp) {
    std::cout << "update picks the newest release (SemVer, not text order)\n";
    fs::path repo = tmp / "fake-reactx";
    fs::create_directories(repo);
    std::string git = "git -C " + repo.string() + " -c user.name=t -c user.email=t@t -c init.defaultBranch=main ";
    bool ok = std::system((git + "init -q && " + git + "commit -q --allow-empty -m x").c_str()) == 0;
    for (const char* tag : {"v0.1.0", "v0.2.0", "v0.9.0", "v0.10.0", "v0.11.0-rc.1", "latest", "v1.2"})
        ok = ok && std::system((git + "tag " + tag).c_str()) == 0;
    CHECK(ok);
    if (!ok) return;  // no git here

    fs::path app = tmp / "upd-app";
    fs::create_directories(app);
    auto manifest = [&](const std::string& rev) {
        std::ofstream(app / "reactx.json") << R"({"name": "upd-app", "reactx": {"git": "file://)" + repo.string() +
                                                  R"(", "rev": ")" + rev + R"("}, "dependencies": {}})";
    };
    auto rev = [&] { return rx::json::parse(read(app / "reactx.json"))["reactx"]["rev"].asString(); };

    manifest("v0.2.0");
    auto check = reactx({"update", "--check"}, app);
    CHECK(check.code == 0 && contains(check.out, "v0.2.0 → v0.10.0 is available"));
    CHECK(rev() == "v0.2.0");  // --check changes nothing

    auto up = reactx({"update"}, app);
    CHECK(up.code == 0 && contains(up.out, "ReactX v0.2.0 → v0.10.0"));
    CHECK(contains(up.out, "may need changes in your code"));  // a new MINOR while MAJOR is 0
    CHECK(rev() == "v0.10.0");                                  // not v0.9.0, not the -rc.1
    CHECK(contains(reactx({"update"}, app).out, "the newest version, nothing to do"));

    auto pre = reactx({"update", "--to", "v0.11.0-rc.1"}, app);  // a pre-release only when asked
    CHECK(pre.code == 0 && rev() == "v0.11.0-rc.1");
    auto ahead = reactx({"update"}, app);  // v0.11.0-rc.1 > v0.10.0: never downgrade without --to
    CHECK(ahead.code == 0 && contains(ahead.out, "newer than the newest release v0.10.0") && rev() == "v0.11.0-rc.1");
    CHECK(reactx({"update", "--to", "v0.10.0"}, app).code == 0 && rev() == "v0.10.0");
    auto missing = reactx({"update", "--to", "v3.0.0"}, app);
    CHECK(missing.code == 1 && contains(missing.out, "there is no release v3.0.0"));
    CHECK(reactx({"update", "--to", "latest"}, app).code == 1);  // not a version
    CHECK(reactx({"update", "--sideways"}, app).code == 1);

    manifest("main");  // a branch, not a release: update pins the newest release
    CHECK(contains(reactx({"update"}, app).out, "\"main\" (not a release) → v0.10.0") && rev() == "v0.10.0");

    std::ofstream(app / "reactx.json") << R"({"name": "upd-app", "reactx": {"path": "/src/reactx"}})";
    auto local = reactx({"update"}, app);
    CHECK(local.code == 0 && contains(local.out, "local ReactX sources"));

    manifest("v0.1.0");
    std::ofstream(app / "reactx.json") << R"({"name": "upd-app", "reactx": {"git": "file:///nowhere/at/all", "rev": "v0.1.0"}})";
    auto offline = reactx({"update"}, app);
    CHECK(offline.code == 1 && contains(offline.out, "can't list the releases"));
}

// ---- slow end-to-end tests ----

std::string httpGet(int port, const std::string& path) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        ::close(fd);
        return {};
    }
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: x\r\n\r\n";
    ::send(fd, req.data(), req.size(), MSG_NOSIGNAL);
    std::string raw;
    char buf[8192];
    for (ssize_t n; (n = ::recv(fd, buf, sizeof buf, 0)) > 0;) raw.append(buf, static_cast<std::size_t>(n));
    ::close(fd);
    auto end = raw.find("\r\n\r\n");
    return end == std::string::npos ? std::string{} : raw.substr(end + 4);
}

template <class F>
bool waitFor(F ready, std::chrono::seconds limit) {
    auto until = std::chrono::steady_clock::now() + limit;
    while (std::chrono::steady_clock::now() < until) {
        if (ready()) return true;
        std::this_thread::sleep_for(250ms);
    }
    return false;
}

void endToEnd(const fs::path& tmp) {
    std::cout << "e2e: terminal app builds\n";
    CHECK(reactx({"new", "term-app", "--terminal", "--local"}, tmp).code == 0);
    auto built = reactx({"build"}, tmp / "term-app");
    CHECK(built.code == 0);
    CHECK(fs::exists(tmp / "term-app/build/native/term_app"));
    if (built.code != 0) std::cerr << built.out.substr(built.out.size() > 3000 ? built.out.size() - 3000 : 0) << '\n';

    std::cout << "e2e: a changed reactx.json is picked up (reactx update / add)\n";
    CHECK(!contains(reactx({"build"}, tmp / "term-app").out, "Configuring done"));  // nothing changed
    std::this_thread::sleep_for(1100ms);  // a newer modification time than CMakeCache.txt
    auto text = read(tmp / "term-app/reactx.json");
    std::ofstream(tmp / "term-app/reactx.json") << text;
    auto again = reactx({"build"}, tmp / "term-app");
    CHECK(again.code == 0 && contains(again.out, "Configuring done"));

    if (!std::getenv("EMSDK")) {
        std::cout << "e2e: web part SKIPPED (EMSDK is not set)\n";
        return;
    }
    std::cout << "e2e: web app builds\n";
    fs::path web = tmp / "web-app";
    CHECK(reactx({"new", "web-app", "--local"}, tmp).code == 0);
    auto wb = reactx({"build"}, web);
    CHECK(wb.code == 0);
    for (const char* f : {"index.html", "index.js", "index.wasm", "404.html", "style.css"}) CHECK(fs::exists(web / "build/web/dist" / f));

    std::cout << "e2e: dev serves, rebuilds after an edit, survives a broken edit\n";
    int port = 18000 + ::getpid() % 2000;
    pid_t dev = ::fork();
    if (dev == 0) {
        int null = ::open("/dev/null", O_WRONLY);
        ::dup2(null, 1);
        ::dup2(null, 2);
        if (::chdir(web.c_str()) != 0) ::_exit(127);
        std::string p = std::to_string(port);
        ::execl(REACTX_CLI, REACTX_CLI, "dev", "--port", p.c_str(), static_cast<char*>(nullptr));
        ::_exit(127);
    }
    CHECK(waitFor([&] { return httpGet(port, "/__reactx/version") == "1"; }, 120s));
    CHECK(contains(httpGet(port, "/"), "__reactx/version"));        // the reload script
    CHECK(contains(httpGet(port, "/some/route"), "<div id=app>") ||  // SPA fallback (minified HTML)
          contains(httpGet(port, "/some/route"), "<div id=\"app\">"));

    std::string marker = "edited-" + std::to_string(::getpid());
    std::string original = read(web / "src/main.cpp");
    std::string edited = original;
    edited.replace(edited.find("Hello, web-app!"), std::string("Hello, web-app!").size(), marker);
    std::ofstream(web / "src/main.cpp") << edited;
    CHECK(waitFor([&] { return httpGet(port, "/__reactx/version") == "2"; }, 300s));
    CHECK(contains(httpGet(port, "/index.wasm"), marker));

    std::ofstream(web / "src/main.cpp") << edited << "\nthis is not C++\n";
    std::this_thread::sleep_for(20s);  // enough for the failed rebuild
    CHECK(httpGet(port, "/__reactx/version") == "2");                 // the old build stays
    CHECK(contains(httpGet(port, "/index.wasm"), marker));

    ::kill(dev, SIGINT);
    int status = 0;
    ::waitpid(dev, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);  // Ctrl+C stops it cleanly
}

}  // namespace

int main(int argc, char** argv) {
    bool e2e = argc > 1 && std::string(argv[1]) == "--e2e";
    TempDir tmp;
    try {
        if (e2e) endToEnd(tmp.path);
        else {
            quick(tmp.path);
            update(tmp.path);
        }
    } catch (const std::exception& e) {
        ++failures;
        std::cerr << "  EXCEPTION: " << e.what() << '\n';
    }
    std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("OK, all tests passed")) << '\n';
    return failures ? 1 : 0;
}
