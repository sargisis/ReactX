#include "reactx/html.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <variant>

#include "reconciler.hpp"

namespace rx {

namespace {

// Elements that never have content or a closing tag.
bool isVoid(std::string_view tag) {
    static constexpr std::array<std::string_view, 13> kVoid = {
        "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"};
    return std::ranges::find(kVoid, tag) != kVoid.end();
}

// Raw text elements: their content is not HTML, entities would show as is.
bool isRawText(std::string_view tag) { return tag == "script" || tag == "style"; }

// The DOM backend's list: attributes that hold a URL.
bool isUrlAttr(std::string_view name) {
    static constexpr std::array<std::string_view, 7> kUrl = {"href",       "src",    "action", "formaction",
                                                              "xlink:href", "poster", "cite"};
    return std::ranges::find(kUrl, name) != kUrl.end();
}

// React's test, the same as in dom.cpp: browsers skip leading control
// characters and spaces, and tabs or newlines inside the scheme.
bool isJsUrl(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size() && static_cast<unsigned char>(s[i]) <= 32) ++i;
    constexpr std::string_view want = "javascript:";
    std::size_t k = 0;
    for (; i < s.size() && k < want.size(); ++i) {
        char c = s[i];
        if (c == '\t' || c == '\n' || c == '\r') continue;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != want[k]) return false;
        ++k;
    }
    return k == want.size();
}

void escape(std::string& out, std::string_view text, bool attribute) {
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"':
                if (attribute) out += "&quot;";
                else out += c;
                break;
            default: out += c;
        }
    }
}

// "svg:text" -> "text" (the DOM backend's spelling of SVG tags HTML also has).
std::string_view tagName(std::string_view type) {
    return type.starts_with("svg:") ? type.substr(4) : type;
}

// Text of a value prop as the DOM backend sets it: strings and numbers.
std::string valueText(const PropValue& v) {
    if (const auto* s = std::get_if<std::string>(&v)) return *s;
    if (std::holds_alternative<std::int64_t>(v) || std::holds_alternative<double>(v)) return propToString(v);
    return {};
}

}  // namespace

std::string renderToString(Element element) {
    HtmlRenderer renderer;
    detail::Reconciler reconciler(renderer);
    reconciler.skipEffects();
    reconciler.render(std::move(element));
    return renderer.html();
}

HtmlRenderer::HtmlRenderer() {
    nodes_.push_back(Node{.type = "#root"});
}

HtmlRenderer::Node& HtmlRenderer::node(NodeHandle h) {
    if (h == 0 || h > nodes_.size() || !nodes_[h - 1].alive)
        throw std::logic_error("HtmlRenderer: unknown node #" + std::to_string(h));
    return nodes_[h - 1];
}

const HtmlRenderer::Node& HtmlRenderer::node(NodeHandle h) const {
    return const_cast<HtmlRenderer*>(this)->node(h);
}

NodeHandle HtmlRenderer::createElement(std::string_view type) {
    nodes_.push_back(Node{.type = std::string(type)});
    return nodes_.size();
}

NodeHandle HtmlRenderer::createText(std::string_view text) {
    nodes_.push_back(Node{.text = std::string(text)});
    return nodes_.size();
}

void HtmlRenderer::setText(NodeHandle h, std::string_view text) { node(h).text = std::string(text); }

void HtmlRenderer::setProp(NodeHandle h, std::string_view name, const PropValue& value) {
    node(h).props.insert_or_assign(std::string(name), value);
}

void HtmlRenderer::removeProp(NodeHandle h, std::string_view name) {
    auto& props = node(h).props;
    if (auto it = props.find(name); it != props.end()) props.erase(it);
}

void HtmlRenderer::insertChild(NodeHandle parent, NodeHandle child, std::size_t index) {
    Node& c = node(child);
    if (c.parent) removeChild(c.parent, child);
    auto& kids = node(parent).children;
    kids.insert(kids.begin() + static_cast<std::ptrdiff_t>(std::min(index, kids.size())), child);
    c.parent = parent;
}

void HtmlRenderer::removeChild(NodeHandle parent, NodeHandle child) {
    std::erase(node(parent).children, child);
    node(child).parent = 0;
}

void HtmlRenderer::destroyNode(NodeHandle h) {
    Node& n = node(h);
    n = Node{};
    n.alive = false;
}

std::string HtmlRenderer::html() const {
    std::string out;
    for (NodeHandle c : node(kRoot).children) print(out, c, "", nullptr);
    return out;
}

void HtmlRenderer::print(std::string& out, NodeHandle h, std::string_view parentType,
                         const std::string* selectValue) const {
    const Node& n = node(h);
    if (n.type.empty()) {
        if (isRawText(parentType)) {
            // Inside <script>, "</" would end the element early.
            std::string_view t = n.text;
            for (std::size_t at; (at = t.find("</")) != std::string_view::npos; t.remove_prefix(at + 2)) {
                out.append(t.substr(0, at));
                out += parentType == "script" ? "<\\/" : "</";
            }
            out.append(t);
        } else {
            escape(out, n.text, false);
        }
        return;
    }

    std::string_view tag = tagName(n.type);
    std::string value;  // <textarea> content, or the <select> value its options compare with
    bool hasValue = false;

    out += '<';
    out += tag;
    for (const auto& [name, v] : n.props) {
        if (std::holds_alternative<std::monostate>(v)) continue;
        if (name == "value" && (tag == "textarea" || tag == "select")) {
            value = valueText(v);
            hasValue = true;
            continue;
        }
        std::string_view attr = name == "className" ? "class"
                                : name == "htmlFor" ? "for"
                                : name == "pointerCapture" ? "data-rx-capture"
                                                           : std::string_view(name);
        if (const bool* b = std::get_if<bool>(&v)) {
            if (*b) {
                out += ' ';
                out += attr;
            }
            continue;
        }
        if ((name == "checked" || name == "disabled" || name == "selected")) continue;  // only booleans count
        std::string text = valueText(v);
        if (isUrlAttr(attr) && isJsUrl(text)) text = "about:blank#blocked";
        out += ' ';
        out += attr;
        out += "=\"";
        escape(out, text, true);
        out += '"';
    }
    // An <option> the <select> value picks.
    if (tag == "option" && selectValue && !n.props.contains("selected")) {
        std::string mine;
        if (const PropValue* v = [&]() -> const PropValue* {
                auto it = n.props.find("value");
                return it == n.props.end() ? nullptr : &it->second;
            }())
            mine = valueText(*v);
        else
            for (NodeHandle c : n.children)
                if (node(c).type.empty()) mine += node(c).text;
        if (mine == *selectValue) out += " selected";
    }
    out += '>';
    if (isVoid(tag)) return;

    if (tag == "textarea" && hasValue) {
        escape(out, value, false);
    } else {
        const std::string* sel = tag == "select" ? (hasValue ? &value : nullptr)
                                : tag == "optgroup" ? selectValue
                                                    : nullptr;
        for (NodeHandle c : n.children) print(out, c, tag, sel);
    }
    out += "</";
    out += tag;
    out += '>';
}

}  // namespace rx
