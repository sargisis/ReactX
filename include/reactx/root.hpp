#pragma once

#include <memory>
#include <string>

#include "reactx/element.hpp"
#include "reactx/renderer.hpp"

namespace rx {

namespace detail {
class Reconciler;
}

// Entry point: mounts an element tree into a renderer and keeps it updated.
class Root {
public:
    explicit Root(Renderer& renderer);
    ~Root();
    Root(const Root&) = delete("a Root owns the mounted tree: pass it by reference");
    Root& operator=(const Root&) = delete("a Root owns the mounted tree: pass it by reference");

    // Mount or update the whole tree.
    void render(Element element);

    // Re-render components whose state changed. Called automatically after
    // render() and dispatch(); call it yourself after setState from outside.
    void flush();

    // Fire an event handler prop of a host node, then flush.
    //   dispatch(n, "onClick")
    //   dispatch(n, "onKey", KeyEvent{"Enter"})
    //   dispatch(n, "onInput", std::string("hello"))
    // A Callback handler ignores the data. A KeyHandler needs a KeyEvent and
    // an InputHandler needs a string. Returns false if the node has no such
    // handler or the data does not match it.
    bool dispatch(NodeHandle node, const std::string& event, const EventData& data = {});

private:
    std::unique_ptr<detail::Reconciler> impl_;
};

}  // namespace rx
