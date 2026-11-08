#pragma once

// reactx::realtime — live messages in rooms: a chat, a dashboard that updates
// itself, "someone else is editing". The server keeps rooms (Hub, native); the
// app joins one with useChannel (browser and terminal) and gets what is
// published there. Link reactx::realtime.
//
// Server:
//   rx::net::HttpServer server;
//   rx::realtime::Hub hub;
//   hub.attach(server, "/ws");                         // the WebSocket route
//   hub.publish("orders", rx::json::object({{"id", 7}, {"status", "paid"}}));  // from any thread
//
// App:
//   auto [orders, setOrders] = useState(std::vector<rx::json::Value>{});
//   auto channel = rx::realtime::useChannel("/ws", "orders", {
//       .onMessage = [setOrders](const rx::json::Value& data) {
//           setOrders([data](auto old) { old.push_back(data); return old; });
//       },
//   });
//   channel.publish(rx::json::object({{"text", "hi"}}));   // if the Hub lets clients publish
//   channel.members                                         // who is in the room now
//
// Under it: JSON over one WebSocket per channel —
//   to the server: {"op":"join","room":r}  {"op":"leave","room":r}  {"op":"publish","room":r,"data":...}
//   to the app:    {"op":"message","room":r,"data":...}  {"op":"members","room":r,"count":n}
// so a client in another language can talk to a Hub too.

#include <cstddef>
#include <functional>
#include <string>

#include "reactx/hooks.hpp"
#include "reactx/json.hpp"
#include "reactx/websocket.hpp"

#if !defined(__EMSCRIPTEN__)
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include "reactx/http.hpp"
#endif

namespace rx::realtime {

// ---- in the app ----

struct ChannelOptions {
    std::function<void(const json::Value& data)> onMessage{};  // runs on the UI thread: may set state
    std::function<void(int members)> onMembers{};
};

struct Channel {
    bool connected = false;  // the socket is open and the room joined
    int members = 0;         // in the room, as the server last said
    std::string error{};     // why the connection dropped ("" while it works); it comes back by itself
    // Sends `data` to everybody in the room (false while not connected).
    std::function<bool(const json::Value& data)> publish{};
};

// Joins `room` on the Hub at `url` ("/ws" in the browser; a full "ws://host:port/ws"
// in a terminal app), and again after a reconnect. A hook: call it at the top
// of a component; leaving the component leaves the room.
Channel useChannel(std::string url, std::string room, ChannelOptions options = {});

// ---- on the server (native) ----
#if !defined(__EMSCRIPTEN__)

struct HubOptions {
    // May this client join this room? (Read the session from ws->request().)
    std::function<bool(const net::WebSocketPtr& ws, const std::string& room)> canJoin{};
    // May this client publish this message? Without it clients can't publish
    // at all (clientsPublish = false) or always can (true).
    std::function<bool(const net::WebSocketPtr& ws, const std::string& room, const json::Value& data)> canPublish{};
    bool clientsPublish = false;
    bool echo = true;  // the sender gets its own message too
    // Something a client published (after canPublish), before it is sent on.
    std::function<void(const net::WebSocketPtr& ws, const std::string& room, const json::Value& data)> onPublish{};
    std::size_t maxMessageBytes = 64 * 1024;
    std::size_t maxRoomsPerClient = 32;
    std::size_t maxRoomName = 128;
};

// Rooms of WebSocket clients. Thread-safe: publish() from a request handler,
// a timer or a worker thread.
class Hub {
public:
    explicit Hub(HubOptions options = {});
    Hub(const Hub&) = delete;

    // Serves the Hub at `path` (server.webSocket). Keep the Hub alive as long as the server.
    void attach(net::HttpServer& server, std::string path = "/ws");
    // Sends `data` to everybody in `room`; returns how many got it.
    std::size_t publish(const std::string& room, const json::Value& data);
    std::size_t members(const std::string& room) const;
    std::vector<std::string> rooms() const;  // rooms with someone in them

private:
    struct State;
    std::shared_ptr<State> state_;  // shared with the socket handlers
};

#endif

}  // namespace rx::realtime
