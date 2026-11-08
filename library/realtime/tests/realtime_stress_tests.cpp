// Stress tests of reactx::realtime: a real HttpServer with a Hub and real
// WebSocket clients — many clients and rooms, publishing from several
// threads, clients joining and leaving under load, broken and hostile
// messages, dropped clients, and useChannel through a server restart.
// Runs under ThreadSanitizer in CI. `ctest -L stress`.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "reactx/testing.hpp"
#include "reactx/ui.hpp"
#include "rx/realtime.hpp"

using namespace rx;
using namespace std::chrono_literals;

namespace {

// A server with a Hub on its own thread.
struct Server {
    explicit Server(realtime::HubOptions options = {}, int port = 0) : server("127.0.0.1", port), hub(std::move(options)) {
        hub.attach(server, "/ws");
        server.start();
        thread = std::jthread([this] { server.run(); });
    }
    ~Server() { server.stop(); }
    std::string url() const { return "ws://127.0.0.1:" + std::to_string(server.port()) + "/ws"; }
    net::HttpServer server;
    realtime::Hub hub;
    std::jthread thread;
};

// A WebSocket client that speaks the Hub's protocol by hand. Its events come
// on this thread when pump() runs the timers.
struct Client {
    struct State {
        bool open = false, closed = false;
        std::vector<json::Value> messages;  // {"op":"message",...}
        int lastMembers = -1;
    };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::shared_ptr<detail::WebSocketClient> ws;

    explicit Client(const std::string& url) {
        ws = detail::WebSocketClient::connect(url, {
            .onOpen = [s = state] { s->open = true; },
            .onMessage = [s = state](const std::string& text) {
                auto v = json::tryParse(text);
                if (!v) return;
                if (v->get<std::string>("op") == "message") s->messages.push_back(*v);
                else if (v->get<std::string>("op") == "members") s->lastMembers = v->get<int>("count");
            },
            .onClose = [s = state](int, const std::string&) { s->closed = true; },
        });
    }
    void send(const json::Value& v) { ws->send(v.dump()); }
    void join(const std::string& room) { send(json::object({{"op", "join"}, {"room", room}})); }
    void leave(const std::string& room) { send(json::object({{"op", "leave"}, {"room", room}})); }
};

// Runs the clients' events until `done` or the time is up.
template <class F>
bool pump(F done, std::chrono::milliseconds limit = 10s) {
    const auto end = std::chrono::steady_clock::now() + limit;
    while (std::chrono::steady_clock::now() < end) {
        detail::runDueTimers();
        if (done()) return true;
        std::this_thread::sleep_for(2ms);
    }
    detail::runDueTimers();
    return done();
}

std::vector<std::unique_ptr<Client>> connectAll(const Server& s, int n) {
    std::vector<std::unique_ptr<Client>> out;
    for (int i = 0; i < n; ++i) out.push_back(std::make_unique<Client>(s.url()));
    pump([&] { return std::ranges::all_of(out, [](const auto& c) { return c->state->open; }); });
    return out;
}

}  // namespace

RX_TEST(forty_clients_four_rooms_four_publishing_threads) {
    Server s;
    auto clients = connectAll(s, 40);
    CHECK(std::ranges::all_of(clients, [](const auto& c) { return c->state->open; }));
    for (int i = 0; i < 40; ++i) clients[static_cast<std::size_t>(i)]->join("room" + std::to_string(i % 4));
    CHECK(pump([&] { return s.hub.members("room0") == 10 && s.hub.members("room3") == 10; }));

    constexpr int perThread = 100;
    {
        std::vector<std::jthread> publishers;
        for (int t = 0; t < 4; ++t)
            publishers.emplace_back([&s, t] {
                for (int k = 0; k < perThread; ++k)
                    for (int r = 0; r < 4; ++r)
                        s.hub.publish("room" + std::to_string(r), json::object({{"t", t}, {"k", k}, {"room", r}}));
            });
    }
    const std::size_t want = 4 * perThread;
    CHECK(pump([&] { return std::ranges::all_of(clients, [&](const auto& c) { return c->state->messages.size() >= want; }); }, 30s));
    int wrongCount = 0, wrongRoom = 0, outOfOrder = 0;
    for (int i = 0; i < 40; ++i) {
        const auto& got = clients[static_cast<std::size_t>(i)]->state->messages;
        wrongCount += got.size() != want;
        int last[4] = {-1, -1, -1, -1};
        for (const auto& m : got) {
            const auto& d = m["data"];
            wrongRoom += d.get<int>("room") != i % 4;
            const int t = d.get<int>("t"), k = d.get<int>("k");
            outOfOrder += k <= last[t];  // one thread's messages arrive in its order
            last[t] = k;
        }
    }
    CHECK(wrongCount == 0);
    CHECK(wrongRoom == 0);
    CHECK(outOfOrder == 0);
}

RX_TEST(clients_join_and_leave_under_load) {
    Server s;
    auto clients = connectAll(s, 30);
    std::atomic<bool> stop{false};
    std::jthread publisher([&] {
        int k = 0;
        while (!stop) {
            s.hub.publish("r" + std::to_string(k % 6), json::object({{"k", k}}));
            ++k;
            std::this_thread::sleep_for(1ms);  // ~1000 messages a second: the clients (this thread) keep up, even under TSan
        }
    });
    std::mt19937 rng(3);
    std::uniform_int_distribution<int> who(0, 29), room(0, 5), what(0, 1);
    for (int i = 0; i < 2000; ++i) {
        auto& c = *clients[static_cast<std::size_t>(who(rng))];
        const std::string r = "r" + std::to_string(room(rng));
        if (what(rng)) c.join(r);
        else c.leave(r);
        if (i % 20 == 0) detail::runDueTimers();  // take in what arrived
    }
    for (auto& c : clients)
        for (int r = 0; r < 6; ++r) c->leave("r" + std::to_string(r));
    stop = true;
    publisher.join();
    CHECK(pump([&] { return s.hub.rooms().empty(); }));
    CHECK(s.hub.members("r0") == 0);
    // Everyone is still connected and can join again.
    for (auto& c : clients) c->join("final");
    CHECK(pump([&] { return s.hub.members("final") == 30; }));
}

