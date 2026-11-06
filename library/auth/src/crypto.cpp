// SHA-256 (FIPS 180-4), HMAC-SHA256 (RFC 2104), PBKDF2 (RFC 8018), random
// bytes from the kernel, URL-safe base64. Checked against the published test
// vectors (NIST, RFC 4231, RFC 7914 section 11).
#include <sys/random.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "reactx/parse.hpp"
#include "rx/auth.hpp"

namespace rx::auth {

namespace {

using Digest = std::array<std::uint8_t, 32>;

class Sha256 {
public:
    void update(const std::uint8_t* data, std::size_t n) {
        length_ += n;
        while (n > 0) {
            const std::size_t take = std::min(n, 64 - used_);
            std::memcpy(block_.data() + used_, data, take);
            used_ += take;
            data += take;
            n -= take;
            if (used_ == 64) {
                compress(block_.data());
                used_ = 0;
            }
        }
    }
    void update(std::string_view s) { update(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()); }

    Digest finish() {
        const std::uint64_t bits = length_ * 8;
        const std::uint8_t one = 0x80, zero = 0;
        update(&one, 1);
        while (used_ != 56) update(&zero, 1);
        std::uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
        update(len, 8);
        Digest out;
        for (int i = 0; i < 8; ++i)
            for (int b = 0; b < 4; ++b) out[static_cast<std::size_t>(4 * i + b)] = static_cast<std::uint8_t>(h_[static_cast<std::size_t>(i)] >> (24 - 8 * b));
        return out;
    }

private:
    static constexpr std::array<std::uint32_t, 64> k{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    static std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void compress(const std::uint8_t* p) {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16; ++i)
            w[i] = static_cast<std::uint32_t>(p[4 * i]) << 24 | static_cast<std::uint32_t>(p[4 * i + 1]) << 16 |
                   static_cast<std::uint32_t>(p[4 * i + 2]) << 8 | static_cast<std::uint32_t>(p[4 * i + 3]);
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h_[0] += a, h_[1] += b, h_[2] += c, h_[3] += d, h_[4] += e, h_[5] += f, h_[6] += g, h_[7] += h;
    }

    std::array<std::uint32_t, 8> h_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<std::uint8_t, 64> block_{};
    std::size_t used_ = 0;
    std::uint64_t length_ = 0;
};

// HMAC with the key's inner and outer states prepared once: each message then
// costs two compressions (PBKDF2 runs it hundreds of thousands of times).
class Hmac {
public:
    explicit Hmac(std::string_view key) {
        std::array<std::uint8_t, 64> k{};
        if (key.size() > 64) {
            Sha256 s;
            s.update(key);
            const Digest d = s.finish();
            std::memcpy(k.data(), d.data(), d.size());
        } else {
            std::memcpy(k.data(), key.data(), key.size());
        }
        std::array<std::uint8_t, 64> pad{};
        for (std::size_t i = 0; i < 64; ++i) pad[i] = k[i] ^ 0x36;
        inner_.update(pad.data(), 64);
        for (std::size_t i = 0; i < 64; ++i) pad[i] = k[i] ^ 0x5c;
        outer_.update(pad.data(), 64);
        std::ranges::fill(k, 0);
    }
    Digest operator()(const std::uint8_t* data, std::size_t n) const {
        Sha256 in = inner_;
        in.update(data, n);
        const Digest d = in.finish();
        Sha256 out = outer_;
        out.update(d.data(), d.size());
        return out.finish();
    }
    Digest operator()(std::string_view s) const { return (*this)(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()); }

private:
    Sha256 inner_, outer_;
};

Digest pbkdf2(std::string_view password, std::string_view salt, int iterations) {
    const Hmac mac(password);
    std::string first(salt);
    first += std::string("\0\0\0\1", 4);  // block 1 (32 bytes is all we need)
    Digest u = mac(first);
    Digest t = u;
    for (int i = 1; i < iterations; ++i) {
        u = mac(u.data(), u.size());
        for (std::size_t j = 0; j < t.size(); ++j) t[j] ^= u[j];
    }
    return t;
}

constexpr std::string_view kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string base64url(const std::uint8_t* p, std::size_t n) {
    std::string out;
    std::uint32_t acc = 0;
    int bits = 0;
    for (std::size_t i = 0; i < n; ++i) {
        acc = (acc << 8) | p[i];
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out += kB64[(acc >> bits) & 63];
        }
    }
    if (bits > 0) out += kB64[(acc << (6 - bits)) & 63];
    return out;
}

std::optional<std::string> unbase64url(std::string_view s) {
    std::string out;
    std::uint32_t acc = 0;
    int bits = 0;
    for (char c : s) {
        const auto v = kB64.find(c);
        if (v == std::string_view::npos) return std::nullopt;
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((acc >> bits) & 0xFF);
        }
    }
    return out;
}

