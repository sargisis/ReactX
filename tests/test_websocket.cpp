// WebSocket: the server (HttpServer::webSocket), the proxy tunnel and the
// native client behind useWebSocket, over real sockets on 127.0.0.1.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../src/ws_frame.hpp"
#include "reactx/http.hpp"
#include "reactx/reactx.hpp"
#include "reactx/text_renderer.hpp"
#include "reactx/websocket.hpp"

using namespace rx;
using namespace rx::net;
using namespace std::chrono_literals;
namespace ws = rx::detail::ws;

namespace {

int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                           \
    } while (false)

std::vector<std::pair<const char*, void (*)()>> tests;
#define TEST(name)                                                         \
    void name();                                                           \
    const bool name##_registered = (tests.emplace_back(#name, name), true); \
    void name()

struct Running {
    HttpServer server{"127.0.0.1", 0};
    std::jthread thread;
    explicit Running(const std::function<void(HttpServer&)>& setup) {
        setup(server);
        server.start();
        thread = std::jthread([this] { server.run(); });
    }
    ~Running() { server.stop(); }
    int port() const { return server.port(); }
};

bool waitFor(const std::function<bool()>& ok, std::chrono::milliseconds limit = 3s) {
    auto end = std::chrono::steady_clock::now() + limit;
    while (std::chrono::steady_clock::now() < end) {
        if (ok()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return ok();
}

// A raw client: the handshake by hand, then frames.
struct RawClient {
    int fd = -1;
    std::string buffer;
    std::string head;

    RawClient(int port, const std::string& path = "/ws", const std::string& key = "dGhlIHNhbXBsZSBub25jZQ==") {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_port = htons(static_cast<std::uint16_t>(port));
        ::inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&to), sizeof to) != 0) return;
        write("GET " + path + " HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
              "Sec-WebSocket-Key: " + key + "\r\nSec-WebSocket-Version: 13\r\n\r\n");
        std::size_t end;
        while ((end = buffer.find("\r\n\r\n")) == std::string::npos)
            if (!read()) return;
        head = buffer.substr(0, end);
        buffer.erase(0, end + 4);
    }
    ~RawClient() {
        if (fd >= 0) ::close(fd);
    }
    void write(std::string_view data) { (void)!::send(fd, data.data(), data.size(), MSG_NOSIGNAL); }
    void sendText(std::string_view text) { write(ws::encode(ws::Text, text, true)); }
    bool read() {
        pollfd p{.fd = fd, .events = POLLIN, .revents = 0};
        if (::poll(&p, 1, 3000) <= 0) return false;
        char buf[8192];
        ssize_t n = ::recv(fd, buf, sizeof buf, 0);
        if (n <= 0) return false;
        buffer.append(buf, static_cast<std::size_t>(n));
        return true;
    }
    std::optional<ws::Frame> frame() {
        for (;;) {
            int error = 0;
            if (auto f = ws::take(buffer, false, error)) return f;
            if (error || !read()) return std::nullopt;
        }
    }
};

// ---- the wire format ----

