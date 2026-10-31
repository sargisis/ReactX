#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "reactx/element.hpp"

namespace rx {

// The backend interface. The core computes what changed and calls these
// methods; a backend (terminal, SDL, DOM, ...) only has to apply them.
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual NodeHandle rootNode() = 0;

    virtual NodeHandle createElement(std::string_view type) = 0;
    virtual NodeHandle createText(std::string_view text) = 0;
    virtual void setText(NodeHandle node, std::string_view text) = 0;

    // Only non-callback props are passed here.
    virtual void setProp(NodeHandle node, std::string_view name, const PropValue& value) = 0;
    virtual void removeProp(NodeHandle node, std::string_view name) = 0;

    // Place child at position `index` of parent. If it is already a child of
    // parent, move it there.
    virtual void insertChild(NodeHandle parent, NodeHandle child, std::size_t index) = 0;
    virtual void removeChild(NodeHandle parent, NodeHandle child) = 0;

    // Called for every node of an unmounted subtree, after it was detached.
    virtual void destroyNode(NodeHandle node) = 0;

    // Called once after each batch of changes.
    virtual void commit() {}

    // Move the keyboard focus to `node` (NodeRef::focus). False if the
    // backend has no focus or the node can't take it.
    virtual bool focus(NodeHandle) { return false; }
};

}  // namespace rx
