// WebSocket wire format (RFC 6455), shared by the server (http.cpp) and the
// native client (websocket.cpp): the handshake key, frames in and out.
#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace rx::detail::ws {

enum Opcode : std::uint8_t { Continuation = 0x0, Text = 0x1, Binary = 0x2, Close = 0x8, Ping = 0x9, Pong = 0xA };

// Messages bigger than this close the connection (1009, "message too big").
inline constexpr std::size_t kMaxMessage = 1 << 20;

// SHA-1 of `data` (only for the handshake: RFC 6455 asks for it).
inline std::array<std::uint8_t, 20> sha1(std::string_view data) {
    std::uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::string msg(data);
    std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8;
    msg += static_cast<char>(0x80);
    while (msg.size() % 64 != 56) msg += '\0';
    for (int i = 7; i >= 0; --i) msg += static_cast<char>((bits >> (i * 8)) & 0xFF);
    for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            auto b = [&](int k) { return static_cast<std::uint32_t>(static_cast<unsigned char>(msg[chunk + i * 4 + k])); };
            w[i] = (b(0) << 24) | (b(1) << 16) | (b(2) << 8) | b(3);
        }
        for (int i = 16; i < 80; ++i) w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
            else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
            else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
            else f = b ^ c ^ d, k = 0xCA62C1D6;
            std::uint32_t t = std::rotl(a, 5) + f + e + k + w[i];
            e = d, d = c, c = std::rotl(b, 30), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    std::array<std::uint8_t, 20> out{};
    for (int i = 0; i < 20; ++i) out[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(h[i / 4] >> (24 - (i % 4) * 8));
    return out;
}

inline std::string base64(std::string_view bytes) {
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        std::uint32_t n = static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i])) << 16;
        if (i + 1 < bytes.size()) n |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i + 1])) << 8;
        if (i + 2 < bytes.size()) n |= static_cast<unsigned char>(bytes[i + 2]);
        out += kAlphabet[(n >> 18) & 63];
        out += kAlphabet[(n >> 12) & 63];
        out += i + 1 < bytes.size() ? kAlphabet[(n >> 6) & 63] : '=';
        out += i + 2 < bytes.size() ? kAlphabet[n & 63] : '=';
    }
    return out;
}

