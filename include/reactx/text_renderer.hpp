#pragma once

#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "reactx/renderer.hpp"

namespace rx {

// Backend that keeps the tree in memory and prints it as text.
// Useful for debugging and tests. Pass a stream to log every operation.
class TextRenderer : public Renderer {
public:
    explicit TextRenderer(std::ostream* log = nullptr);

    NodeHandle rootNode() override { return kRoot; }
    NodeHandle createElement(std::string_view type) override;
    NodeHandle createText(std::string_view text) override;
    void setText(NodeHandle node, std::string_view text) override;
    void setProp(NodeHandle node, std::string_view name, const PropValue& value) override;
    void removeProp(NodeHandle node, std::string_view name) override;
    void insertChild(NodeHandle parent, NodeHandle child, std::size_t index) override;
    void removeChild(NodeHandle parent, NodeHandle child) override;
    void destroyNode(NodeHandle node) override;
    void commit() override;

    // Whole tree as XML-like text.
    std::string toString() const;
    // First node whose prop `name` equals `value`.
    std::optional<NodeHandle> findByProp(const std::string& name, const std::string& value) const;
    std::size_t nodeCount() const { return nodes_.size(); }
    std::size_t operationCount() const { return ops_; }

protected:
    static constexpr NodeHandle kRoot = 1;

    struct Node {
        std::string type;  // empty for text nodes
        std::string text;
        bool isText = false;
        std::map<std::string, std::string, std::less<>> props;
        std::vector<NodeHandle> children;
        NodeHandle parent = 0;
    };

    Node& node(NodeHandle h);
    // nullptr if the node does not exist (for subclasses that read the tree).
    const Node* findNode(NodeHandle h) const;

private:
    void print(std::ostream& out, NodeHandle h, int depth) const;
    void log(const std::string& line);

protected:
    std::unordered_map<NodeHandle, Node> nodes_;

private:
    NodeHandle next_ = kRoot + 1;
    std::ostream* log_;
    std::size_t ops_ = 0;
};

}  // namespace rx
