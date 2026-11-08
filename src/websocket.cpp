// useWebSocket and its two transports: the page's WebSocket (browser) and a
// small RFC 6455 client on a thread of its own (terminal build).
#include "reactx/websocket.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "reactx/hooks.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "reactx/dom.hpp"
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <variant>

#include "reactx/parse.hpp"
#include "ws_frame.hpp"
#endif

namespace rx {

namespace detail {

namespace {

using Events = WebSocketClient::Events;

#ifdef __EMSCRIPTEN__

// ---- browser: the page's WebSocket ----

EM_JS(void, rx_ws_open, (int id, const char* url), {
    const sockets = Module.rxSockets || (Module.rxSockets = {});
    let u;
    try {
        u = new URL(UTF8ToString(url), document.baseURI);
        if (u.protocol === "http:") u.protocol = "ws:";
        else if (u.protocol === "https:") u.protocol = "wss:";
    } catch (e) {
        setTimeout(function() { Module.ccall("rx_ws_event", null, ["number", "number", "number", "string"], [id, 2, 1006, "bad URL"]); }, 0);
        return;
    }
    let ws;
    try {
        ws = new WebSocket(u.href);
    } catch (e) {
        const why = String((e && e.message) || e);
        setTimeout(function() { Module.ccall("rx_ws_event", null, ["number", "number", "number", "string"], [id, 2, 1006, why]); }, 0);
        return;
    }
    sockets[id] = ws;
    const event = function(kind, code, text) {
        if (sockets[id] !== ws) return;  // closed from C++ already
        if (kind === 2) delete sockets[id];
        Module.ccall("rx_ws_event", null, ["number", "number", "number", "string"], [id, kind, code, text]);
    };
    ws.onopen = function() { event(0, 0, ""); };
    ws.onmessage = function(e) { if (typeof e.data === "string") event(1, 0, e.data); };
    ws.onclose = function(e) { event(2, e.code || 1006, e.reason || ""); };
});

EM_JS(int, rx_ws_send, (int id, const char* text), {
    const ws = (Module.rxSockets || {})[id];
    if (!ws || ws.readyState !== 1) return 0;
    ws.send(UTF8ToString(text));
    return 1;
});

EM_JS(void, rx_ws_close, (int id), {
    const sockets = Module.rxSockets || {};
    const ws = sockets[id];
    delete sockets[id];
    if (ws && ws.readyState < 2) ws.close(1000);
});

class BrowserSocket;
std::map<int, BrowserSocket*>& browserSockets() {
    static std::map<int, BrowserSocket*> map;
    return map;
}

class BrowserSocket : public WebSocketClient {
public:
    BrowserSocket(int id, Events events) : id_(id), events_(std::move(events)) { browserSockets()[id_] = this; }
    ~BrowserSocket() override { close(); }
    bool send(const std::string& text) override { return !closed_ && rx_ws_send(id_, text.c_str()) != 0; }
    void close() override {
        if (closed_) return;
        closed_ = true;
        browserSockets().erase(id_);
        rx_ws_close(id_);
    }
    void event(int kind, int code, const std::string& text) {
        if (closed_) return;
        if (kind == 0 && events_.onOpen) events_.onOpen();
        else if (kind == 1 && events_.onMessage) events_.onMessage(text);
        else if (kind == 2) {
            closed_ = true;
            browserSockets().erase(id_);
            if (events_.onClose) events_.onClose(code, text.empty() ? "closed (" + std::to_string(code) + ")" : text);
        }
    }

private:
    int id_;
    Events events_;
    bool closed_ = false;
};

#else

// ---- terminal: a client on its own thread ----

bool sendAll(int fd, std::string_view data) {
    while (!data.empty()) {
        ssize_t n = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

struct Target {
    std::string host;
    int port = 80;
    std::string path;  // with the query
};

std::optional<Target> parseWsUrl(std::string_view url, std::string& why) {
    if (!url.starts_with("ws://")) {
        why = url.starts_with("wss://") ? "wss:// isn't supported in the terminal build; use ws://"
                                        : "the terminal build needs a full ws:// URL, got \"" + std::string(url) + "\"";
        return std::nullopt;
    }
    url.remove_prefix(5);
    auto slash = url.find('/');
    std::string_view hostPort = url.substr(0, slash);
    Target t{.host = std::string(hostPort), .port = 80, .path = slash == std::string_view::npos ? "/" : std::string(url.substr(slash))};
    if (auto colon = hostPort.rfind(':'); colon != std::string_view::npos) {
        auto port = parsePort(hostPort.substr(colon + 1));
        if (!port) {
            why = "bad port in " + std::string(hostPort);
            return std::nullopt;
        }
        t.host = std::string(hostPort.substr(0, colon));
        t.port = *port;
    }
    return t;
}

int connectTo(const Target& t, std::string& why) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    std::string host = t.host == "localhost" ? "127.0.0.1" : t.host;
    if (int rc = ::getaddrinfo(host.c_str(), std::to_string(t.port).c_str(), &hints, &found); rc != 0 || !found) {
        why = "can't resolve " + t.host + " (" + ::gai_strerror(rc) + ")";
        return -1;
    }
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    timeval timeout{.tv_sec = 10, .tv_usec = 0};
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);  // connect() too
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);  // the handshake
    if (fd < 0 || ::connect(fd, found->ai_addr, found->ai_addrlen) != 0) {
        why = "can't connect to " + t.host + ":" + std::to_string(t.port) + " (" + std::strerror(errno) + ")";
        if (fd >= 0) ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(found);
    return fd;
}

// What the socket thread hands to the UI thread.
struct Opened {};
struct Message {
    std::string text;
};
struct Closed {
    int code;
    std::string why;
};
using Incoming = std::variant<Opened, Message, Closed>;

// Shared by the UI side and the socket thread.
struct Shared {
    std::mutex mutex;  // guards everything below; never held while writing to the socket
    int fd = -1;
    std::deque<Incoming> inbox;
    bool open = false;
    bool closing = false;  // close() was called: the thread stops soon
    // One writer at a time, and the fd stays open while it is held. Taken
    // before `mutex`. The socket thread only try-locks it: it must keep
    // reading, or both ends can wait on each other's full buffers (the app
    // blocked in send() while the server, blocked sending to us, stops reading).
    std::mutex writeMutex;
};

// Writes a whole frame. wait = false (the socket thread): give up at once if
// another write is going on or the socket can't take the start of it now.
bool writeFrame(Shared& s, std::string_view frame, bool wait) {
    std::unique_lock w(s.writeMutex, std::defer_lock);
    if (wait) w.lock();
    else if (!w.try_lock()) return false;
    int fd = -1;
    {
        std::scoped_lock lock(s.mutex);
        fd = s.fd;
    }
    if (fd < 0) return false;
    if (wait) return sendAll(fd, frame);
    ssize_t n = ::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
    if (n <= 0) return false;  // try again later: nothing of the frame went out
    return sendAll(fd, frame.substr(static_cast<std::size_t>(n)));  // a rare partial write: finish the frame
}

void runSocket(std::shared_ptr<Shared> s, Target t) {
    auto finish = [&](int code, std::string why) {
        {
            std::scoped_lock lock(s->mutex);
            if (s->fd >= 0) ::shutdown(s->fd, SHUT_RDWR);  // a write blocked in send() returns now
        }
        std::scoped_lock lock(s->writeMutex, s->mutex);
        if (s->fd >= 0) {
            ::close(s->fd);
            s->fd = -1;
        }
        s->open = false;
        s->inbox.push_back(Closed{code, std::move(why)});
    };
    std::string why;
    int fd = connectTo(t, why);
    if (fd < 0) return finish(1006, why);
    {
        std::scoped_lock lock(s->mutex);
        if (s->closing) {
            ::close(fd);
            return;
        }
        s->fd = fd;
    }
    std::string key = ws::newKey();
    std::string hostHeader = t.host + (t.port == 80 ? "" : ":" + std::to_string(t.port));
    if (!sendAll(fd, "GET " + t.path + " HTTP/1.1\r\nHost: " + hostHeader +
                         "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key +
                         "\r\nSec-WebSocket-Version: 13\r\n\r\n"))
        return finish(1006, "can't send the handshake");
    std::string buffer;
    char buf[16384];
    std::size_t headEnd;
    while ((headEnd = buffer.find("\r\n\r\n")) == std::string::npos) {
        ssize_t n = ::recv(fd, buf, sizeof buf, 0);
        if (n <= 0 || buffer.size() > 64 * 1024) return finish(1006, "no WebSocket handshake from " + hostHeader);
        buffer.append(buf, static_cast<std::size_t>(n));
    }
    std::string head = buffer.substr(0, headEnd);
    if (!head.starts_with("HTTP/1.1 101") || head.find(ws::acceptKey(key)) == std::string::npos)
        return finish(1006, "not a WebSocket at " + t.path + ": " + head.substr(0, head.find("\r\n")));
    buffer.erase(0, headEnd + 4);
    timeval blocking{};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &blocking, sizeof blocking);
    {
        std::scoped_lock lock(s->mutex);
        s->open = true;
        s->inbox.push_back(Opened{});
    }

