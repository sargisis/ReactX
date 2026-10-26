#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/element.hpp"

namespace rx::dsl {

namespace detail {

inline void collectChildren(std::vector<Element>&) {}

template <class First, class... Rest>
void collectChildren(std::vector<Element>& out, First&& first, Rest&&... rest) {
    if constexpr (std::convertible_to<First, Element>) {
        out.push_back(Element(std::forward<First>(first)));
    } else if constexpr (std::same_as<std::decay_t<First>, std::vector<Element>>) {
        for (auto& e : first) out.push_back(std::move(e));
    } else if constexpr (std::convertible_to<First, std::string_view>) {
        out.push_back(Element(std::string(std::forward<First>(first))));
    } else {
        out.push_back(Element(std::to_string(std::forward<First>(first))));
    }
    collectChildren(out, std::forward<Rest>(rest)...);
}

}  // namespace detail

// Fluent tag builder:
//   tag::div.cls("card").id("c1")("Hello, world!", tag::button("Click me"))
class TagBuilder {
public:
    constexpr explicit TagBuilder(std::string_view tag) : tag_(tag) {}

    TagBuilder cls(std::string_view className) const {
        TagBuilder copy = *this;
        copy.props_.set("className", std::string(className));
        return copy;
    }

    TagBuilder id(std::string_view idVal) const {
        TagBuilder copy = *this;
        copy.props_.set("id", std::string(idVal));
        return copy;
    }

    TagBuilder key(std::string_view keyVal) const {
        TagBuilder copy = *this;
        copy.key_ = std::string(keyVal);
        return copy;
    }

    TagBuilder prop(std::string_view name, PropValue val) const {
        TagBuilder copy = *this;
        copy.props_.set(std::string(name), std::move(val));
        return copy;
    }

    TagBuilder onClick(Callback cb) const {
        TagBuilder copy = *this;
        copy.props_.set("onClick", std::move(cb));
        return copy;
    }

    TagBuilder onInput(InputHandler ih) const {
        TagBuilder copy = *this;
        copy.props_.set("onInput", std::move(ih));
        return copy;
    }

    TagBuilder onKey(KeyHandler kh) const {
        TagBuilder copy = *this;
        copy.props_.set("onKey", std::move(kh));
        return copy;
    }

    // Call operator creates an Element with children
    template <class... Args>
    Element operator()(Args&&... children) const {
        std::vector<Element> outChildren;
        outChildren.reserve(sizeof...(children));
        detail::collectChildren(outChildren, std::forward<Args>(children)...);
        
        Element el;
        el.kind = ElementKind::Host;
        el.type = std::string(tag_);
        el.props = props_;
        el.props.setChildren(std::move(outChildren));
        if (!key_.empty()) el.key = key_;
        return el;
    }

    // Implicit conversion to Element when passed with no children: tag::br
    operator Element() const {
        Element el;
        el.kind = ElementKind::Host;
        el.type = std::string(tag_);
        el.props = props_;
        if (!key_.empty()) el.key = key_;
        return el;
    }

private:
    std::string_view tag_;
    Props props_{};
    std::string key_{};
};

// User-defined literal: "my_custom_tag"_tag.cls("fancy")(...)
constexpr TagBuilder operator""_tag(const char* str, std::size_t len) {
    return TagBuilder(std::string_view(str, len));
}

// Common web & terminal tags predefined for convenience
namespace tag {
inline constexpr TagBuilder div{"div"};
inline constexpr TagBuilder span{"span"};
inline constexpr TagBuilder p{"p"};
inline constexpr TagBuilder h1{"h1"};
inline constexpr TagBuilder h2{"h2"};
inline constexpr TagBuilder h3{"h3"};
inline constexpr TagBuilder button{"button"};
inline constexpr TagBuilder input{"input"};
inline constexpr TagBuilder a{"a"};
inline constexpr TagBuilder ul{"ul"};
inline constexpr TagBuilder li{"li"};
inline constexpr TagBuilder header{"header"};
inline constexpr TagBuilder footer{"footer"};
inline constexpr TagBuilder nav{"nav"};
inline constexpr TagBuilder section{"section"};
inline constexpr TagBuilder article{"article"};
inline constexpr TagBuilder aside{"aside"};
inline constexpr TagBuilder main{"main"};
inline constexpr TagBuilder code{"code"};
inline constexpr TagBuilder pre{"pre"};
inline constexpr TagBuilder small{"small"};
inline constexpr TagBuilder b{"b"};
inline constexpr TagBuilder em{"em"};

// Terminal tags
inline constexpr TagBuilder box{"box"};
inline constexpr TagBuilder text{"text"};
}  // namespace tag

}  // namespace rx::dsl
