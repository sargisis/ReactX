// reactx::forms — validation rules and schemas for forms (with ui::useForm)
// and for JSON from a client (on a server, see reactx::api).
//
//   struct Signup { std::string email, password, confirm; bool terms = false; };
//
//   const auto signup = rx::forms::Schema<Signup>()
//       .field("email", &Signup::email, {rx::forms::required(), rx::forms::email()})
//       .field("password", &Signup::password, {rx::forms::minLength(8)})
//       .check("confirm", [](const Signup& s) { return s.confirm == s.password ? "" : "The passwords differ"; })
//       .field("terms", &Signup::terms, {rx::forms::accepted()});
//
//   auto form = ui::useForm<Signup>({.validate = signup.validator(), .onSubmit = send});
//
// Comes with ReactX (library/forms); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::forms)
#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/json.hpp"

namespace rx::forms {

// field name -> message; the same type as ui::FormErrors.
using Errors = std::map<std::string, std::string>;

// A rule: "" when the value is fine, otherwise the message to show.
template <class V>
using Rule = std::function<std::string(const V&)>;

// The default messages, in English; {n} is the number of the rule. Change them
// once at start (another language, your own wording): rx::forms::messages().required = "Обязательное поле";
struct Messages {
    std::string required = "Required";
    std::string minLength = "At least {n} characters";
    std::string maxLength = "At most {n} characters";
    std::string email = "Not an email address";
    std::string url = "Not a web address (https://...)";
    std::string digits = "Only digits";
    std::string min = "At least {n}";
    std::string max = "At most {n}";
    std::string oneOf = "Not one of the choices";
    std::string accepted = "Must be accepted";
    std::string notText = "Must be text";
    std::string notNumber = "Must be a number";
    std::string notWhole = "Must be a whole number";
    std::string notBool = "Must be true or false";
    std::string notObject = "Must be a JSON object";
};
Messages& messages();

// ---- rules for text (message "" = the default from messages()) ----

Rule<std::string> required(std::string message = {});       // not empty (spaces don't count)
Rule<std::string> minLength(std::size_t n, std::string message = {});  // characters, not bytes
Rule<std::string> maxLength(std::size_t n, std::string message = {});
Rule<std::string> email(std::string message = {});          // name@host.domain, no spaces
Rule<std::string> url(std::string message = {});            // http:// or https:// and a host
Rule<std::string> digits(std::string message = {});         // 0-9 only (a code, a phone without +)
Rule<std::string> oneOf(std::vector<std::string> choices, std::string message = {});

// The rules below check text only when there is some: pair them with
// required() to make a field mandatory.

// ---- rules for numbers and checkboxes ----

std::string formatLimit(double n);  // "8", "2.5": the {n} in messages

template <class V>
    requires(std::integral<V> || std::floating_point<V>) && (!std::same_as<V, bool>)
Rule<V> min(V limit, std::string message = {}) {
    return [limit, message = std::move(message)](const V& v) -> std::string {
        if (v >= limit) return {};
        std::string m = message.empty() ? messages().min : message;
        if (auto at = m.find("{n}"); at != std::string::npos) m.replace(at, 3, formatLimit(static_cast<double>(limit)));
        return m;
    };
}
template <class V>
    requires(std::integral<V> || std::floating_point<V>) && (!std::same_as<V, bool>)
Rule<V> max(V limit, std::string message = {}) {
    return [limit, message = std::move(message)](const V& v) -> std::string {
        if (v <= limit) return {};
        std::string m = message.empty() ? messages().max : message;
        if (auto at = m.find("{n}"); at != std::string::npos) m.replace(at, 3, formatLimit(static_cast<double>(limit)));
        return m;
    };
}
Rule<bool> accepted(std::string message = {});  // a checkbox that must be ticked ("I agree")

// Any rule of your own: forms::rule<std::string>([](auto& s) { return !s.contains(' '); }, "No spaces")
template <class V, class F>
    requires std::is_invocable_r_v<bool, F, const V&>
Rule<V> rule(F ok, std::string message) {
    return [ok = std::move(ok), message = std::move(message)](const V& v) -> std::string { return ok(v) ? "" : message; };
}

// ---- a schema for a struct ----

template <class T>
class Schema {
public:
    // The rules of one field, checked in order: the first message is the field's error.
    template <class V>
    Schema& field(std::string name, V T::* member, std::initializer_list<Rule<V>> rules) {
        std::vector<Rule<V>> list(rules);
        checks_.push_back({std::move(name), [member, list = std::move(list)](const T& value) -> std::string {
                               for (const auto& r : list)
                                   if (std::string m = r(value.*member); !m.empty()) return m;
                               return {};
                           }});
        return *this;
    }
    // A check that needs several fields ("confirm" == "password"); its message
    // goes to `name`. Runs after the field rules, only if `name` has no error yet.
    Schema& check(std::string name, std::function<std::string(const T&)> rule) {
        checks_.push_back({std::move(name), std::move(rule)});
        return *this;
    }

    Errors validate(const T& value) const {
        Errors errors;
        for (const auto& c : checks_) {
            if (errors.contains(c.name)) continue;
            if (std::string m = c.rule(value); !m.empty()) errors.emplace(c.name, std::move(m));
        }
        return errors;
    }
    bool valid(const T& value) const { return validate(value).empty(); }
    // For ui::useForm: {.validate = schema.validator()}.
    std::function<Errors(const T&)> validator() const {
        return [self = *this](const T& value) { return self.validate(value); };
    }

private:
    struct Check {
        std::string name;
        std::function<std::string(const T&)> rule;
    };
    std::vector<Check> checks_;
};

// ---- a schema for JSON (a request body on a server, a message from a socket) ----

class JsonSchema {
public:
    // A text field; `optional` lets it be missing or null (rules then don't run).
    JsonSchema& text(std::string name, std::initializer_list<Rule<std::string>> rules = {}, bool optional = false);
    // A number; `whole` refuses 2.5.
    JsonSchema& number(std::string name, std::initializer_list<Rule<double>> rules = {}, bool optional = false,
                       bool whole = false);
    JsonSchema& boolean(std::string name, std::initializer_list<Rule<bool>> rules = {}, bool optional = false);
    // Anything else: your own check of the whole object; its message goes to `name`.
    JsonSchema& check(std::string name, std::function<std::string(const json::Value&)> rule);

    // Errors by field name: a missing or wrong-typed field, then the rules.
    // Not an object at all: {"": "Must be a JSON object"}.
    Errors validate(const json::Value& value) const;

    // No fields and no checks: validate() then only checks that it is an object.
    bool empty() const { return checks_.empty(); }

    // Errors as JSON, for an HTTP 422 answer: {"errors": {"email": "Not an email address"}}.
    static json::Value toJson(const Errors& errors);

private:
    struct Check {
        std::string name;
        std::function<std::string(const json::Value&)> rule;
    };
    std::vector<Check> checks_;
};

}  // namespace rx::forms
