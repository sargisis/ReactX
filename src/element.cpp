#include "reactx/element.hpp"
#include "reactx/renderer.hpp"

#include "reactx/contract.hpp"
#include "number_text.hpp"

#include <charconv>
#include <cstdlib>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include <concepts>

namespace rx {

namespace detail {

#if defined(__EMSCRIPTEN__)
EM_JS(char*, rx_js_number_text, (double x), { return stringToNewUTF8(String(x)); });
#endif

std::string numberText(double x) {
#if defined(__EMSCRIPTEN__)
    char* raw = rx_js_number_text(x);
    std::string text(raw);
    std::free(raw);
    return text;
#else
    char buf[32];
    auto [end, ec] = std::to_chars(buf, buf + sizeof buf, x);
    return ec == std::errc{} ? std::string(buf, end) : std::string("nan");
#endif
}

}  // namespace detail

bool NodeRef::focus() const {
    return state_ && state_->node && state_->renderer && state_->renderer->focus(state_->node);
}

bool isHandler(const PropValue& value) {
    return std::holds_alternative<Callback>(value) || std::holds_alternative<KeyHandler>(value) ||
           std::holds_alternative<InputHandler>(value) || std::holds_alternative<ScrollHandler>(value) ||
           std::holds_alternative<RefHandler>(value) || std::holds_alternative<PointerHandler>(value);
}

namespace {

template <class Sig>
bool sameStable(const std::function<Sig>& a, const std::function<Sig>& b) {
    auto* x = a.template target<detail::StableFn<Sig>>();
    auto* y = b.template target<detail::StableFn<Sig>>();
    return x && y && x->impl == y->impl;
}

// Two copies of one NodeRef are the same ref (useNodeRef returns a copy each render).
bool sameRef(const RefHandler& a, const RefHandler& b) {
    auto* x = a.target<NodeRef>();
    auto* y = b.target<NodeRef>();
    return x && y ? *x == *y : sameStable(a, b);
}

}  // namespace

bool propEquals(const PropValue& a, const PropValue& b) {
    if (a.index() != b.index()) return false;
    return std::visit(
        [&](const auto& x) -> bool {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::equality_comparable<T>) return x == std::get<T>(b);
            else if constexpr (std::is_same_v<T, RefHandler>) return sameRef(x, std::get<T>(b));
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
                return detail::numberText(x);  // shortest form that reads back exactly
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
