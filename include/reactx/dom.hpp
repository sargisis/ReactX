#pragma once

// Browser backend (WebAssembly, built with Emscripten): renders into the real
// DOM. Tags are plain HTML tags ("div", "h1", "button", "input", ...), so CSS
// works as usual.
//
// Props:
//   className            -> class attribute
//   style                -> inline CSS text: {"style", "color: red; padding: 4px"}
//   value                -> input value (controlled: the page always shows
//                           the value from your state)
//   checked, disabled,
//   selected             -> DOM properties
//   htmlFor              -> for attribute
//   anything else        -> attribute (bool true = present, false = removed)
//
// Events (handled in C++, never sent to the page):
//   onClick   Callback                   click
//   onInput   InputHandler(text)         input / textarea / select typing
//   onChange  InputHandler or Callback   change; text is the value, or
//                                        "true"/"false" for checkboxes
//   onKey     KeyHandler(KeyEvent)       keydown; arrows are "Up", "Down",
//                                        "Left", "Right" like the terminal
//   onSubmit  Callback                   form submit (the page does not reload)
// Events bubble: if the target has no handler, its parent is tried, and so on.

#include <string>

#include "reactx/element.hpp"
#include "reactx/renderer.hpp"
#include "reactx/root.hpp"

namespace rx {

class DomRenderer : public Renderer {
public:
    // `container` is a CSS selector of an existing element, e.g. "#app".
    explicit DomRenderer(std::string container = "#app");

    NodeHandle rootNode() override { return root_; }
    NodeHandle createElement(std::string_view type) override;
    NodeHandle createText(std::string_view text) override;
    void setText(NodeHandle node, std::string_view text) override;
    void setProp(NodeHandle node, std::string_view name, const PropValue& value) override;
    void removeProp(NodeHandle node, std::string_view name) override;
    void insertChild(NodeHandle parent, NodeHandle child, std::size_t index) override;
    void removeChild(NodeHandle parent, NodeHandle child) override;
    void destroyNode(NodeHandle node) override;

private:
    NodeHandle root_;
};

// Mounts `element` into the page element matching `container` and keeps it
// running after main() returns (a web page has no main loop). Returns the Root,
// e.g. to call render() again later.
Root& mountDom(Element element, const std::string& container = "#app");

}  // namespace rx
