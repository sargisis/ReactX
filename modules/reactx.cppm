// C++20 module for ReactX: `import reactx;` instead of the headers.
// The headers stay the source of truth; this file only re-exports them.
module;

#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"
#include "reactx/text_renderer.hpp"

export module reactx;

export namespace rx {

// element.hpp
using rx::KeyEvent;
using rx::Callback;
using rx::KeyHandler;
using rx::InputHandler;
using rx::EventData;
using rx::PropValue;
using rx::Component;
using rx::PropList;
using rx::Props;
using rx::ElementKind;
using rx::Element;
using rx::isHandler;
using rx::propEquals;
using rx::propToString;
using rx::h;
using rx::text;
using rx::fragment;

// hooks.hpp
using rx::Cleanup;
using rx::Setter;
using rx::useState;
using rx::useRef;
using rx::useEffect;
using rx::deps;

// renderer.hpp, root.hpp, text_renderer.hpp
using rx::NodeHandle;
using rx::Renderer;
using rx::Root;
using rx::TextRenderer;

// terminal.hpp
using rx::decodeKeys;
using rx::Terminal;
using rx::TerminalApp;
using rx::TerminalRenderer;

}  // namespace rx
