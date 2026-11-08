#include "rx/realtime.hpp"

#include <utility>

namespace rx::realtime {

namespace {
std::string frame(const char* op, const std::string& room) {
    return json::object({{"op", op}, {"room", room}}).dump();
}
}  // namespace

Channel useChannel(std::string url, std::string room, ChannelOptions options) {
    auto [members, setMembers] = useState(0);
    auto& latest = useRef(ChannelOptions{});
    latest = std::move(options);

    auto ws = useWebSocket(std::move(url), {
        .onMessage = [room, setMembers, &latest](const std::string& text) {
            auto msg = json::tryParse(text);
            if (!msg || !msg->isObject() || msg->get<std::string>("room") != room) return;
            const std::string op = msg->get<std::string>("op");
            if (op == "message") {
                if (latest.onMessage) latest.onMessage((*msg)["data"]);
            } else if (op == "members") {
                const int n = static_cast<int>(msg->get<double>("count"));
                setMembers(n);
                if (latest.onMembers) latest.onMembers(n);
            }
        },
    });

    // Join when the socket opens (again after a reconnect) or the room changes;
    // leave when the room changes or the component goes away.
    const bool open = ws.open();
    useEffect([open, room, send = ws.send]() -> std::function<void()> {
        if (!open || room.empty()) return {};
        send(frame("join", room));
        return [room, send] { send(frame("leave", room)); };
    }, deps(open, room));

    Channel c;
    c.connected = open;
    c.members = open ? members : 0;
    c.error = ws.error;
    c.publish = [room, send = ws.send, open](const json::Value& data) {
        if (!open) return false;
        return send(json::object({{"op", "publish"}, {"room", room}, {"data", data}}).dump());
    };
    return c;
}

}  // namespace rx::realtime
