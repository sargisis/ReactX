// C++20 module for ReactX: `import reactx;` instead of the headers.
// The headers stay the source of truth; this file only re-exports them.
module;

#include "reactx/contract.hpp"
#include "reactx/fetch.hpp"
#include "reactx/html.hpp"
#include "reactx/json.hpp"
#include "reactx/path.hpp"
#include "reactx/reactx.hpp"
#include "reactx/router.hpp"
#include "reactx/terminal.hpp"
#include "reactx/text_renderer.hpp"
#include "reactx/ui.hpp"
#include "reactx/version.hpp"
#include "reactx/websocket.hpp"

export module reactx;

export namespace rx {

// version.hpp (the REACTX_VERSION macro needs the header; modules don't export macros)
using rx::version;

// element.hpp
using rx::KeyEvent;
using rx::Callback;
using rx::KeyHandler;
using rx::InputHandler;
using rx::ScrollHandler;
using rx::RefHandler;
using rx::PointerHandler;
using rx::PointerEvent;
using rx::NodeRef;
using rx::ScrollEvent;
using rx::EventData;
using rx::PropValue;
using rx::Component;
using rx::PropList;
using rx::Props;
using rx::ElementKind;
using rx::Element;
using rx::isHandler;
using rx::boolHandler;
using rx::propEquals;
using rx::propToString;
using rx::h;
using rx::text;
using rx::fragment;
using rx::RenderError;
using rx::ErrorFallback;
using rx::errorBoundary;

// hooks.hpp
using rx::Cleanup;
using rx::Setter;
using rx::EqualityComparable;
using rx::useState;
using rx::useRef;
using rx::useNodeRef;
using rx::useEffect;
using rx::useMemo;
using rx::useCallback;
using rx::useDebounce;
using rx::useLocalStorage;
using rx::StorageErrorHandler;
using rx::storageRead;
using rx::storageWrite;
using rx::storageRemove;
using rx::useWindowSize;
using rx::useTitle;
using rx::WindowSize;
using rx::deps;
using rx::useReducer;
using rx::Dispatch;
using rx::useTimeout;
using rx::useInterval;
using rx::useKeyboard;
using rx::useId;

// store.hpp
using rx::Store;
using rx::createStore;
using rx::useStore;

// context.hpp, memo.hpp
using rx::Context;
using rx::createContext;
using rx::useContext;
using rx::memo;

// renderer.hpp, root.hpp, text_renderer.hpp
using rx::NodeHandle;
using rx::HtmlRenderer;
using rx::Renderer;
using rx::renderToString;
using rx::Root;
using rx::TextRenderer;

// fetch.hpp, path.hpp
using rx::FetchRequest;
using rx::FetchHeaders;
using rx::FetchHandle;
using rx::FetchResponse;
using rx::FetchState;
using rx::fetch;
using rx::useFetch;
using rx::LocalFetch;
using rx::setLocalFetch;
using rx::matchPath;
using rx::urlDecode;
using rx::urlEncode;
using rx::queryParam;

// websocket.hpp
using rx::WebSocketStatus;
using rx::WebSocketOptions;
using rx::WebSocketState;
using rx::useWebSocket;

// terminal.hpp
using rx::decodeKeys;
using rx::Terminal;
using rx::TerminalApp;
using rx::TerminalRenderer;

}  // namespace rx

// json.hpp
export namespace rx::json {
using rx::json::Value;
using rx::json::Array;
using rx::json::Object;
using rx::json::Error;
using rx::json::Type;
using rx::json::object;
using rx::json::array;
using rx::json::parse;
using rx::json::quote;
}  // namespace rx::json

// router.hpp
export namespace rx::router {
using rx::router::Params;
using rx::router::Route;
using rx::router::LinkProps;
using rx::router::routes;
using rx::router::link;
using rx::router::redirect;
using rx::router::navigate;
using rx::router::back;
using rx::router::currentPath;
using rx::router::reset;
using rx::router::base;
using rx::router::setBase;
using rx::router::href;
using rx::router::useLocation;
using rx::router::useParams;
using rx::router::useParam;
using rx::router::useQuery;
}  // namespace rx::router

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
using rx::ui::Text;
using rx::ui::Card;
using rx::ui::Stack;
using rx::ui::Select;
using rx::ui::Tooltip;
using rx::ui::Confirm;
using rx::ui::Toaster;

using rx::ui::ButtonProps;
using rx::ui::InputProps;
using rx::ui::CheckboxProps;
using rx::ui::Tab;
using rx::ui::TabsProps;
using rx::ui::ModalProps;
using rx::ui::ListItemProps;
using rx::ui::ListProps;
using rx::ui::TextProps;
using rx::ui::CardProps;
using rx::ui::Direction;
using rx::ui::Align;
using rx::ui::Justify;
using rx::ui::StackProps;
using rx::ui::Option;
using rx::ui::SelectProps;
using rx::ui::TooltipProps;
using rx::ui::ConfirmProps;
using rx::ui::ToastProps;
using rx::ui::VirtualListOptions;
using rx::ui::VirtualList;
using rx::ui::useVirtualList;
using rx::ui::TableColumn;
using rx::ui::TableRow;
using rx::ui::TableProps;
using rx::ui::table;
using rx::ui::TextareaProps;
using rx::ui::textarea;
using rx::ui::RadioGroupProps;
using rx::ui::radioGroup;
using rx::ui::SliderProps;
using rx::ui::slider;
using rx::ui::ProgressProps;
using rx::ui::progress;
using rx::ui::FormErrors;
using rx::ui::FormOptions;
using rx::ui::Form;
using rx::ui::useForm;

using rx::ui::button;
using rx::ui::input;
using rx::ui::checkbox;
using rx::ui::tabs;
using rx::ui::modal;
using rx::ui::listItem;
using rx::ui::list;
using rx::ui::text;
using rx::ui::card;
using rx::ui::stack;
using rx::ui::select;
using rx::ui::tooltip;
using rx::ui::confirm;
using rx::ui::toast;
using rx::ui::toaster;

}  // namespace rx::ui

export namespace rx::dsl {
using rx::dsl::TagBuilder;
using rx::dsl::operator""_tag;
namespace tag {
using namespace rx::dsl::tag;
}
}  // namespace rx::dsl
