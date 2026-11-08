# reactx::realtime

Live messages in rooms for [ReactX](https://github.com/sargisis/ReactX) apps: a chat, a dashboard that updates itself, "3 people are looking at this order". The server keeps the rooms (`Hub`, native); the app joins one with `useChannel` — in the browser and in a terminal — and gets what is published there.

На русском: [README.ru.md](README.ru.md).

## Server

```cpp
#include "rx/realtime.hpp"
using namespace rx;

net::HttpServer server("0.0.0.0", 8090);
realtime::Hub hub({
    .canJoin = [](const net::WebSocketPtr& ws, const std::string& room) {
        return room == "lobby" || signedIn(ws->request());   // your own check
    },
    .clientsPublish = true,                                    // clients may send to their rooms
});
hub.attach(server, "/ws");

// From a request handler, a timer or another thread:
hub.publish("orders", json::object({{"id", 7}, {"status", "paid"}}));
hub.members("lobby");   // how many are in a room
```

## App

```cpp
Element Chat(const Props&) {
    auto [lines, setLines] = useState(std::vector<std::string>{});
    auto channel = realtime::useChannel("/ws", "lobby", {
        .onMessage = [setLines](const json::Value& data) {
            setLines([data](auto old) { old.push_back(data.get<std::string>("text")); return old; });
        },
    });
    // channel.connected, channel.members, channel.error
    // channel.publish(json::object({{"text", "hi"}}));
    ...
}
```

`useChannel(url, room)` joins the room when the socket opens, again after a reconnect (it comes back by itself), and leaves it when the component goes away or `room` changes. In the browser the URL can be relative (`"/ws"`); a terminal app gives the full one (`"ws://127.0.0.1:8090/ws"`). Each channel is one WebSocket.

## Add it to a project

It comes with ReactX (since 1.7). Link it in the project's `CMakeLists.txt` (the app and the server):

```cmake
target_link_libraries(my_app PRIVATE reactx::realtime)
```

## Hub options

| Field | |
|---|---|
| `canJoin(ws, room)` | may this client join? (read the session from `ws->request()`) |
| `canPublish(ws, room, data)` | may this client publish this message? Without it: `clientsPublish` |
| `clientsPublish` | `false`: only the server publishes (`hub.publish`) |
| `echo` | the sender gets its own message too (`true`) |
| `onPublish(ws, room, data)` | a client published (after the checks), before it is sent on: save it, log it |
| `maxMessageBytes`, `maxRoomsPerClient`, `maxRoomName` | 64 KB, 32, 128 |

A client publishes only into rooms it has joined. The Hub is thread-safe.

## The protocol

JSON text over the WebSocket, so a client in another language can talk to a Hub:

- to the server: `{"op":"join","room":"lobby"}`, `{"op":"leave","room":"lobby"}`, `{"op":"publish","room":"lobby","data":{...}}`
- to the app: `{"op":"message","room":"lobby","data":{...}}`, `{"op":"members","room":"lobby","count":3}`
