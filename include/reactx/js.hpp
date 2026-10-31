#pragma once

// JavaScript from C++ in the browser build: the page's element behind a
// "ref", and anything else on the page through emscripten::val (embind).
//
//   auto canvas = useNodeRef();
//   useEffect([canvas] {
//       emscripten::val el = rx::domElement(canvas);
//       el.call<void>("focus");
//       int width = el["clientWidth"].as<int>();
//       emscripten::val chart = emscripten::val::global("Chart").new_(el, rx::jsValue(config));
//   }, deps());
//   return h("canvas", {{"ref", canvas}});
//
// Link the program with reactx::js (CMake: target_link_libraries(app PRIVATE
// reactx::js), it adds -lembind); apps that don't include this header don't
// pay for embind. Browser build only: natively there is no page.

#if !defined(__EMSCRIPTEN__)
#error "reactx/js.hpp works only in the browser build (Emscripten); keep its use under #ifdef __EMSCRIPTEN__"
#endif

#include <emscripten/val.h>

#include <string>

#include "reactx/element.hpp"
#include "reactx/json.hpp"

namespace rx {

// The DOM node the backend made for `node`; undefined for 0 or a removed node.
inline emscripten::val domElement(NodeHandle node) {
    if (node == 0) return emscripten::val::undefined();
    emscripten::val nodes = emscripten::val::module_property("rxNodes");
    if (nodes.isUndefined()) return emscripten::val::undefined();  // nothing mounted yet
    return nodes.call<emscripten::val>("get", static_cast<double>(node));
}

// The element behind a ref; undefined until the element is on the page.
inline emscripten::val domElement(const NodeRef& ref) { return domElement(ref.get()); }

// A JavaScript value from JSON: objects for a JS library's options.
//   rx::jsValue(json::object({{"type", "line"}, {"data", data}}))
inline emscripten::val jsValue(const json::Value& value) {
    return emscripten::val::global("JSON").call<emscripten::val>("parse", value.dump());
}

}  // namespace rx
