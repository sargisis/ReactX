#include "reactx/json.hpp"

#include "number_text.hpp"

#include <charconv>
#include <cmath>
#include <system_error>

namespace rx::json {

namespace {

const char* typeName(Type t) {
    switch (t) {
        case Type::Null: return "null";
        case Type::Bool: return "bool";
        case Type::Number: return "number";
        case Type::String: return "string";
        case Type::Array: return "array";
        default: return "object";
    }
}

[[noreturn]] void mismatch(Type wanted, Type got) {
    throw Error(std::string("json: expected ") + typeName(wanted) + ", got " + typeName(got));
}

const Value& nullValue() {
    static const Value null;
    return null;
}

void appendUtf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

void writeNumber(std::string& out, double d) {
    if (!std::isfinite(d)) {  // JSON has no NaN / Infinity
        out += "null";
        return;
    }
    char buf[32];
    if (d == std::trunc(d) && std::fabs(d) < 9007199254740992.0) {  // whole number: no ".0"
        auto [p, ec] = std::to_chars(buf, buf + sizeof buf, static_cast<std::int64_t>(d));
        out.append(buf, p);
    } else {
        out += rx::detail::numberText(d);  // shortest round-trip form
    }
}

// ---- parser ----

class Parser {
public:
    explicit Parser(std::string_view text) : s_(text) {}

    Value document() {
        Value v = value(0);
        skipSpace();
        if (pos_ != s_.size()) fail("unexpected text after the value");
        return v;
    }

private:
    static constexpr int kMaxDepth = 512;

    [[noreturn]] void fail(const std::string& what) const {
        throw Error("json: " + what + " at offset " + std::to_string(pos_), pos_);
    }

    void skipSpace() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r'))
            ++pos_;
    }

    bool consume(std::string_view word) {
        if (s_.substr(pos_, word.size()) != word) return false;
        pos_ += word.size();
        return true;
    }

    Value value(int depth) {
        if (depth > kMaxDepth) fail("nesting too deep");
        skipSpace();
        if (pos_ >= s_.size()) fail("unexpected end");
        char c = s_[pos_];
        if (c == '{') return objectValue(depth);
        if (c == '[') return arrayValue(depth);
        if (c == '"') return Value(string());
        if (c == '-' || (c >= '0' && c <= '9')) return number();
        if (consume("true")) return Value(true);
        if (consume("false")) return Value(false);
        if (consume("null")) return Value();
        fail(std::string("unexpected character '") + c + "'");
    }

    Value objectValue(int depth) {
        ++pos_;  // {
        Object o;
        skipSpace();
        if (pos_ < s_.size() && s_[pos_] == '}') {
            ++pos_;
            return Value(std::move(o));
        }
        while (true) {
            skipSpace();
            if (pos_ >= s_.size() || s_[pos_] != '"') fail("expected a key string");
            std::string key = string();
            skipSpace();
            if (pos_ >= s_.size() || s_[pos_] != ':') fail("expected ':'");
            ++pos_;
            o.insert_or_assign(std::move(key), value(depth + 1));
            skipSpace();
            if (pos_ < s_.size() && s_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < s_.size() && s_[pos_] == '}') {
                ++pos_;
                return Value(std::move(o));
            }
            fail("expected ',' or '}'");
        }
    }

    Value arrayValue(int depth) {
        ++pos_;  // [
        Array a;
        skipSpace();
        if (pos_ < s_.size() && s_[pos_] == ']') {
            ++pos_;
            return Value(std::move(a));
        }
        while (true) {
            a.push_back(value(depth + 1));
            skipSpace();
            if (pos_ < s_.size() && s_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < s_.size() && s_[pos_] == ']') {
                ++pos_;
                return Value(std::move(a));
            }
            fail("expected ',' or ']'");
        }
    }

    std::uint32_t hex4() {
        if (pos_ + 4 > s_.size()) fail("short \\u escape");
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
            else fail("bad \\u escape");
        }
        return v;
    }

    std::string string() {
        ++pos_;  // opening quote
        std::string out;
        while (true) {
            if (pos_ >= s_.size()) fail("unterminated string");
            char c = s_[pos_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= s_.size()) fail("unterminated escape");
            char e = s_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {  // surrogate pair
                        if (!consume("\\u")) fail("lone high surrogate");
                        std::uint32_t low = hex4();
                        if (low < 0xDC00 || low > 0xDFFF) fail("bad low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail("lone low surrogate");
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: fail("bad escape");
            }
        }
    }

    Value number() {
        std::size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        auto digits = [&] {
            std::size_t from = pos_;
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
            return pos_ - from;
        };
        if (pos_ < s_.size() && s_[pos_] == '0') ++pos_;  // no leading zeros
        else if (digits() == 0) fail("bad number");
        if (pos_ < s_.size() && s_[pos_] == '.') {
            ++pos_;
            if (digits() == 0) fail("bad number");
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (digits() == 0) fail("bad number");
        }
        double d = 0;
        auto [p, ec] = std::from_chars(s_.data() + start, s_.data() + pos_, d);
        if (ec == std::errc::result_out_of_range) fail("number out of range");
        if (ec != std::errc{} || p != s_.data() + pos_) fail("bad number");
        return Value(d);
    }

    std::string_view s_;
    std::size_t pos_ = 0;
};

}  // namespace

