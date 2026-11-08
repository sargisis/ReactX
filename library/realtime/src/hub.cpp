#include "rx/realtime.hpp"

#include <algorithm>
#include <utility>

namespace rx::realtime {

struct Hub::State {
    HubOptions options;
    mutable std::mutex mutex;
    std::map<std::string, std::map<int, net::WebSocketPtr>, std::less<>> rooms;  // room -> socket id -> socket
    std::map<int, std::set<std::string>> joined;                                 // socket id -> its rooms

    // Under the lock: the members of a room, to send to after unlocking.
    std::vector<net::WebSocketPtr> membersOf(const std::string& room) const {
        std::vector<net::WebSocketPtr> out;
        if (auto it = rooms.find(room); it != rooms.end())
            for (const auto& [id, ws] : it->second) out.push_back(ws);
        return out;
    }

    void sendAll(const std::vector<net::WebSocketPtr>& to, const std::string& text, int except = -1) {
        for (const auto& ws : to)
            if (ws->id() != except) ws->send(text);
    }

    void announce(const std::string& room) {
        std::vector<net::WebSocketPtr> to;
        {
            std::lock_guard lock(mutex);
            to = membersOf(room);
        }
        const std::string text =
            json::object({{"op", "members"}, {"room", room}, {"count", static_cast<double>(to.size())}}).dump();
        sendAll(to, text);
    }

    void join(const net::WebSocketPtr& ws, const std::string& room) {
        if (room.empty() || room.size() > options.maxRoomName) return;
        if (options.canJoin && !options.canJoin(ws, room)) return;
        {
            std::lock_guard lock(mutex);
            auto& mine = joined[ws->id()];
            if (mine.contains(room) || mine.size() >= options.maxRoomsPerClient) return;
            mine.insert(room);
            rooms[room][ws->id()] = ws;
        }
        announce(room);
    }

    void leave(int id, const std::string& room) {
        {
            std::lock_guard lock(mutex);
            auto mine = joined.find(id);
            if (mine == joined.end() || !mine->second.erase(room)) return;
            if (auto it = rooms.find(room); it != rooms.end()) {
                it->second.erase(id);
                if (it->second.empty()) rooms.erase(it);
            }
        }
        announce(room);
    }

    void leaveAll(int id) {
        std::set<std::string> was;
        {
            std::lock_guard lock(mutex);
            if (auto mine = joined.find(id); mine != joined.end()) {
                was = std::move(mine->second);
                joined.erase(mine);
            }
            for (const auto& room : was)
                if (auto it = rooms.find(room); it != rooms.end()) {
                    it->second.erase(id);
                    if (it->second.empty()) rooms.erase(it);
                }
        }
        for (const auto& room : was) announce(room);
    }

    void fromClient(const net::WebSocketPtr& ws, const std::string& text) {
        if (text.size() > options.maxMessageBytes) return;
        auto msg = json::tryParse(text);
        if (!msg || !msg->isObject()) return;
        const std::string op = msg->get<std::string>("op");
        const std::string room = msg->get<std::string>("room");
        if (op == "join") return join(ws, room);
        if (op == "leave") return leave(ws->id(), room);
        if (op != "publish") return;
        {
            std::lock_guard lock(mutex);
            auto mine = joined.find(ws->id());
            if (mine == joined.end() || !mine->second.contains(room)) return;  // only into rooms it is in
        }
        const json::Value& data = (*msg)["data"];
        const bool allowed = options.canPublish ? options.canPublish(ws, room, data) : options.clientsPublish;
        if (!allowed) return;
        if (options.onPublish) options.onPublish(ws, room, data);
        std::vector<net::WebSocketPtr> to;
        {
            std::lock_guard lock(mutex);
            to = membersOf(room);
        }
        sendAll(to, json::object({{"op", "message"}, {"room", room}, {"data", data}}).dump(),
                options.echo ? -1 : ws->id());
    }
};

Hub::Hub(HubOptions options) : state_(std::make_shared<State>()) { state_->options = std::move(options); }

void Hub::attach(net::HttpServer& server, std::string path) {
    std::weak_ptr<State> weak = state_;
    server.webSocket(std::move(path), {
        .onMessage = [weak](const net::WebSocketPtr& ws, const std::string& text) {
            if (auto s = weak.lock()) s->fromClient(ws, text);
        },
        .onClose = [weak](const net::WebSocketPtr& ws, int) {
            if (auto s = weak.lock()) s->leaveAll(ws->id());
        },
    });
}

std::size_t Hub::publish(const std::string& room, const json::Value& data) {
    std::vector<net::WebSocketPtr> to;
    {
        std::lock_guard lock(state_->mutex);
        to = state_->membersOf(room);
    }
    const std::string text = json::object({{"op", "message"}, {"room", room}, {"data", data}}).dump();
    std::size_t sent = 0;
    for (const auto& ws : to) sent += ws->send(text) ? 1 : 0;
    return sent;
}

std::size_t Hub::members(const std::string& room) const {
    std::lock_guard lock(state_->mutex);
    auto it = state_->rooms.find(room);
    return it == state_->rooms.end() ? 0 : it->second.size();
}

std::vector<std::string> Hub::rooms() const {
    std::lock_guard lock(state_->mutex);
    std::vector<std::string> out;
    for (const auto& [room, members] : state_->rooms) out.push_back(room);
    return out;
}

}  // namespace rx::realtime