    ws::Reader reader;
    int code = 1006;
    std::string reason = "connection dropped";
    auto now = [] { return std::chrono::steady_clock::now(); };
    std::optional<std::chrono::steady_clock::time_point> closingSince;
    std::optional<std::string> pendingPong;  // a pong that couldn't go out yet
    for (bool done = false; !done;) {
        int error = 0;
        while (auto frame = ws::take(buffer, /*expectMasked=*/false, error)) {
            auto event = reader.feed(std::move(*frame));
            using Kind = ws::Reader::Event::Kind;
            if (event.kind == Kind::Message) {
                std::scoped_lock lock(s->mutex);
                if (!s->closing) s->inbox.push_back(Message{std::move(event.data)});
            } else if (event.kind == Kind::Ping) {
                std::string pong = ws::encode(ws::Pong, event.data, true);
                if (!writeFrame(*s, pong, /*wait=*/false)) pendingPong = std::move(pong);
            } else if (event.kind == Kind::Close) {
                code = event.code == 1005 ? 1000 : event.code;
                reason = event.data.empty() ? "closed (" + std::to_string(code) + ")" : event.data;
                bool answer = false;
                {
                    std::scoped_lock lock(s->mutex);
                    answer = !s->closing;
                }
                if (answer) writeFrame(*s, ws::encode(ws::Close, ws::closePayload(code), true), /*wait=*/false);
                done = true;
                break;
            } else if (event.kind == Kind::Error) {
                error = event.code;
                break;
            }
        }
        if (error) {
            writeFrame(*s, ws::encode(ws::Close, ws::closePayload(error), true), /*wait=*/false);
            code = error;
            reason = "protocol error";
            break;
        }
        if (done) break;
        {
            std::scoped_lock lock(s->mutex);
            if (s->closing && !closingSince) closingSince = now();
        }
        if (closingSince && now() - *closingSince > std::chrono::seconds(1)) break;  // the server didn't answer the close
        if (pendingPong && writeFrame(*s, *pendingPong, /*wait=*/false)) pendingPong.reset();
        pollfd p{.fd = fd, .events = POLLIN, .revents = 0};
        int ready = ::poll(&p, 1, 100);
        if (ready == 0 || (ready < 0 && errno == EINTR)) continue;
        ssize_t n = ::recv(fd, buf, sizeof buf, 0);
        if (n <= 0) break;
        buffer.append(buf, static_cast<std::size_t>(n));
    }
    finish(code, reason);
}

class NativeSocket : public WebSocketClient, public std::enable_shared_from_this<NativeSocket> {
public:
    NativeSocket(Events events) : events_(std::move(events)), shared_(std::make_shared<Shared>()) {}
    ~NativeSocket() override { close(); }

