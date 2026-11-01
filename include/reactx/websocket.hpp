#pragma once

// Live data over a WebSocket: messages from the server as they happen,
// without polling.
//
//   auto chat = useWebSocket("/ws/chat", {
//       .onMessage = [dispatch](const std::string& text) { dispatch({.kind = Action::Message, .text = text}); },
//   });
//   if (!chat.open()) return h("p", {}, {"connecting..."});
//   chat.send(R"({"text":"hi"})");
//
// The handlers run on the UI thread, so they may set state. The connection
// opens when the component mounts (and again when the URL changes), closes
// when it unmounts, and comes back after a drop (reconnect, with a growing
// delay up to 30 s) unless reconnect = false.
//
// Browser: the page's WebSocket; a relative URL ("/ws/chat") is turned into
// ws:// or wss:// on the page's host (behind <base href> too). Terminal
// build: a full ws:// URL ("ws://127.0.0.1:8090/ws/chat"; no wss). Text
// messages only. The server side: HttpServer::webSocket (reactx/http.hpp).

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "reactx/element.hpp"

namespace rx {

enum class WebSocketStatus { Connecting, Open, Closed };

struct WebSocketOptions {
    std::function<void(const std::string& message)> onMessage{};
    Callback onOpen{};
    std::function<void(int code)> onClose{};  // 1000 normal, 1006 dropped, ...
    bool reconnect = true;
    std::chrono::milliseconds retryDelay{1000};  // doubles after each failed try, up to 30 s
};

struct WebSocketState {
    WebSocketStatus status = WebSocketStatus::Connecting;
    std::string error{};  // why the last connection ended ("" while it works)
    std::function<bool(const std::string& text)> send{};  // false unless open
    Callback reconnect{};  // connect again now
    bool open() const { return status == WebSocketStatus::Open; }
};

// A hook: call it at the top of a component.
WebSocketState useWebSocket(std::string url, WebSocketOptions options = {});

namespace detail {

// One connection; the hook's building block. Events arrive on the UI thread
// until close() (after it, none).
class WebSocketClient {
public:
    struct Events {
        std::function<void()> onOpen{};
        std::function<void(const std::string&)> onMessage{};
        std::function<void(int code, const std::string& why)> onClose{};
    };
    static std::shared_ptr<WebSocketClient> connect(const std::string& url, Events events);
    virtual ~WebSocketClient() = default;
    virtual bool send(const std::string& text) = 0;
    virtual void close() = 0;
};

}  // namespace detail

}  // namespace rx