RX_TEST(broken_and_hostile_messages_change_nothing) {
    Server s({.clientsPublish = false, .maxMessageBytes = 1024, .maxRoomsPerClient = 8, .maxRoomName = 16});
    auto clients = connectAll(s, 3);
    auto& bad = *clients[0];
    auto& listener = *clients[1];
    listener.join("news");
    CHECK(pump([&] { return s.hub.members("news") == 1; }));

    for (std::string junk : {std::string("not json"), std::string("[1,2,3]"), std::string("42"), std::string("{}"),
                             std::string(R"({"op":"explode","room":"news"})"), std::string(R"({"op":"join"})"),
                             std::string(R"({"op":"join","room":42})"), std::string(R"({"op":"join","room":""})"),
                             std::string("{\"op\":\"join\",\"room\":\"") + std::string(5000, 'x') + "\"}",
                             std::string(R"({"op":"publish","room":"news","data":"not a member"})")})
        bad.ws->send(junk);
    // Too long a room name, too many rooms, too big a message, publishing when only the server may.
    bad.join(std::string(17, 'r'));
    for (int i = 0; i < 20; ++i) bad.join("room" + std::to_string(i));
    bad.join("news");
    bad.send(json::object({{"op", "publish"}, {"room", "news"}, {"data", std::string(4000, 'z')}}));
    bad.send(json::object({{"op", "publish"}, {"room", "news"}, {"data", "hi"}}));
    std::this_thread::sleep_for(200ms);
    pump([] { return false; }, 300ms);
    CHECK(listener.state->messages.empty());  // nothing got through
    CHECK(s.hub.members(std::string(17, 'r')) == 0);
    // The bad client got its first 8 rooms (room0..room7) and no more — not "news" either.
    CHECK(s.hub.rooms().size() == 9);
    CHECK(s.hub.members("room7") == 1);
    CHECK(s.hub.members("room8") == 0);
    CHECK(s.hub.members("news") == 1);
    // The server still works for everybody.
    CHECK(s.hub.publish("news", json::object({{"ok", true}})) == 1);
    CHECK(pump([&] { return listener.state->messages.size() == 1; }));
}

RX_TEST(dropped_clients_leave_their_rooms) {
    Server s;
    {
        auto clients = connectAll(s, 50);
        for (auto& c : clients) {
            c->join("a");
            c->join("b");
        }
        CHECK(pump([&] { return s.hub.members("a") == 50 && s.hub.members("b") == 50; }));
        for (std::size_t i = 0; i < clients.size(); i += 2) clients[i]->ws->close();
        CHECK(pump([&] { return s.hub.members("a") == 25; }));
    }  // the rest go away without a word
    CHECK(pump([&] { return s.hub.rooms().empty(); }));
}

namespace {
int serverPort = 0;
struct ChatProps {
    int n = 0;
};
Element Chat(const ChatProps& p) {
    auto [seen, setSeen] = useState(0);
    auto ch = realtime::useChannel("ws://127.0.0.1:" + std::to_string(serverPort) + "/ws", "chat", {
        .onMessage = [setSeen](const json::Value&) { setSeen([](int n) { return n + 1; }); },
    });
    return h("box", {}, {ui::text({.text = "#" + std::to_string(p.n) + (ch.connected ? " on " : " off ") +
                                           std::to_string(ch.members) + " seen " + std::to_string(seen)}),
                         ui::button({.label = "send" + std::to_string(p.n),
                                     .onClick = [publish = ch.publish] { publish(json::object({{"x", 1}})); }})});
}
}  // namespace

RX_TEST(eight_channels_publish_and_come_back_after_a_restart) {
    auto server = std::make_unique<Server>(realtime::HubOptions{.clientsPublish = true});
    serverPort = server->server.port();
    std::vector<std::unique_ptr<testing::Screen>> screens;
    for (int i = 0; i < 8; ++i) screens.push_back(std::make_unique<testing::Screen>(h(Chat, {.n = i}), 60, 4));
    auto settleAll = [&] {
        for (auto& s : screens) s->settle(1ms);
    };
    auto all = [&](const std::string& part) {
        return std::ranges::all_of(screens, [&](const auto& s) { return s->shows(part); });
    };
    CHECK(pump([&] { settleAll(); return all(" on 8 "); }, 15s));
    for (int i = 0; i < 8; ++i)
        for (int k = 0; k < 25; ++k) screens[static_cast<std::size_t>(i)]->click("send" + std::to_string(i));
    CHECK(pump([&] { settleAll(); return all("seen 200"); }, 20s));

    // The server goes away and comes back on the same port: every channel reconnects and joins again.
    server.reset();
    CHECK(pump([&] { settleAll(); return all(" off "); }, 15s));
    server = std::make_unique<Server>(realtime::HubOptions{.clientsPublish = true}, serverPort);
    CHECK(pump([&] { settleAll(); return all(" on 8 "); }, 30s));
    CHECK(server->hub.members("chat") == 8);
    screens.clear();  // unmounting leaves the room
    CHECK(pump([&] { return server->hub.members("chat") == 0; }));
}