// Sec-WebSocket-Accept for a Sec-WebSocket-Key.
inline std::string acceptKey(std::string_view key) {
    auto digest = sha1(std::string(key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    return base64(std::string_view(reinterpret_cast<const char*>(digest.data()), digest.size()));
}

inline std::uint32_t randomMask() {
    static thread_local std::mt19937 gen{std::random_device{}()};
    return gen();
}

// A random Sec-WebSocket-Key (16 bytes, base64).
inline std::string newKey() {
    std::string raw;
    for (int i = 0; i < 4; ++i) {
        std::uint32_t r = randomMask();
        for (int k = 0; k < 4; ++k) raw += static_cast<char>((r >> (k * 8)) & 0xFF);
    }
    return base64(raw);
}

// One frame. Clients must mask what they send; servers must not.
inline std::string encode(Opcode op, std::string_view payload, bool mask) {
    std::string out;
    out += static_cast<char>(0x80 | op);  // FIN: messages are never split
    std::uint8_t maskBit = mask ? 0x80 : 0;
    if (payload.size() < 126) {
        out += static_cast<char>(maskBit | payload.size());
    } else if (payload.size() <= 0xFFFF) {
        out += static_cast<char>(maskBit | 126);
        out += static_cast<char>((payload.size() >> 8) & 0xFF);
        out += static_cast<char>(payload.size() & 0xFF);
    } else {
        out += static_cast<char>(maskBit | 127);
        for (int i = 7; i >= 0; --i) out += static_cast<char>((static_cast<std::uint64_t>(payload.size()) >> (i * 8)) & 0xFF);
    }
    if (!mask) return out.append(payload);
    std::uint32_t key = randomMask();
    char k[4] = {static_cast<char>(key), static_cast<char>(key >> 8), static_cast<char>(key >> 16), static_cast<char>(key >> 24)};
    out.append(k, 4);
    for (std::size_t i = 0; i < payload.size(); ++i) out += static_cast<char>(payload[i] ^ k[i % 4]);
    return out;
}

// A close frame's payload: the code, then the reason.
inline std::string closePayload(int code, std::string_view reason = {}) {
    std::string p;
    p += static_cast<char>((code >> 8) & 0xFF);
    p += static_cast<char>(code & 0xFF);
    return p.append(reason.substr(0, 120));
}

struct Frame {
    bool fin = true;
    Opcode op = Text;
    std::string payload;
};

// Takes one complete frame from the front of `buffer`. nullopt: wait for
// more bytes. `error` is set (and nullopt returned) for a frame that breaks
// the rules: wrong masking for this side, too big, a bad control frame.
inline std::optional<Frame> take(std::string& buffer, bool expectMasked, int& error) {
    error = 0;
    if (buffer.size() < 2) return std::nullopt;
    auto byte = [&](std::size_t i) { return static_cast<std::uint8_t>(buffer[i]); };
    Frame f;
    f.fin = (byte(0) & 0x80) != 0;
    f.op = static_cast<Opcode>(byte(0) & 0x0F);
    if (byte(0) & 0x70) {  // no extensions were agreed on
        error = 1002;
        return std::nullopt;
    }
    bool masked = (byte(1) & 0x80) != 0;
    if (masked != expectMasked) {
        error = 1002;
        return std::nullopt;
    }
    std::uint64_t length = byte(1) & 0x7F;
    std::size_t at = 2;
    if (length == 126) {
        if (buffer.size() < 4) return std::nullopt;
        length = (static_cast<std::uint64_t>(byte(2)) << 8) | byte(3);
        at = 4;
    } else if (length == 127) {
        if (buffer.size() < 10) return std::nullopt;
        length = 0;
        for (int i = 0; i < 8; ++i) length = (length << 8) | byte(2 + static_cast<std::size_t>(i));
        at = 10;
    }
    bool control = (f.op & 0x8) != 0;
    if ((control && (length > 125 || !f.fin)) || length > kMaxMessage) {
        error = control ? 1002 : 1009;
        return std::nullopt;
    }
    std::size_t maskAt = at;
    if (masked) at += 4;
    if (buffer.size() < at + length) return std::nullopt;
    f.payload = buffer.substr(at, static_cast<std::size_t>(length));
    if (masked)
        for (std::size_t i = 0; i < f.payload.size(); ++i) f.payload[i] = static_cast<char>(f.payload[i] ^ buffer[maskAt + i % 4]);
    buffer.erase(0, at + static_cast<std::size_t>(length));
    return f;
}

// Collects messages from frames (continuations, pings, close).
struct Reader {
    std::string partial;
    Opcode partialOp = Text;
    bool inMessage = false;

    // What a frame means for the connection.
    struct Event {
        enum Kind { None, Message, Ping, Close, Error } kind = None;
        std::string data;  // the message, the ping's payload, or the close reason
        int code = 1000;   // Close / Error
    };

    Event feed(Frame f) {
        switch (f.op) {
            case Ping: return {Event::Ping, std::move(f.payload)};
            case Pong: return {};
            case Close: {
                int code = f.payload.size() >= 2
                               ? (static_cast<std::uint8_t>(f.payload[0]) << 8) | static_cast<std::uint8_t>(f.payload[1])
                               : 1005;
                return {Event::Close, f.payload.size() > 2 ? f.payload.substr(2) : std::string{}, code};
            }
            case Text:
            case Binary:
                if (inMessage) return {Event::Error, "a new message before the last one ended", 1002};
                if (f.fin) return {Event::Message, std::move(f.payload)};
                inMessage = true;
                partialOp = f.op;
                partial = std::move(f.payload);
                return {};
            case Continuation:
                if (!inMessage) return {Event::Error, "a continuation without a message", 1002};
                if (partial.size() + f.payload.size() > kMaxMessage) return {Event::Error, "message too big", 1009};
                partial += f.payload;
                if (!f.fin) return {};
                inMessage = false;
                return {Event::Message, std::exchange(partial, {})};
            default: return {Event::Error, "unknown opcode", 1002};
        }
    }
};

}  // namespace rx::detail::ws