// ---- Value ----

bool Value::asBool() const {
    if (!isBool()) mismatch(Type::Bool, type());
    return std::get<bool>(data_);
}

double Value::asNumber() const {
    if (!isNumber()) mismatch(Type::Number, type());
    return std::get<double>(data_);
}

std::int64_t Value::asInt() const { return static_cast<std::int64_t>(asNumber()); }

const std::string& Value::asString() const& {
    if (!isString()) mismatch(Type::String, type());
    return std::get<std::string>(data_);
}

std::string Value::asString() && {
    if (!isString()) mismatch(Type::String, type());
    return std::move(std::get<std::string>(data_));
}

const Array& Value::asArray() const& {
    if (!isArray()) mismatch(Type::Array, type());
    return std::get<Array>(data_);
}

Array& Value::asArray() & {
    if (!isArray()) mismatch(Type::Array, type());
    return std::get<Array>(data_);
}

Array Value::asArray() && {
    if (!isArray()) mismatch(Type::Array, type());
    return std::move(std::get<Array>(data_));
}

const Object& Value::asObject() const& {
    if (!isObject()) mismatch(Type::Object, type());
    return std::get<Object>(data_);
}

Object& Value::asObject() & {
    if (!isObject()) mismatch(Type::Object, type());
    return std::get<Object>(data_);
}

Object Value::asObject() && {
    if (!isObject()) mismatch(Type::Object, type());
    return std::move(std::get<Object>(data_));
}

Value& Value::operator[](std::string_view key) {
    if (isNull()) data_ = Object{};
    Object& o = asObject();
    auto it = o.find(key);
    if (it == o.end()) it = o.emplace(std::string(key), Value{}).first;
    return it->second;
}

const Value& Value::operator[](std::string_view key) const {
    if (!isObject()) return nullValue();
    const Object& o = std::get<Object>(data_);
    auto it = o.find(key);
    return it == o.end() ? nullValue() : it->second;
}

bool Value::contains(std::string_view key) const {
    return isObject() && std::get<Object>(data_).contains(key);
}

Value& Value::operator[](std::size_t index) {
    Array& a = asArray();
    if (index >= a.size()) throw Error("json: index " + std::to_string(index) + " out of range");
    return a[index];
}

const Value& Value::operator[](std::size_t index) const {
    if (!isArray()) return nullValue();
    const Array& a = std::get<Array>(data_);
    return index < a.size() ? a[index] : nullValue();
}

void Value::push(Value v) {
    if (isNull()) data_ = Array{};
    asArray().push_back(std::move(v));
}

std::size_t Value::size() const {
    if (isArray()) return std::get<Array>(data_).size();
    if (isObject()) return std::get<Object>(data_).size();
    return 0;
}

std::string quote(std::string_view s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    static const char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(c >> 4) & 0xF];
                    out += hex[c & 0xF];
                } else {
                    out += c;  // UTF-8 passes through
                }
        }
    }
    return out + "\"";
}

void Value::write(std::string& out, int indent, int depth) const {
    auto newline = [&](int d) {
        if (indent <= 0) return;
        out += '\n';
        out.append(static_cast<std::size_t>(indent * d), ' ');
    };
    switch (type()) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += std::get<bool>(data_) ? "true" : "false"; break;
        case Type::Number: writeNumber(out, std::get<double>(data_)); break;
        case Type::String: out += quote(std::get<std::string>(data_)); break;
        case Type::Array: {
            const Array& a = std::get<Array>(data_);
            out += '[';
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (i) out += ',';
                newline(depth + 1);
                a[i].write(out, indent, depth + 1);
            }
            if (!a.empty()) newline(depth);
            out += ']';
            break;
        }
        case Type::Object: {
            const Object& o = std::get<Object>(data_);
            out += '{';
            bool first = true;
            for (const auto& [k, v] : o) {
                if (!first) out += ',';
                first = false;
                newline(depth + 1);
                out += quote(k);
                out += indent > 0 ? ": " : ":";
                v.write(out, indent, depth + 1);
            }
            if (!o.empty()) newline(depth);
            out += '}';
            break;
        }
    }
}

std::string Value::dump(int indent) const {
    std::string out;
    write(out, indent, 0);
    return out;
}

Value object(std::initializer_list<std::pair<const std::string, Value>> members) {
    return Value(Object(members.begin(), members.end()));
}

Value array(std::initializer_list<Value> items) { return Value(Array(items)); }

Value parse(std::string_view text) { return Parser(text).document(); }

std::expected<Value, Error> tryParse(std::string_view text) {
    try {
        return Parser(text).document();
    } catch (const Error& e) {
        return std::unexpected(e);
    }
}

}  // namespace rx::json
