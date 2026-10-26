// reactx_serve: development web server for ReactX sites, in C++20.
//
//   reactx_serve [DIR] [--port N] [--host IP] [--spa] [--base /ReactX/] [--not-found 404.html] [--quiet]
//
// Serves DIR (default: build/web) at http://127.0.0.1:8090/. With --spa,
// unknown paths without a file extension get index.html (for a router).
// --base and --not-found make it behave like GitHub Pages for a project site:
//   reactx_serve build/pages/site --base /ReactX/ --not-found 404.html
#include <csignal>
#include <print>
#include <string>
#include <string_view>

#include "reactx/http.hpp"
#include "reactx/parse.hpp"

namespace {

rx::net::HttpServer* running = nullptr;

void onSignal(int) {
    if (running) running->stop();
}

int usage() {
    std::println(stderr, "usage: reactx_serve [DIR] [--port N] [--host IP] [--spa] [--base PATH] [--not-found FILE] [--quiet]");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = "build/web";
    std::string host = "127.0.0.1";
    int port = 8090;
    bool spa = false, quiet = false;
    std::string base, notFound;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            auto parsed = rx::parsePort(argv[++i]);
            if (!parsed) {
                std::println(stderr, "{}", parsed.error());
                return 2;
            }
            port = *parsed;
        }
        else if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--spa") spa = true;
        else if (arg == "--base" && i + 1 < argc) base = argv[++i];
        else if (arg == "--not-found" && i + 1 < argc) notFound = argv[++i];
        else if (arg == "--quiet") quiet = true;
        else if (arg == "-h" || arg == "--help") return usage();
        else if (!arg.starts_with("--")) dir = arg;
        else return usage();
    }

    try {
        rx::net::HttpServer server(host, port);
        server.serveDirectory(dir, spa);
        if (!base.empty()) server.setUrlPrefix(base);
        if (!notFound.empty()) server.setNotFoundPage(notFound);
        server.setLogging(!quiet);
        server.start();

        running = &server;
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);

        std::string at = base.empty() ? "/" : (base.back() == '/' ? base : base + "/");
        std::println("ReactX: serving {} at http://{}:{}{}  (Ctrl+C to stop)", dir, host, server.port(), at);
        std::fflush(stdout);
        server.run();
        running = nullptr;
        std::println("ReactX: stopped");
    } catch (const std::exception& e) {
        std::println(stderr, "{}", e.what());
        return 1;
    }
}
