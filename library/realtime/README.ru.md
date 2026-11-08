# reactx::realtime

Живые сообщения в комнатах для приложений на [ReactX](https://github.com/sargisis/ReactX): чат, дашборд, который обновляется сам, «этот заказ смотрят ещё 3 человека». Комнаты держит сервер (`Hub`, нативно); приложение входит в комнату через `useChannel` — в браузере и в терминале — и получает всё, что туда публикуют.

In English: [README.md](README.md).

## Сервер

```cpp
#include "rx/realtime.hpp"
using namespace rx;

net::HttpServer server("0.0.0.0", 8090);
realtime::Hub hub({
    .canJoin = [](const net::WebSocketPtr& ws, const std::string& room) {
        return room == "lobby" || signedIn(ws->request());   // ваша проверка
    },
    .clientsPublish = true,                                    // клиенты могут писать в свои комнаты
});
hub.attach(server, "/ws");

// Из обработчика запроса, таймера или другого потока:
hub.publish("orders", json::object({{"id", 7}, {"status", "paid"}}));
hub.members("lobby");   // сколько человек в комнате
```

## Приложение

```cpp
Element Chat(const Props&) {
    auto [lines, setLines] = useState(std::vector<std::string>{});
    auto channel = realtime::useChannel("/ws", "lobby", {
        .onMessage = [setLines](const json::Value& data) {
            setLines([data](auto old) { old.push_back(data.get<std::string>("text")); return old; });
        },
    });
    // channel.connected, channel.members, channel.error
    // channel.publish(json::object({{"text", "привет"}}));
    ...
}
```

`useChannel(url, room)` входит в комнату, когда сокет открылся, снова — после переподключения (оно происходит само), и выходит, когда компонент исчезает или меняется `room`. В браузере адрес может быть относительным (`"/ws"`); терминальное приложение даёт полный (`"ws://127.0.0.1:8090/ws"`). Один канал — один WebSocket.

## Подключение

Идёт вместе с ReactX (с 1.7). В `CMakeLists.txt` проекта (и приложения, и сервера):

```cmake
target_link_libraries(my_app PRIVATE reactx::realtime)
```

## Опции Hub

| Поле | |
|---|---|
| `canJoin(ws, room)` | можно ли этому клиенту войти? (сессию берите из `ws->request()`) |
| `canPublish(ws, room, data)` | можно ли ему опубликовать это сообщение? Без неё — `clientsPublish` |
| `clientsPublish` | `false`: публикует только сервер (`hub.publish`) |
| `echo` | отправитель тоже получает своё сообщение (`true`) |
| `onPublish(ws, room, data)` | клиент опубликовал (после проверок), до рассылки: сохраните, запишите в лог |
| `maxMessageBytes`, `maxRoomsPerClient`, `maxRoomName` | 64 КБ, 32, 128 |

Клиент публикует только в комнаты, в которые вошёл. Hub потокобезопасен.

## Протокол

JSON-текст через WebSocket, так что клиент на другом языке тоже может говорить с Hub:

- серверу: `{"op":"join","room":"lobby"}`, `{"op":"leave","room":"lobby"}`, `{"op":"publish","room":"lobby","data":{...}}`
- приложению: `{"op":"message","room":"lobby","data":{...}}`, `{"op":"members","room":"lobby","count":3}`
