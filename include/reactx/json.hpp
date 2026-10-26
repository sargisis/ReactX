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
// With C++26 reflection (<meta>; GCC 16 with -freflection, which ReactX turns
// on natively) any plain struct converts by itself — the field names are the
// keys. Not in the browser build: Emscripten has no reflection yet.
//
//   struct Todo { int id; std::string title; bool done; };
//   rx::json::Value v = rx::json::to(Todo{1, "Read", false});   // {"done":false,"id":1,"title":"Read"}
//   auto todo = rx::json::from<Todo>(v);                         // std::expected<Todo, json::Error>
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
#include <version>

#if defined(__cpp_lib_reflection)
#include <cmath>
#include <concepts>
#include <limits>
#include <meta>
#include <optional>
#include <ranges>
#endif

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


// ---- structs <-> JSON with C++26 reflection ----

#if defined(__cpp_lib_reflection)

namespace detail {

template <class T>
inline constexpr bool isOptional = false;
template <class T>
inline constexpr bool isOptional<std::optional<T>> = true;

template <class T>
concept StringLike = std::convertible_to<const T&, std::string_view>;

// std::map<std::string, T>, std::flat_map<std::string, T, ...>, ...
template <class T>
concept StringMap = requires { typename T::key_type; typename T::mapped_type; } &&
                    std::same_as<typename T::key_type, std::string>;

// std::vector<T>, std::list<T>, ... (not strings, not maps)
template <class T>
concept Sequence = std::ranges::range<T> && !StringLike<T> && !StringMap<T> &&
                   requires(T t, std::ranges::range_value_t<T> x) { t.push_back(std::move(x)); };

// A struct ReactX can walk with reflection: an aggregate class (public
// fields, no constructors), e.g. struct Todo { int id; std::string title; }.
template <class T>
concept Record = std::is_class_v<T> && std::is_aggregate_v<T> && !Sequence<T> && !StringMap<T>;

// The JSON key of a field: its name, without a trailing '_' (so a field can
// be called `template_` or `default_` and still read "template" / "default").
consteval std::string_view keyOf(std::meta::info member) {
    std::string_view name = std::meta::identifier_of(member);
    return name.ends_with('_') ? name.substr(0, name.size() - 1) : name;
}

template <class T>
consteval auto fieldsOf() {
    return std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));
}

}  // namespace detail

// C++ value -> JSON. Supports bool, numbers, strings, std::optional (empty
// -> null), sequences (std::vector, ...), maps with string keys, json::Value
// and structs of those (via reflection), nested to any depth.
template <class T>
Value to(const T& value) {
    if constexpr (std::same_as<T, Value>) {
        return value;
    } else if constexpr (std::same_as<T, bool> || std::is_arithmetic_v<T>) {
        return Value(value);
    } else if constexpr (detail::StringLike<T>) {
        return Value(std::string_view(value));
    } else if constexpr (detail::isOptional<T>) {
        return value ? to(*value) : Value(nullptr);
    } else if constexpr (detail::StringMap<T>) {
        Value out = object();
        for (const auto& [key, item] : value) out[key] = to(item);
        return out;
    } else if constexpr (detail::Sequence<T>) {
        Value out = array();
        for (const auto& item : value) out.push(to(item));
        return out;
    } else if constexpr (detail::Record<T>) {
        Value out = object();
        template for (constexpr std::meta::info field : detail::fieldsOf<T>()) {
            out[detail::keyOf(field)] = to(value.[:field:]);
        }
        return out;
    } else {
        static_assert(false, "json::to: this type can't be converted to JSON");
    }
}

// JSON -> C++ value, checking every type. Unknown keys are ignored; a
// missing key is an error unless the field is a std::optional. The error
// says where: "field \"deps\": [2]: field \"rev\": expected a string".
template <class T>
std::expected<T, Error> from(const Value& v) {
    auto expected = [](std::string_view what) { return std::unexpected(Error("expected " + std::string(what))); };
    if constexpr (std::same_as<T, Value>) {
        return v;
    } else if constexpr (std::same_as<T, bool>) {
        if (!v.isBool()) return expected("true or false");
        return v.asBool();
    } else if constexpr (std::integral<T>) {
        if (!v.isNumber()) return expected("a number");
        double d = v.asNumber();
        if (d != std::trunc(d) || d < static_cast<double>(std::numeric_limits<T>::min()) ||
            d > static_cast<double>(std::numeric_limits<T>::max()))
            return expected("an integer that fits");
        return static_cast<T>(d);
    } else if constexpr (std::floating_point<T>) {
        if (!v.isNumber()) return expected("a number");
        return static_cast<T>(v.asNumber());
    } else if constexpr (std::same_as<T, std::string>) {
        if (!v.isString()) return expected("a string");
        return v.asString();
    } else if constexpr (detail::isOptional<T>) {
        if (v.isNull()) return T{};
        return from<typename T::value_type>(v).transform([](auto x) { return T(std::move(x)); });
    } else if constexpr (detail::StringMap<T>) {
        if (!v.isObject()) return expected("an object");
        T out;
        for (const auto& [key, item] : v.asObject()) {
            auto x = from<typename T::mapped_type>(item);
            if (!x) return std::unexpected(Error("\"" + key + "\": " + x.error().what()));
            out.emplace(key, std::move(*x));
        }
        return out;
    } else if constexpr (detail::Sequence<T>) {
        if (!v.isArray()) return expected("an array");
        T out;
        for (std::size_t i = 0; i < v.size(); ++i) {
            auto x = from<std::ranges::range_value_t<T>>(v[i]);
            if (!x) return std::unexpected(Error("[" + std::to_string(i) + "]: " + x.error().what()));
            out.push_back(std::move(*x));
        }
        return out;
    } else if constexpr (detail::Record<T>) {
        if (!v.isObject()) return expected("an object");
        T out{};
        std::optional<Error> failed;
        template for (constexpr std::meta::info field : detail::fieldsOf<T>()) {
            using Field = std::remove_cvref_t<typename[:std::meta::type_of(field):]>;
            constexpr std::string_view key = detail::keyOf(field);
            if (failed) {
                // an earlier field failed: skip the rest
            } else if (!v.contains(key)) {
                if constexpr (!detail::isOptional<Field>) failed = Error("missing field \"" + std::string(key) + "\"");
            } else if (auto x = from<Field>(v[key])) {
                out.[:field:] = std::move(*x);
            } else {
                failed = Error("field \"" + std::string(key) + "\": " + x.error().what());
            }
        }
        if (failed) return std::unexpected(std::move(*failed));
        return out;
    } else {
        static_assert(false, "json::from: this type can't be read from JSON");
    }
}

#endif  // __cpp_lib_reflection

}  // namespace rx::json
