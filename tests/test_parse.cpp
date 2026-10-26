// Tests for rx::parseNumber / rx::parsePort (reactx/parse.hpp). Built natively
// (libstdc++) and for the web (libc++ under Node).
#include <cstdint>
#include <functional>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "reactx/parse.hpp"

using namespace rx;

namespace {

int failures = 0;
std::vector<std::pair<std::string, std::function<void()>>> tests;

#define CHECK(cond)                                                               \
    do {                                                                          \
        if (!(cond)) {                                                            \
            ++failures;                                                           \
            std::println(stderr, "  FAIL {}:{}: {}", __FILE__, __LINE__, #cond);  \
        }                                                                         \
    } while (0)

#define TEST(name)                                                            \
    void name();                                                              \
    const bool name##_registered = (tests.emplace_back(#name, name), true);   \
    void name()

TEST(parses_numbers) {
    CHECK(parseNumber<int>("42") == 42);
    CHECK(parseNumber<int>("-7") == -7);
    CHECK(parseNumber<std::size_t>("1048576") == 1048576u);
    CHECK(parseNumber<int>("").error() == ParseError::Empty);
    CHECK(parseNumber<int>("abc").error() == ParseError::NotANumber);
    CHECK(parseNumber<unsigned>("-1").error() == ParseError::NotANumber);
    CHECK(parseNumber<int>("12abc").error() == ParseError::Trailing);
    CHECK(parseNumber<int>(" 1").error() == ParseError::NotANumber);
    CHECK(parseNumber<std::int8_t>("300").error() == ParseError::OutOfRange);
    CHECK(parseNumber<double>("2.5") == 2.5);
    static_assert(parseNumber<int>("123").value() == 123);  // constexpr (C++23 from_chars)
}

TEST(parses_ports) {
    CHECK(parsePort("8090") == 8090);
    CHECK(parsePort("0") == 0);
    CHECK(!parsePort("65536"));
    CHECK(!parsePort("-1"));
    CHECK(!parsePort("80x"));
    CHECK(parsePort("http").error() == "bad port \"http\": not a number");
}

}  // namespace

int main() {
    for (auto& [name, fn] : tests) {
        std::println("{}", name);
        try {
            fn();
        } catch (const std::exception& e) {
            ++failures;
            std::println(stderr, "  EXCEPTION: {}", e.what());
        }
    }
    if (failures) std::println("FAILED: {}", failures);
    else std::println("OK, all tests passed");
    return failures ? 1 : 0;
}
