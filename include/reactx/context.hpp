#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "reactx/element.hpp"
#include "reactx/hooks.hpp"

namespace rx {

namespace detail {

template <class T>
struct ContextValue : ContextValueBase {
    T value;
    ContextValue(const void* contextId, T v) : value(std::move(v)) { id = contextId; }
    bool equals(const ContextValueBase& other) const override {
        if constexpr (std::equality_comparable<T>) {
            auto* o = dynamic_cast<const ContextValue*>(&other);
            return o && o->value == value;
        } else {
            return false;  // can't compare: treat every new value as a change
        }
    }
};

}  // namespace detail

// A value passed down the tree without threading it through props.
//
//   const auto Theme = createContext<std::string>("light");
//   Theme.provide("dark", {h(Toolbar)})        // in a parent
//   const std::string& theme = useContext(Theme);  // in any component below
//
// When the provided value changes, every component that reads it re-renders,
// even if a memo() component between them skipped its own render.
template <class T>
class Context {
public:
    explicit Context(T defaultValue)
        : token_(std::make_shared<char>()), default_(std::move(defaultValue)) {}

    Element provide(T value, std::vector<Element> children) const {
        Element e;
        e.kind = ElementKind::Provider;
        e.provided = std::make_shared<const detail::ContextValue<T>>(id(), std::move(value));
        e.props.setChildren(std::move(children));
        return e;
    }

    const T& defaultValue() const { return default_; }
    const void* id() const { return token_.get(); }

private:
    std::shared_ptr<char> token_;  // copies of a Context share the identity
    T default_;
};

template <class T>
Context<T> createContext(T defaultValue) {
    return Context<T>(std::move(defaultValue));
}

inline Context<std::string> createContext(const char* defaultValue) {
    return Context<std::string>(defaultValue);
}

// Value from the nearest provider above, or the context's default value.
// The reference stays valid until the provider renders again; copy it if you
// need it longer (e.g. inside an effect).
template <class T>
const T& useContext(const Context<T>& context) {
    const detail::ContextValueBase* found = detail::readContext(context.id());
    if (!found) return context.defaultValue();
    return static_cast<const detail::ContextValue<T>*>(found)->value;
}

}  // namespace rx
