#pragma once

// HTML from components, without a browser: prerender a page at build time so
// it shows before the WebAssembly arrives (or serve it from a C++ server).
//
//   std::string page = rx::renderToString(h(Home));
//   // "<main class=\"home\"><h1>Hello</h1>...</main>"
//
// Props become attributes the way the DOM backend sets them (className ->
// class, htmlFor -> for, true -> a present attribute, false -> none, value of a
// <textarea> or <select> -> its content or selected <option>). Text is escaped;
// javascript: URLs are blocked like in the browser. Handlers are left out.
//
// Components render once and effects don't run (as in React's renderToString):
// the HTML shows the first state, and code in useEffect may use the page
// freely, even when the prerender runs under Node. mountDom later replaces the
// container's content with the live app (no hydration yet), so the markup must
// match what the app draws first for the switch to be invisible.

#include <cstddef>
#include <flat_map>
#include <string>
#include <string_view>
#include <vector>

#include "reactx/element.hpp"
#include "reactx/renderer.hpp"

namespace rx {

// The HTML of `element` (no <!doctype>, no <html>: the content of a container).
std::string renderToString(Element element);

// The backend behind renderToString: keeps the tree in memory and prints HTML.
// Use it with a Root to render, dispatch events, and print the result again.
class HtmlRenderer : public Renderer {
public:
    HtmlRenderer();

    NodeHandle rootNode() override { return kRoot; }
    NodeHandle createElement(std::string_view type) override;
    NodeHandle createText(std::string_view text) override;
    void setText(NodeHandle node, std::string_view text) override;
    void setProp(NodeHandle node, std::string_view name, const PropValue& value) override;
    void removeProp(NodeHandle node, std::string_view name) override;
    void insertChild(NodeHandle parent, NodeHandle child, std::size_t index) override;
    void removeChild(NodeHandle parent, NodeHandle child) override;
    void destroyNode(NodeHandle node) override;

    // The HTML of everything mounted into the root.
    std::string html() const;

private:
    static constexpr NodeHandle kRoot = 1;

    struct Node {
        std::string type{};  // empty for text
        std::string text{};
        bool alive = true;
        std::flat_map<std::string, PropValue, std::less<>> props{};
        std::vector<NodeHandle> children{};
        NodeHandle parent = 0;
    };

    Node& node(NodeHandle h);
    const Node& node(NodeHandle h) const;
    void print(std::string& out, NodeHandle h, std::string_view parentType, const std::string* selectValue) const;

    std::vector<Node> nodes_;  // index = handle - 1
};

}  // namespace rx