TEST(sha1_and_accept_key) {
    auto d = ws::sha1("abc");
    CHECK(d[0] == 0xA9 && d[1] == 0x99 && d[19] == 0x9D);              // a9993e36...c89d
    CHECK(ws::acceptKey("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");  // RFC 6455's example
    CHECK(ws::base64("ab") == "YWI=" && ws::base64("abc") == "YWJj");
}

TEST(frames_round_trip) {
    for (std::size_t size : {0uz, 5uz, 125uz, 126uz, 70000uz}) {
        std::string payload(size, 'x');
        for (bool mask : {false, true}) {
            std::string wire = ws::encode(ws::Text, payload, mask);
            int error = 0;
            std::string cut = wire.substr(0, wire.size() - 1);
            CHECK(!ws::take(cut, mask, error) && error == 0);  // not all there yet
            auto f = ws::take(wire, mask, error);
            CHECK(f && f->payload == payload && f->op == ws::Text && wire.empty());
        }
    }
    std::string wrong = ws::encode(ws::Text, "hi", false);
    int error = 0;
    CHECK(!ws::take(wrong, /*expectMasked=*/true, error) && error == 1002);  // a client must mask
}

// ---- the server ----

TEST(server_handshake_echo_ping_close) {
    std::atomic<int> closedWith{0};
    Running r([&](HttpServer& s) {
        s.webSocket("/ws", {
            .onOpen = [](const WebSocketPtr& w) { w->send("welcome " + w->request().param("name").value_or("?")); },
            .onMessage = [](const WebSocketPtr& w, const std::string& m) { w->send("echo:" + m); },
            .onClose = [&](const WebSocketPtr&, int code) { closedWith = code; },
        });
    });
    RawClient c(r.port(), "/ws?name=ann");
    CHECK(c.head.starts_with("HTTP/1.1 101"));
    CHECK(c.head.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
    auto hello = c.frame();
    CHECK(hello && hello->payload == "welcome ann");
    c.sendText("hi");
    auto echo = c.frame();
    CHECK(echo && echo->payload == "echo:hi");
    // A message in two frames.
    const std::string zeroMask(4, '\0');
    c.write(std::string("\x01\x83") + zeroMask + "abc");  // text, not FIN, masked with 0
    c.write(std::string("\x80\x82") + zeroMask + "de");    // continuation, FIN
    auto joined = c.frame();
    CHECK(joined && joined->payload == "echo:abcde");
    c.write(ws::encode(ws::Ping, "p1", true));
    auto pong = c.frame();
    CHECK(pong && pong->op == ws::Pong && pong->payload == "p1");
    c.write(ws::encode(ws::Close, ws::closePayload(1000), true));
    auto bye = c.frame();
    CHECK(bye && bye->op == ws::Close);
    CHECK(waitFor([&] { return closedWith == 1000; }));
}

TEST(server_broadcasts_and_rejects_bad_clients) {
    Running r([&](HttpServer& s) {
        s.webSocket("/room", {.onMessage = [&s](const WebSocketPtr&, const std::string& m) {
                                  for (const auto& peer : s.webSockets("/room")) peer->send(m);
                              }});
    });
    RawClient a(r.port(), "/room"), b(r.port(), "/room");
    CHECK(waitFor([&] { return r.server.webSockets("/room").size() == 2; }));
    a.sendText("to all");
    auto ga = a.frame(), gb = b.frame();
    CHECK(ga && ga->payload == "to all" && gb && gb->payload == "to all");

    // Unmasked from a client: closed with 1002.
    b.write(ws::encode(ws::Text, "bad", false));
    auto closed = b.frame();
    CHECK(closed && closed->op == ws::Close && closed->payload.size() >= 2 &&
          ((static_cast<unsigned char>(closed->payload[0]) << 8) | static_cast<unsigned char>(closed->payload[1])) == 1002);
    CHECK(waitFor([&] { return r.server.webSockets("/room").size() == 1; }));

    // Over 1 MB: 1009.
    a.write(ws::encode(ws::Text, std::string(ws::kMaxMessage + 1, 'x'), true));
    auto big = a.frame();
    CHECK(big && big->op == ws::Close &&
          ((static_cast<unsigned char>(big->payload[0]) << 8) | static_cast<unsigned char>(big->payload[1])) == 1009);

    // Not a WebSocket route: a normal 404; no key: 400.
    RawClient none(r.port(), "/nope");
    CHECK(none.head.starts_with("HTTP/1.1 404"));
    RawClient noKey(r.port(), "/room", "");
    CHECK(noKey.head.starts_with("HTTP/1.1 400"));
}

TEST(proxy_passes_websockets_through) {
    Running backend([](HttpServer& s) {
        s.webSocket("/api/ws", {.onMessage = [](const WebSocketPtr& w, const std::string& m) { w->send("backend:" + m); }});
    });
    Running front([&](HttpServer& s) { s.proxy("/api", *parseUpstream("http://127.0.0.1:" + std::to_string(backend.port()))); });
    RawClient c(front.port(), "/api/ws");
    CHECK(c.head.starts_with("HTTP/1.1 101"));
    c.sendText("through");
    auto f = c.frame();
    CHECK(f && f->payload == "backend:through");
}

// ---- useWebSocket (the native client) ----

struct Log {
    std::mutex m;
    std::vector<std::string> lines;
    void add(std::string s) {
        std::scoped_lock l(m);
        lines.push_back(std::move(s));
    }
};

// Shows the status and every message; says hello once open.
struct ChatProps {
    std::string url{};
    bool reconnect = true;
};

Element Chat(const ChatProps& p) {
    auto [messages, setMessages] = useState(std::string{});
    auto socket = useWebSocket(p.url, {
        .onMessage = [setMessages](const std::string& m) { setMessages([m](const std::string& all) { return all + "[" + m + "]"; }); },
        .reconnect = p.reconnect,
        .retryDelay = 20ms,
    });
    auto send = socket.send;
    return h("box", {}, {
        h("text", {}, {socket.open() ? "open" : socket.status == WebSocketStatus::Connecting ? "connecting" : "closed:" + socket.error}),
        h("text", {}, {messages}),
        h("button", {{"id", "send"}, {"onClick", [send] { send("ping"); }}}, {"send"}),
    });
}

bool flushUntil(Root& root, TextRenderer& r, const std::string& text, std::chrono::milliseconds limit = 3s) {
    return waitFor([&] {
        root.flush();
        return r.toString().find(text) != std::string::npos;
    }, limit);
}

TEST(use_websocket_connects_sends_and_receives) {
    std::atomic<int> opens{0};
    Running server([&](HttpServer& s) {
        s.webSocket("/ws", {
            .onOpen = [&](const WebSocketPtr& w) { ++opens; w->send("hello"); },
            .onMessage = [](const WebSocketPtr& w, const std::string& m) { w->send("got " + m); },
        });
    });
    TextRenderer r;
    Root root(r);
    root.render(h(Chat, {.url = "ws://127.0.0.1:" + std::to_string(server.port()) + "/ws"}));
    CHECK(r.toString().find("connecting") != std::string::npos);
    CHECK(flushUntil(root, r, "open"));
    CHECK(flushUntil(root, r, "[hello]"));
    root.dispatch(*r.findByProp("id", "send"), "onClick");
    CHECK(flushUntil(root, r, "[hello][got ping]"));

    // The server drops it: the hook reconnects.
    for (const auto& w : server.server.webSockets("/ws")) w->close(1001);
    CHECK(waitFor([&] { root.flush(); return opens == 2; }));
    CHECK(flushUntil(root, r, "open"));
    root.render(h("box", {}, {}));  // unmount: closes, no more reconnects
    CHECK(waitFor([&] { return server.server.webSockets("/ws").empty(); }));
}

// The native client must keep reading while the app is sending: here the
// server echoes big messages while the client still sends more. A client
// that held its lock through a blocking write stopped reading, the server
// then stopped reading too, and both waited for ever.
TEST(client_keeps_reading_while_a_big_send_blocks) {
    Running r([](HttpServer& s) {
        s.webSocket("/echo", {.onMessage = [](const WebSocketPtr& w, const std::string& m) { w->send(m); }});
    });
    std::atomic<int> echoed{0};
    auto client = detail::WebSocketClient::connect("ws://127.0.0.1:" + std::to_string(r.port()) + "/echo",
                                                   {.onMessage = [&](const std::string&) { ++echoed; }});
    bool open = false;
    CHECK(waitFor([&] {
        detail::runDueTimers();
        return open = client->send("hi");
    }));
    constexpr int count = 24;  // 24 x 900 KB: much more than the sockets' buffers
    std::atomic<bool> sent{false};
    std::thread sender([&] {
        const std::string big(900 * 1024, 'x');
        for (int i = 0; i < count; ++i) client->send(big);
        sent = true;
    });
    const bool finished = waitFor([&] { return sent.load(); }, 20s);
    CHECK(finished);
    if (!finished) {  // stuck: the old deadlock; don't hang the test run
        std::cerr << "  FAIL client and server wait on each other\n";
        std::_Exit(1);
    }
    sender.join();
    CHECK(waitFor([&] {
        detail::runDueTimers();
        return echoed == count + 1;
    }, 20s));
    client->close();
}

TEST(use_websocket_reports_errors) {
    TextRenderer r;
    Root root(r);
    root.render(h(Chat, {.url = "http://127.0.0.1:1/ws", .reconnect = false}));
    CHECK(flushUntil(root, r, "closed:the terminal build needs a full ws:// URL"));
    root.render(h(Chat, {.url = "ws://127.0.0.1:1/ws", .reconnect = false}));  // nothing listens there
    CHECK(flushUntil(root, r, "closed:can't connect to 127.0.0.1:1"));
}

}  // namespace

int main() {
    for (auto& [name, fn] : tests) {
        std::cout << name << '\n';
        fn();
    }
    std::cout << (failures ? "FAILED: " + std::to_string(failures) : std::string("OK, all tests passed")) << '\n';
    return failures ? 1 : 0;
}
