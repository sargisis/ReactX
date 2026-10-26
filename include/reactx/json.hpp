#pragma once

// rx::json — a small JSON library (C++20): parse, build, read, write.
//
//   auto v = rx::json::parse(R"({"title": "Buy milk", "done": false, "tags": ["home"]})");
//   std::string title = v["title"].asString();
//   bool done = v.get<bool>("done");          // with a fallback for missing keys
//   v["done"] = true;
//   v["tags"].push("shop");
//   std::string text = v.dump();              // {"done":true,"tags":["home","shop"],"title":"Buy milk"}
//   auto maybe = rx::json::tryParse(input);   // std::expected<Value, json::Error>, no exceptions
//
//   rx::json::Value todo = rx::json::object({{"id", 1}, {"title", "Read"}});
//
// Numbers are doubles (integers up to 2^53 are exact and printed without ".0").
// Object keys are kept sorted, so dump() is stable.

#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rx::json {

class Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value, std::less<>>;

// Thrown by parse() (with the byte offset) and by as*() on a type mismatch.
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& message, std::size_t offset = 0)
        : std::runtime_error(message), offset_(offset) {}
    std::size_t offset() const { return offset_; }

private:
    std::size_t offset_;
};

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    Value() = default;  // null
    Value(std::nullptr_t) {}
    Value(bool b) : data_(b) {}
    template <class T>
        requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
    Value(T number) : data_(static_cast<double>(number)) {}
    Value(const char* s) : data_(std::string(s)) {}
    Value(std::string s) : data_(std::move(s)) {}
    Value(std::string_view s) : data_(std::string(s)) {}
    Value(Array a) : data_(std::move(a)) {}
    Value(Object o) : data_(std::move(o)) {}

    Type type() const { return static_cast<Type>(data_.index()); }
    bool isNull() const { return type() == Type::Null; }
    bool isBool() const { return type() == Type::Bool; }
    bool isNumber() const { return type() == Type::Number; }
    bool isString() const { return type() == Type::String; }
    bool isArray() const { return type() == Type::Array; }
    bool isObject() const { return type() == Type::Object; }

    // Throw json::Error if the type is different.
    // On a temporary (json::parse(text).asArray()) they return a copy instead
    // of a reference, so `for (auto& x : json::parse(t).asArray())` is safe in
    // C++20 too (the temporary would be gone before the loop starts).
    bool asBool() const;
    double asNumber() const;
    std::int64_t asInt() const;
    const std::string& asString() const&;
    std::string asString() &&;
    const Array& asArray() const&;
    Array& asArray() &;
    Array asArray() &&;
    const Object& asObject() const&;
    Object& asObject() &;
    Object asObject() &&;

    // Object access. The non-const operator[] turns null into an object and
    // adds missing keys; the const one returns null for missing keys.
    Value& operator[](std::string_view key);
    const Value& operator[](std::string_view key) const;
    bool contains(std::string_view key) const;
    // Typed read with a fallback when the key is missing or has another type:
    // v.get<int>("count", 0), v.get<std::string>("name").
    template <class T>
    T get(std::string_view key, T fallback = T{}) const;

    // Array access.
    Value& operator[](std::size_t index);
    const Value& operator[](std::size_t index) const;
    void push(Value v);  // null becomes an array
    std::size_t size() const;  // elements of an array / object, 0 otherwise

    // Compact JSON text. indent > 0 pretty-prints.
    std::string dump(int indent = 0) const;

    bool operator==(const Value&) const = default;

private:
    void write(std::string& out, int indent, int depth) const;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_;
};

// Builders: object({{"a", 1}, {"b", "x"}}), array({1, 2, "three"}).
Value object(std::initializer_list<std::pair<const std::string, Value>> members = {});
Value array(std::initializer_list<Value> items = {});

// Parses JSON text. Throws json::Error on invalid input.
Value parse(std::string_view text);
// The same without exceptions: the value, or the error (message + offset).
//   if (auto v = json::tryParse(body)) use(*v); else log(v.error().what());
std::expected<Value, Error> tryParse(std::string_view text);

// JSON string literal for `s` (with quotes and escapes).
std::string quote(std::string_view s);

// ---- template implementation ----

template <class T>
T Value::get(std::string_view key, T fallback) const {
    if (!isObject()) return fallback;
    const Object& o = std::get<Object>(data_);
    auto it = o.find(key);
    if (it == o.end()) return fallback;
    const Value& v = it->second;
    if constexpr (std::is_same_v<T, bool>) {
        return v.isBool() ? v.asBool() : fallback;
    } else if constexpr (std::is_arithmetic_v<T>) {
        return v.isNumber() ? static_cast<T>(v.asNumber()) : fallback;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return v.isString() ? v.asString() : fallback;
    } else if constexpr (std::is_same_v<T, Value>) {
        return v;
    } else {
        static_assert(sizeof(T) == 0, "json::Value::get: unsupported type");
    }
}

}  // namespace rx::json
