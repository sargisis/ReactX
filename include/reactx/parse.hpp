#pragma once

// Text -> number without exceptions or locales: std::from_chars + std::expected.
//
//   auto n = rx::parseNumber<int>("42");          // std::expected<int, rx::ParseError>
//   if (!n) std::println("{}", rx::describe(n.error()));
//   auto port = rx::parsePort(argv[2]);           // std::expected<int, std::string>
//
// Unlike std::stoi, "12abc", "" and "99999999999" are errors, not 12, an
// exception and an exception.

#include <charconv>
#include <concepts>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>

namespace rx {

enum class ParseError {
    Empty,       // ""
    NotANumber,  // "abc", "-1" for an unsigned type
    OutOfRange,  // doesn't fit the type
    Trailing,    // "12abc": a number followed by something else
};

constexpr std::string_view describe(ParseError error) {
    switch (error) {
        case ParseError::Empty: return "empty";
        case ParseError::NotANumber: return "not a number";
        case ParseError::OutOfRange: return "out of range";
        case ParseError::Trailing: return "unexpected characters after the number";
    }
    return "invalid";
}

template <class T>
concept Number = (std::integral<T> && !std::same_as<T, bool>) || std::floating_point<T>;

// The whole of `text` as a T (decimal; a leading '-' only for signed types).
template <Number T>
constexpr std::expected<T, ParseError> parseNumber(std::string_view text) {
    if (text.empty()) return std::unexpected(ParseError::Empty);
    T value{};
    const char* last = text.data() + text.size();
    auto [end, ec] = std::from_chars(text.data(), last, value);
    if (ec == std::errc::invalid_argument) return std::unexpected(ParseError::NotANumber);
    if (ec == std::errc::result_out_of_range) return std::unexpected(ParseError::OutOfRange);
    if (end != last) return std::unexpected(ParseError::Trailing);
    return value;
}

// A TCP port for --port options: 0..65535 (0 = pick a free one).
inline std::expected<int, std::string> parsePort(std::string_view text) {
    auto bad = [text](std::string_view why) {
        return std::unexpected("bad port \"" + std::string(text) + "\": " + std::string(why));
    };
    return parseNumber<int>(text)
        .transform_error([&](ParseError e) { return bad(describe(e)).error(); })
        .and_then([&](int port) -> std::expected<int, std::string> {
            if (port < 0 || port > 65535) return bad("must be 0..65535");
            return port;
        });
}

}  // namespace rx
