// Tests for rx::json.
#include <functional>
#include <utility>
#include <iostream>
#include <string>
#include <vector>

#include "reactx/json.hpp"

#include <map>
#include <optional>

using namespace rx;

namespace {

int failures = 0;
std::vector<std::pair<std::string, std::function<void()>>> tests;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

#define TEST(name)                                                            \
    void name();                                                              \
    const bool name##_registered = (tests.emplace_back(#name, name), true);   \
    void name()

bool fails(std::string_view text) {
    try {
        json::parse(text);
        return false;
    } catch (const json::Error&) {
        return true;
    }
}

TEST(parses_scalars) {
    CHECK(json::parse("null").isNull());
    CHECK(json::parse("true").asBool());
    CHECK(!json::parse(" false ").asBool());
    CHECK(json::parse("42").asInt() == 42);
    CHECK(json::parse("-0.5").asNumber() == -0.5);
    CHECK(json::parse("1e3").asNumber() == 1000);
    CHECK(json::parse("2.5E-1").asNumber() == 0.25);
    CHECK(json::parse(R"("hi")").asString() == "hi");
}

TEST(parses_nested_values) {
    auto v = json::parse(R"({"title": "Buy milk", "done": false, "tags": ["home", "shop"], "meta": {"n": 3}})");
    CHECK(v.isObject());
    CHECK(v.size() == 4);
    CHECK(v["title"].asString() == "Buy milk");
    CHECK(v["tags"].size() == 2);
    CHECK(v["tags"][1].asString() == "shop");
    CHECK(v["meta"]["n"].asInt() == 3);
    CHECK(v["missing"].isNull());        // const-style lookup of a missing key
    CHECK(std::as_const(v)["tags"][9].isNull());  // out of range on a const value: null
}

TEST(string_escapes_and_unicode) {
    CHECK(json::parse(R"("a\"b\\c\/d\n\t")").asString() == "a\"b\\c/d\n\t");
    CHECK(json::parse(R"("привет")").asString() == "привет");
    CHECK(json::parse(R"("😀")").asString() == "😀");  // surrogate pair
    CHECK(json::parse(R"("уже UTF-8")").asString() == "уже UTF-8");
    CHECK(json::quote("a\"b\n\x01") == R"("a\"b\n\u0001")");
}

TEST(rejects_invalid_json) {
    CHECK(fails(""));
    CHECK(fails("{"));
    CHECK(fails("[1, 2,]"));
    CHECK(fails(R"({"a" 1})"));
    CHECK(fails(R"({a: 1})"));
    CHECK(fails("01"));
    CHECK(fails("1."));
    CHECK(fails("-"));
    CHECK(fails("tru"));
    CHECK(fails(R"("unterminated)"));
    CHECK(fails("\"tab\there\""));      // raw control character
    CHECK(fails(R"("\ud83d")"));        // lone surrogate
    CHECK(fails("1e400"));               // out of range
    CHECK(fails("[1] [2]"));
    CHECK(fails(std::string(600, '[') + std::string(600, ']')));  // too deep
}

TEST(error_has_offset) {
    try {
        json::parse(R"({"a": tru})");
        CHECK(false);
    } catch (const json::Error& e) {
        CHECK(e.offset() == 6);
        CHECK(std::string(e.what()).find("offset 6") != std::string::npos);
    }
}

TEST(dump_is_compact_and_sorted) {
    auto v = json::parse(R"({ "b": [1, 2.5, -3], "a": "x", "c": {"z": null, "y": true} })");
    CHECK(v.dump() == R"({"a":"x","b":[1,2.5,-3],"c":{"y":true,"z":null}})");
    CHECK(json::parse(v.dump()) == v);  // round trip
}

TEST(pretty_print) {
    auto v = json::object({{"a", 1}, {"list", json::array({true, "x"})}});
    CHECK(v.dump(2) == "{\n  \"a\": 1,\n  \"list\": [\n    true,\n    \"x\"\n  ]\n}");
    CHECK(json::array().dump(2) == "[]");
}

TEST(builders_and_mutation) {
    json::Value todo = json::object({{"id", 7}, {"title", "Read"}});
    todo["done"] = true;
    todo["tags"].push("book");
    todo["tags"].push(2);
    CHECK(todo.dump() == R"({"done":true,"id":7,"tags":["book",2],"title":"Read"})");

    json::Value empty;
    empty["x"]["y"] = 1;  // null becomes nested objects
    CHECK(empty.dump() == R"({"x":{"y":1}})");
}

TEST(get_with_fallback) {
    auto v = json::parse(R"({"n": 5, "s": "str", "b": true})");
    CHECK(v.get<int>("n") == 5);
    CHECK(v.get<int>("missing", -1) == -1);
    CHECK(v.get<int>("s", -1) == -1);  // wrong type
    CHECK(v.get<std::string>("s") == "str");
    CHECK(v.get<bool>("b"));
    CHECK(v.contains("n") && !v.contains("zzz"));
}

TEST(type_mismatch_throws) {
    bool threw = false;
    try {
        (void)json::parse("[1]").asString();
    } catch (const json::Error& e) {
        threw = std::string(e.what()) == "json: expected string, got array";
    }
    CHECK(threw);
}

TEST(try_parse_returns_expected) {
    auto ok = json::tryParse(R"({"a": [1, 2]})");
    CHECK(ok.has_value());
    CHECK((*ok)["a"].size() == 2);
    auto bad = json::tryParse(R"({"a": tru})");
    CHECK(!bad.has_value());
    CHECK(bad.error().offset() == 6);
    CHECK(json::tryParse("[1,").transform([](const json::Value& v) { return v.size(); }).value_or(99) == 99);
    CHECK(json::tryParse("[1,2,3]").transform([](const json::Value& v) { return v.size(); }).value_or(99) == 3);
}

TEST(temporaries_are_safe_in_range_for) {
    // C++20 would destroy the parsed value before the loop if asArray()
    // returned a reference here; on a temporary it returns a copy.
    int sum = 0;
    for (const auto& n : json::parse("[1, 2, 3]").asArray()) sum += static_cast<int>(n.asInt());
    CHECK(sum == 6);
    std::string s = json::parse(R"("moved")").asString();
    CHECK(s == "moved");
    std::size_t keys = 0;
    for (const auto& [k, v] : json::parse(R"({"a": 1, "b": 2})").asObject()) keys += k.size();
    CHECK(keys == 2);
}

TEST(numbers_print_cleanly) {
    CHECK(json::Value(3).dump() == "3");
    CHECK(json::Value(0.1).dump() == "0.1");
    CHECK(json::Value(-2.5e-8).dump() == "-2.5e-08");
    CHECK(json::Value(1e20).dump() == "1e+20");  // beyond 2^53: shortest exact form
    CHECK(json::parse("9007199254740993").dump() == "9007199254740992");  // beyond 2^53: nearest double
}

// ---- structs with C++26 reflection (native GCC 16; not in the web build) ----
#if defined(__cpp_lib_reflection)

struct Point {
    int x = 0;
    int y = 0;
    bool operator==(const Point&) const = default;
};

struct Shape {
    std::string name;
    std::vector<Point> points;
    std::optional<std::string> color;  // may be missing
    std::map<std::string, double> tags;
    bool closed = false;
    std::string default_;  // key "default" (trailing '_' dropped)
    bool operator==(const Shape&) const = default;
};

TEST(reflection_struct_to_json) {
    Shape s{"tri", {{0, 0}, {4, 0}, {0, 3}}, std::nullopt, {{"area", 6}}, true, "yes"};
    CHECK(json::to(s).dump() ==
          R"({"closed":true,"color":null,"default":"yes","name":"tri","points":[{"x":0,"y":0},{"x":4,"y":0},{"x":0,"y":3}],"tags":{"area":6}})");
    CHECK(json::to(Point{1, 2}) == json::object({{"x", 1}, {"y", 2}}));
    CHECK(json::to(std::vector<int>{1, 2}).dump() == "[1,2]");
}

TEST(reflection_round_trip) {
    Shape s{"sq", {{0, 0}, {1, 1}}, "red", {{"w", 1.5}}, false, ""};
    auto back = json::from<Shape>(json::to(s));
    CHECK(back && *back == s);
}

TEST(reflection_reads_and_checks_types) {
    auto p = json::from<Point>(json::parse(R"({"x": 3, "y": -4, "extra": "ignored"})"));
    CHECK(p && *p == (Point{3, -4}));

    auto missing = json::from<Point>(json::parse(R"({"x": 3})"));
    CHECK(!missing && std::string(missing.error().what()) == "missing field \"y\"");

    auto wrongType = json::from<Point>(json::parse(R"({"x": "3", "y": 0})"));
    CHECK(!wrongType && std::string(wrongType.error().what()) == "field \"x\": expected a number");

    auto fraction = json::from<Point>(json::parse(R"({"x": 1.5, "y": 0})"));
    CHECK(!fraction && std::string(fraction.error().what()) == "field \"x\": expected an integer that fits");

    // The error says where, even deep inside.
    auto deep = json::from<Shape>(json::parse(R"({"name": "t", "points": [{"x": 0, "y": 0}, {"x": 1}], "tags": {}, "closed": false, "default": ""})"));
    CHECK(!deep && std::string(deep.error().what()) == "field \"points\": [1]: missing field \"y\"");

    // std::optional: missing or null -> empty.
    auto noColor = json::from<Shape>(json::parse(R"({"name": "t", "points": [], "tags": {}, "closed": true, "default": ""})"));
    CHECK(noColor && !noColor->color);
    CHECK(!json::from<Point>(json::parse("[1, 2]")));  // not an object
}

#endif  // __cpp_lib_reflection

}  // namespace

int main() {
    for (auto& [name, fn] : tests) {
        std::cout << name << '\n';
        try {
            fn();
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "  EXCEPTION: " << e.what() << '\n';
        }
    }
    std::cout << (failures ? "FAILED: " : "OK, all tests passed") << (failures ? std::to_string(failures) : "")
              << '\n';
    return failures ? 1 : 0;
}
