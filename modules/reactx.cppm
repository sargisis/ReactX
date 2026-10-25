// C++20 module for ReactX: `import reactx;` instead of the headers.
// The headers stay the source of truth; this file only re-exports them.
module;

#include "reactx/reactx.hpp"
#include "reactx/terminal.hpp"
#include "reactx/text_renderer.hpp"
#include "reactx/ui.hpp"

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
using rx::useMemo;
using rx::useCallback;
using rx::deps;

// context.hpp, memo.hpp
using rx::Context;
using rx::createContext;
using rx::useContext;
using rx::memo;

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

// ui.hpp
export namespace rx::ui {

using rx::ui::Theme;
using rx::ui::ThemeContext;
using rx::ui::themeProvider;
using rx::ui::useTheme;
using rx::ui::Variant;

using rx::ui::Button;
using rx::ui::Input;
using rx::ui::Checkbox;
using rx::ui::Tabs;
using rx::ui::Modal;
using rx::ui::List;
using rx::ui::ListItem;

using rx::ui::ButtonProps;
using rx::ui::InputProps;
using rx::ui::CheckboxProps;
using rx::ui::Tab;
using rx::ui::TabsProps;
using rx::ui::ModalProps;
using rx::ui::ListItemProps;
using rx::ui::ListProps;

using rx::ui::button;
using rx::ui::input;
using rx::ui::checkbox;
using rx::ui::tabs;
using rx::ui::modal;
using rx::ui::listItem;
using rx::ui::list;

}  // namespace rx::ui