std::string hex(const Digest& d) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out;
    for (std::uint8_t b : d) {
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}

void fillRandom(std::uint8_t* p, std::size_t n) {
    while (n > 0) {
        const ssize_t got = ::getrandom(p, n, 0);
        if (got < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("getrandom failed: no secure random numbers");
        }
        p += got;
        n -= static_cast<std::size_t>(got);
    }
}

struct Stored {
    int iterations = 0;
    std::string salt, hash;
};

std::optional<Stored> parseStored(std::string_view s) {
    constexpr std::string_view prefix = "pbkdf2-sha256$";
    if (!s.starts_with(prefix)) return std::nullopt;
    s.remove_prefix(prefix.size());
    const auto a = s.find('$');
    if (a == std::string_view::npos) return std::nullopt;
    const auto b = s.find('$', a + 1);
    if (b == std::string_view::npos) return std::nullopt;
    auto iterations = parseNumber<int>(s.substr(0, a));
    auto salt = unbase64url(s.substr(a + 1, b - a - 1));
    auto hash = unbase64url(s.substr(b + 1));
    if (!iterations || *iterations < 1 || !salt || !hash || hash->size() != 32) return std::nullopt;
    return Stored{*iterations, std::move(*salt), std::move(*hash)};
}

}  // namespace

std::string randomToken(std::size_t bytes) {
    std::string buf(bytes, '\0');
    fillRandom(reinterpret_cast<std::uint8_t*>(buf.data()), bytes);
    return base64url(reinterpret_cast<const std::uint8_t*>(buf.data()), bytes);
}

std::string sha256Hex(std::string_view data) {
    Sha256 s;
    s.update(data);
    return hex(s.finish());
}

std::string hmacSha256Hex(std::string_view key, std::string_view data) { return hex(Hmac(key)(data)); }

bool constantTimeEquals(std::string_view a, std::string_view b) {
    std::uint8_t diff = a.size() == b.size() ? 0 : 1;
    const std::size_t n = std::max(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto x = static_cast<std::uint8_t>(i < a.size() ? a[i] : 0);
        const auto y = static_cast<std::uint8_t>(i < b.size() ? b[i] : 0);
        diff |= static_cast<std::uint8_t>(x ^ y);
    }
    return diff == 0;
}

std::string hashPassword(std::string_view password, int iterations) {
    std::array<std::uint8_t, 16> salt{};
    fillRandom(salt.data(), salt.size());
    const std::string saltBytes(reinterpret_cast<const char*>(salt.data()), salt.size());
    const Digest d = pbkdf2(password, saltBytes, iterations);
    return "pbkdf2-sha256$" + std::to_string(iterations) + "$" + base64url(salt.data(), salt.size()) + "$" +
           base64url(d.data(), d.size());
}

bool verifyPassword(std::string_view password, std::string_view stored) {
    auto s = parseStored(stored);
    if (!s || s->iterations > 10'000'000) return false;  // a planted huge count would hang the server
    const Digest d = pbkdf2(password, s->salt, s->iterations);
    return constantTimeEquals(std::string_view(reinterpret_cast<const char*>(d.data()), d.size()), s->hash);
}

bool needsRehash(std::string_view stored, int iterations) {
    auto s = parseStored(stored);
    return !s || s->iterations < iterations;
}

// For the throwaway vector checks only (not in the header): PBKDF2 hex for any salt.
std::string pbkdf2HexForTests(std::string_view password, std::string_view salt, int iterations) {
    return hex(pbkdf2(password, salt, iterations));
}

}  // namespace rx::auth
