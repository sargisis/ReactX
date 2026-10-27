#include "reactx/element.hpp"

#include "reactx/contract.hpp"

#include <concepts>
#include <format>

namespace rx {

bool isHandler(const PropValue& value) {
    return std::holds_alternative<Callback>(value) || std::holds_alternative<KeyHandler>(value) ||
           std::holds_alternative<InputHandler>(value) || std::holds_alternative<ScrollHandler>(value);
}

namespace {

template <class Sig>
bool sameStable(const std::function<Sig>& a, const std::function<Sig>& b) {
    auto* x = a.template target<detail::StableFn<Sig>>();
    auto* y = b.template target<detail::StableFn<Sig>>();
    return x && y && x->impl == y->impl;
}

}  // namespace

bool propEquals(const PropValue& a, const PropValue& b) {
    if (a.index() != b.index()) return false;
    return std::visit(
        [&](const auto& x) -> bool {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::equality_comparable<T>) return x == std::get<T>(b);
            else return sameStable(x, std::get<T>(b));  // handlers
        },
        a);
}

std::string propToString(const PropValue& value) {
    return std::visit(
        [](const auto& x) -> std::string {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::monostate>) return "null";
            else if constexpr (std::is_same_v<T, bool>) return x ? "true" : "false";
            else if constexpr (std::is_same_v<T, std::int64_t>) return std::to_string(x);
            else if constexpr (std::is_same_v<T, double>) {
                return std::format("{}", x);  // shortest form that reads back exactly
            } else if constexpr (std::is_same_v<T, std::string>) return x;
            else return "<handler>";
        },
        value);
}

Props::Props(PropList list) {
    for (const auto& [name, value] : list) values_.insert_or_assign(name, value);
}

const PropValue* Props::find(std::string_view name) const {
    auto it = values_.find(name);
    return it == values_.end() ? nullptr : &it->second;
}

void Props::set(std::string name, PropValue value) {
    values_.insert_or_assign(std::move(name), std::move(value));
}

void Props::setChildren(std::vector<Element> children) { children_ = std::move(children); }

std::vector<Element> Props::takeChildren() { return std::exchange(children_, {}); }

Element::Element(const char* text) : Element(std::string(text)) {}

Element::Element(std::string text) : kind(ElementKind::Text), type(std::move(text)) {}

namespace {

Element makeElement(ElementKind kind, PropList props, std::vector<Element> children) {
    Element e;
    e.kind = kind;
    for (const auto& [name, value] : props) {
        if (name == "key") e.key = propToString(value);
        else e.props.set(name, value);
    }
    e.props.setChildren(std::move(children));
    return e;
}

}  // namespace

Element h(std::string type, PropList props, std::vector<Element> children) {
    Element e = makeElement(ElementKind::Host, props, std::move(children));
    e.type = std::move(type);
    return e;
}

Element h(Component component, PropList props, std::vector<Element> children) {
    Element e = makeElement(ElementKind::Component, props, std::move(children));
    e.component = component;
    return e;
}

Element text(std::string content) { return Element(std::move(content)); }

Element errorBoundary(ErrorFallback fallback, std::vector<Element> children, std::string key) {
    REACTX_PRE(static_cast<bool>(fallback));
    Element e;
    e.kind = ElementKind::Boundary;
    e.key = std::move(key);
    e.fallback = std::make_shared<const ErrorFallback>(std::move(fallback));
    e.props.setChildren(std::move(children));
    return e;
}

Element fragment(std::vector<Element> children, std::string key) {
    Element e;
    e.kind = ElementKind::Fragment;
    e.key = std::move(key);
    e.props.setChildren(std::move(children));
    return e;
}

}  // namespace rx
