// api_server: the whole ReactX web app in one C++ process — serves the site
// (examples/web_app.cpp, built to build/web/app) and its REST API.
//
//   api_server [SITE_DIR] [--port 8090] [--data todos.json]
//
// Without --data the todos live in memory until the server stops.
#include <csignal>
#include <print>
#include <string>
#include <string_view>

#include "reactx/http.hpp"
#include "reactx/parse.hpp"
#include "todo_api.hpp"

namespace {

rx::net::HttpServer* running = nullptr;
void onSignal(int) {
    if (running) running->stop();
}

}  // namespace

int main(int argc, char** argv) {
    std::string site = "build/web/app";
    std::string data;
    int port = 8090;
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
        else if (arg == "--data" && i + 1 < argc) data = argv[++i];
        else if (!arg.starts_with("--")) site = arg;
        else {
            std::println(stderr, "usage: api_server [SITE_DIR] [--port N] [--data FILE.json]");
            return 2;
        }
    }

    try {
        todo_api::Store store(data);
        rx::net::HttpServer server("127.0.0.1", port);
        todo_api::addRoutes(server, store);
        server.serveDirectory(site, /*spaFallback=*/true);  // /todos/7 -> index.html, the router does the rest
        server.setLogging(true);
        server.start();

        running = &server;
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
        std::println("ReactX app: http://127.0.0.1:{}/  (site: {}, todos {})", server.port(), site,
                     data.empty() ? "in memory" : "in " + data);
        std::fflush(stdout);
        server.run();
        running = nullptr;
    } catch (const std::exception& e) {
        std::println(stderr, "{}", e.what());
        return 1;
    }
}
