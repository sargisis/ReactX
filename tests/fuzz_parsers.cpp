// Fuzzing of the code that reads untrusted input: JSON, HTTP request heads
// and replies (the dev server and the proxy), URLs, route patterns.
// Mutates valid inputs at random for a number of seconds; meant to run under
// ASan + UBSan, which turn any out-of-bounds read, overflow or leak into a
// crash. Every run picks a new seed, so CI keeps exploring:
//   reactx_fuzz_tests [seconds = 20] [seed]
// A failure prints the seed and saves the input to fuzz-crash.bin.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#if (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)) && __has_include(<sanitizer/common_interface_defs.h>)
#include <sanitizer/common_interface_defs.h>
#define REACTX_HAS_SANITIZER_API 1
#endif

#include "reactx/http.hpp"
#include "reactx/json.hpp"
#include "reactx/path.hpp"

namespace {

std::string current;  // the input being tried, for the crash report
std::uint64_t seed = 0;

void saveInput() {
    std::ofstream("fuzz-crash.bin", std::ios::binary) << current;
    std::fprintf(stderr, "fuzz: seed %llu, input saved to fuzz-crash.bin (%zu bytes)\n",
                 static_cast<unsigned long long>(seed), current.size());
}

const std::vector<std::string> kSeeds = {
    R"({"a": [1, 2.5e10, -0, "x\u00e9\ud83d\ude00", true, null, {"b": {}}], "c": "\n\t\\"})",
    "[[[[[[[[[[1]]]]]]]]]]", "\"\\uD800\"", "1e999", "-", "{\"a\":1,}", "{\"name\": \"app\", \"web\": {\"proxy\": {}}}",
    "GET /api/a%2Fb?x=1&y=%zz HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\n",
    "POST / HTTP/1.0\r\nA:b\r\n\r\n", "/todos/:id/*", "http://127.0.0.1:5000/v1/",
    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\nffffffffffffffff\r\nab\r\n0\r\n\r\n",
    "HTTP/1.1 200 OK\r\nContent-Length: 18446744073709551615\r\nSet-Cookie: a\r\n\r\nbody",
    "HTTP/1.0 304 Not Modified\r\n\r\n",
};

std::string mutate(std::mt19937_64& rng, std::string s) {
    static const char* tokens[] = {"[", "{", "\"", "\\u", "%", ":", "\r\n", "*", "/", "e", "-", "0", "ffff", "\r\n\r\n"};
    int edits = 1 + static_cast<int>(rng() % 8);
    for (int i = 0; i < edits; ++i) {
        std::size_t pos = s.empty() ? 0 : rng() % (s.size() + 1);
        switch (rng() % 5) {
            case 0: s.insert(pos, 1, static_cast<char>(rng())); break;
            case 1:
                if (pos < s.size()) s.erase(pos, 1 + rng() % 4);
                break;
            case 2:
                if (pos < s.size()) s[pos] = static_cast<char>(rng());
                break;
            case 3: s.insert(pos, kSeeds[rng() % kSeeds.size()].substr(0, rng() % 24)); break;
            default: s.insert(pos, tokens[rng() % std::size(tokens)]); break;
        }
    }
    return s;
}

// What we write we must read back unchanged.
bool jsonRoundTrips(const std::string& in) {
    auto v = rx::json::tryParse(in);
    if (!v) return true;
    auto again = rx::json::tryParse(v->dump());
    if (!again || again->dump() != v->dump()) return false;
    (void)v->dump(2);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    int seconds = argc > 1 ? std::atoi(argv[1]) : 20;
    seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : std::random_device{}();
#ifdef REACTX_HAS_SANITIZER_API
    __sanitizer_set_death_callback(saveInput);
#endif
    std::mt19937_64 rng(seed);
    long runs = 0;
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < until) {
        current = mutate(rng, kSeeds[rng() % kSeeds.size()]);
        if (!jsonRoundTrips(current)) {
            std::fprintf(stderr, "fuzz: JSON doesn't read back what it wrote\n");
            saveInput();
            return 1;
        }
        (void)rx::net::parseRequestHead(current);
        (void)rx::net::parseResponse(current);
        (void)rx::net::parseResponse(current, true);
        (void)rx::net::parseUpstream(current);
        (void)rx::urlDecode(current, true);
        (void)rx::queryParam(current, "x");
        std::map<std::string, std::string> params;
        (void)rx::matchPath(current, mutate(rng, "/todos/7/a/b"), params);
        ++runs;
    }
    std::printf("fuzz: %ld inputs in %d s (seed %llu), no failure\n", runs, seconds, static_cast<unsigned long long>(seed));
    return 0;
}