    void start(const std::string& url) {
        std::string why;
        auto target = parseWsUrl(url, why);
        if (!target) {
            std::scoped_lock lock(shared_->mutex);
            shared_->inbox.push_back(Closed{1006, why});
        } else {
            std::thread(runSocket, shared_, *target).detach();
        }
        poll();
    }

    bool send(const std::string& text) override {
        {
            std::scoped_lock lock(shared_->mutex);
            if (!shared_->open || shared_->closing || shared_->fd < 0) return false;
        }
        // Not under `mutex`: a slow server mustn't stop the socket thread from reading.
        return writeFrame(*shared_, ws::encode(ws::Text, text, /*mask=*/true), /*wait=*/true);
    }

    void close() override {
        if (closed_) return;
        closed_ = true;
        if (stopPoll_) stopPoll_();
        bool wasOpen = false;
        {
            std::scoped_lock lock(shared_->mutex);
            if (shared_->closing) return;
            shared_->closing = true;
            wasOpen = shared_->open && shared_->fd >= 0;
        }
        if (wasOpen) writeFrame(*shared_, ws::encode(ws::Close, ws::closePayload(1000), true), /*wait=*/true);
    }

private:
    // On the UI thread: hand over what arrived, then look again soon.
    void poll() {
        std::weak_ptr<NativeSocket> weak = weak_from_this();
        stopPoll_ = setTimer(std::chrono::milliseconds(15), [weak] {
            if (auto self = weak.lock()) self->deliver();
        });
    }

