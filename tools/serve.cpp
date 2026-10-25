// reactx_serve: development web server for ReactX sites, in C++20.
//
//   reactx_serve [DIR] [--port N] [--host IP] [--spa] [--quiet]
//
// Serves DIR (default: build/web) at http://127.0.0.1:8090/. With --spa,
// unknown paths without a file extension get index.html (for a router).
#include <csignal>
#include <iostream>
#include <string>
#include <string_view>

#include "reactx/http.hpp"

namespace {

rx::net::HttpServer* running = nullptr;

void onSignal(int) {
    if (running) running->stop();
}

int usage() {
    std::cerr << "usage: reactx_serve [DIR] [--port N] [--host IP] [--spa] [--quiet]\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = "build/web";
    std::string host = "127.0.0.1";
    int port = 8090;
    bool spa = false, quiet = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--spa") spa = true;
        else if (arg == "--quiet") quiet = true;
        else if (arg == "-h" || arg == "--help") return usage();
        else if (!arg.starts_with("--")) dir = arg;
        else return usage();
    }

    try {
        rx::net::HttpServer server(host, port);
        server.serveDirectory(dir, spa);
        server.setLogging(!quiet);
        server.start();

        running = &server;
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);

        std::cout << "ReactX: serving " << dir << " at http://" << host << ':' << server.port() << "/  (Ctrl+C to stop)"
                  << std::endl;
        server.run();
        running = nullptr;
        std::cout << "ReactX: stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
