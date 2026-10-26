#pragma once

#include <cstdint>
#include <flat_map>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rx {

struct Element;
class Props;

namespace detail {

// Handler with a stable identity, made by useCallback. Two handlers that wrap
// the same StableFn compare equal, which lets memo() skip re-renders.
template <class Sig>
struct StableFn;

template <class R, class... A>
struct StableFn<R(A...)> {
    std::shared_ptr<std::function<R(A...)>> impl;
    R operator()(A... args) const { return (*impl)(std::forward<A>(args)...); }
};

// Value given by a context provider (see context.hpp).
struct ContextValueBase {
    const void* id = nullptr;  // which context
    virtual ~ContextValueBase() = default;
    virtual bool equals(const ContextValueBase& other) const = 0;
};

}  // namespace detail

// A key press reported by a backend. `key` is a printable character ("a")
// or a name ("Enter", "Backspace", "Up", "Tab", "Escape", ...).
struct KeyEvent {
    std::string key;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    bool operator==(const KeyEvent&) const = default;
};

// Event handlers stored in props. They stay in the core and are never sent
// to the renderer.
using Callback = std::function<void()>;                         // onClick, onFocus, ...
using KeyHandler = std::function<void(const KeyEvent&)>;        // onKey
using InputHandler = std::function<void(const std::string&)>;   // onInput, onChange

// Data a backend sends with an event: nothing, a key press, or text.
using EventData = std::variant<std::monostate, KeyEvent, std::string>;

using PropValue = std::variant<std::monostate, bool, std::int64_t, double, std::string,
                               Callback, KeyHandler, InputHandler>;

// True for any event handler type (Callback, KeyHandler, InputHandler).
bool isHandler(const PropValue& value);

// A component is a plain function: props in, element tree out.
using Component = Element (*)(const Props&);

using PropList = std::initializer_list<std::pair<std::string, PropValue>>;

// Handlers are equal only if both come from the same useCallback (same
// identity); other std::functions can't be compared.
bool propEquals(const PropValue& a, const PropValue& b);
std::string propToString(const PropValue& value);

class Props {
public:
    Props() = default;
    Props(PropList list);

    bool has(std::string_view name) const { return values_.contains(name); }
    const PropValue* find(std::string_view name) const;

    // Typed read with a fallback: get<int>("start", 0), get<std::string>("title").
    template <class T>
    T get(std::string_view name, T fallback = T{}) const;
    std::string get(std::string_view name, const char* fallback) const {
        return get<std::string>(name, fallback);
    }

    void set(std::string name, PropValue value);
    // Sorted by name. A flat map (C++23): props are few, so one sorted array
    // beats a tree of nodes for lookups and copies.
    using Values = std::flat_map<std::string, PropValue, std::less<>>;
    const Values& values() const { return values_; }

    const std::vector<Element>& children() const { return children_; }
    void setChildren(std::vector<Element> children);
    std::vector<Element> takeChildren();

private:
    Values values_;
    std::vector<Element> children_;
};

enum class ElementKind { Host, Text, Component, Fragment, Provider };

// Immutable description of UI, like a React element.
struct Element {
    ElementKind kind = ElementKind::Fragment;
    std::string type;  // host tag ("box") or text content
    Component component = nullptr;
    Props props;       // children live in props.children()
    std::string key;
    std::shared_ptr<const detail::ContextValueBase> provided;  // Provider only

    Element() = default;
    Element(const char* text);  // implicit: lets you write {"Hello"} as a child
    Element(std::string text);
};

// h("box", {{"id", "main"}}, {child, child})
Element h(std::string type, PropList props = {}, std::vector<Element> children = {});
// h(Counter, {{"start", 5}})
Element h(Component component, PropList props = {}, std::vector<Element> children = {});
Element text(std::string content);
Element fragment(std::vector<Element> children, std::string key = {});

// ---- template implementation ----

template <class T>
T Props::get(std::string_view name, T fallback) const {
    const PropValue* v = find(name);
    if (!v) return fallback;
    if constexpr (std::is_same_v<T, bool>) {
        if (auto p = std::get_if<bool>(v)) return *p;
    } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
        if (auto p = std::get_if<std::int64_t>(v)) return static_cast<T>(*p);
        if (auto p = std::get_if<double>(v)) return static_cast<T>(*p);
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (auto p = std::get_if<std::string>(v)) return *p;
    } else if constexpr (std::is_same_v<T, Callback> || std::is_same_v<T, KeyHandler> ||
                         std::is_same_v<T, InputHandler>) {
        if (auto p = std::get_if<T>(v)) return *p;
    } else {
        static_assert(sizeof(T) == 0, "Props::get: unsupported type");
    }
    return fallback;
}

}  // namespace rx