    void deliver() {
        std::deque<Incoming> batch;
        {
            std::scoped_lock lock(shared_->mutex);
            batch.swap(shared_->inbox);
        }
        bool ended = false;
        for (auto& in : batch) {
            if (closed_) return;
            if (std::holds_alternative<Opened>(in)) {
                if (events_.onOpen) events_.onOpen();
            } else if (auto* m = std::get_if<Message>(&in)) {
                if (events_.onMessage) events_.onMessage(m->text);
            } else {
                auto& c = std::get<Closed>(in);
                closed_ = true;
                ended = true;
                if (events_.onClose) events_.onClose(c.code, c.why);
            }
        }
        if (!ended && !closed_) poll();
    }

    Events events_;
    std::shared_ptr<Shared> shared_;
    Cleanup stopPoll_{};
    bool closed_ = false;
};

#endif

}  // namespace

std::shared_ptr<WebSocketClient> WebSocketClient::connect(const std::string& url, Events events) {
#ifdef __EMSCRIPTEN__
    static int nextId = 0;
    int id = ++nextId;
    auto socket = std::make_shared<BrowserSocket>(id, std::move(events));
    rx_ws_open(id, url.c_str());
    return socket;
#else
    auto socket = std::make_shared<NativeSocket>(std::move(events));
    socket->start(url);
    return socket;
#endif
}

}  // namespace detail

WebSocketState useWebSocket(std::string url, WebSocketOptions options) {
    auto [status, setStatus] = useState(WebSocketStatus::Connecting);
    auto [error, setError] = useState(std::string{});
    auto [attempt, setAttempt] = useState(0);
    // The newest handlers: they see this render's state.
    auto& latest = useRef(WebSocketOptions{});
    latest = std::move(options);
    auto& client = useRef(std::shared_ptr<detail::WebSocketClient>{});
    auto& failures = useRef(0);

    useEffect([url, attempt, setStatus, setError, setAttempt, &latest, &client, &failures] {
        (void)attempt;
        setStatus(WebSocketStatus::Connecting);
        auto retry = std::make_shared<Cleanup>();  // the pending reconnect timer
        auto socket = detail::WebSocketClient::connect(url, {
            .onOpen = [setStatus, setError, &latest, &failures] {
                failures = 0;
                setStatus(WebSocketStatus::Open);
                setError(std::string{});
                if (latest.onOpen) latest.onOpen();
            },
            .onMessage = [&latest](const std::string& text) {
                if (latest.onMessage) latest.onMessage(text);
            },
            .onClose = [setStatus, setError, setAttempt, retry, &latest, &failures](int code, const std::string& why) {
                setStatus(WebSocketStatus::Closed);
                setError(why);
                if (latest.onClose) latest.onClose(code);
                if (!latest.reconnect) return;
                auto delay = latest.retryDelay * (1 << std::min(failures, 5));
                delay = std::min<std::chrono::milliseconds>(delay, std::chrono::seconds(30));
                ++failures;
                *retry = detail::setTimer(delay, [setAttempt] { setAttempt([](int n) { return n + 1; }); });
            },
        });
        client = socket;
        return [socket, retry, &client] {
            if (*retry) (*retry)();
            socket->close();
            if (client == socket) client.reset();
        };
    }, deps(url, attempt));

    return WebSocketState{
        .status = status,
        .error = error,
        .send = [&client](const std::string& text) { return client && client->send(text); },
        .reconnect = [setAttempt] { setAttempt([](int n) { return n + 1; }); },
    };
}

}  // namespace rx

#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void rx_ws_event(int id, int kind, int code, const char* text) {
    auto& map = rx::detail::browserSockets();
    auto it = map.find(id);
    if (it == map.end()) return;
    it->second->event(kind, code, text);
    rx::flushDom();  // the message arrived outside of a DOM event
}
#endif
