#include "reactx/text_renderer.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <stdexcept>

namespace rx {

TextRenderer::TextRenderer(std::ostream* log) : log_(log) {
    nodes_[kRoot].type = "root";
}

TextRenderer::Node& TextRenderer::node(NodeHandle h) {
    auto it = nodes_.find(h);
    if (it == nodes_.end()) throw std::logic_error("TextRenderer: unknown node #" + std::to_string(h));
    return it->second;
}

const TextRenderer::Node* TextRenderer::findNode(NodeHandle h) const {
    auto it = nodes_.find(h);
    return it == nodes_.end() ? nullptr : &it->second;
}

void TextRenderer::log(const std::string& line) {
    ++ops_;
    if (log_) *log_ << "  [op] " << line << '\n';
}

NodeHandle TextRenderer::createElement(std::string_view type) {
    NodeHandle h = next_++;
    nodes_[h].type = std::string(type);
    log("create <" + std::string(type) + "> #" + std::to_string(h));
    return h;
}

NodeHandle TextRenderer::createText(std::string_view text) {
    NodeHandle h = next_++;
    Node& n = nodes_[h];
    n.isText = true;
    n.text = std::string(text);
    log("create text \"" + n.text + "\" #" + std::to_string(h));
    return h;
}

void TextRenderer::setText(NodeHandle h, std::string_view text) {
    node(h).text = std::string(text);
    log("text #" + std::to_string(h) + " = \"" + std::string(text) + "\"");
}

void TextRenderer::setProp(NodeHandle h, std::string_view name, const PropValue& value) {
    node(h).props.insert_or_assign(std::string(name), propToString(value));
    log("set #" + std::to_string(h) + " " + std::string(name) + "=\"" + propToString(value) + "\"");
}

void TextRenderer::removeProp(NodeHandle h, std::string_view name) {
    auto& props = node(h).props;
    if (auto it = props.find(name); it != props.end()) props.erase(it);
    log("unset #" + std::to_string(h) + " " + std::string(name));
}

void TextRenderer::insertChild(NodeHandle parent, NodeHandle child, std::size_t index) {
    auto& kids = node(parent).children;
    if (auto it = std::ranges::find(kids, child); it != kids.end()) kids.erase(it);
    index = std::min(index, kids.size());
    kids.insert(kids.begin() + static_cast<std::ptrdiff_t>(index), child);
    node(child).parent = parent;
    log("insert #" + std::to_string(child) + " into #" + std::to_string(parent) + " at " +
        std::to_string(index));
}

void TextRenderer::removeChild(NodeHandle parent, NodeHandle child) {
    auto& kids = node(parent).children;
    if (auto it = std::ranges::find(kids, child); it != kids.end()) kids.erase(it);
    node(child).parent = 0;
    log("remove #" + std::to_string(child) + " from #" + std::to_string(parent));
}

void TextRenderer::destroyNode(NodeHandle h) {
    nodes_.erase(h);
    log("destroy #" + std::to_string(h));
}

void TextRenderer::commit() {
    if (log_) *log_ << "  [commit]\n";
}

void TextRenderer::print(std::string& out, NodeHandle h, int depth) const {
    const Node& n = nodes_.at(h);
    auto to = std::back_inserter(out);
    std::string indent = std::format("{:{}}", "", depth * 2);  // depth * 2 spaces
    if (n.isText) {
        std::format_to(to, "{}\"{}\"\n", indent, n.text);
        return;
    }
    std::format_to(to, "{}<{}", indent, n.type);
    for (const auto& [name, value] : n.props) std::format_to(to, " {}=\"{}\"", name, value);
    if (n.children.empty()) {
        out += "/>\n";
        return;
    }
    out += ">\n";
    for (NodeHandle c : n.children) print(out, c, depth + 1);
    std::format_to(to, "{}</{}>\n", indent, n.type);
}

std::string TextRenderer::toString() const {
    std::string out;
    print(out, kRoot, 0);
    return out;
}

std::optional<NodeHandle> TextRenderer::findByProp(const std::string& name,
                                                   const std::string& value) const {
    for (const auto& [h, n] : nodes_) {
        auto it = n.props.find(name);
        if (it != n.props.end() && it->second == value) return h;
    }
    return std::nullopt;
}

}  // namespace rx
