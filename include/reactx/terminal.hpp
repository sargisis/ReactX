#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "reactx/element.hpp"
#include "reactx/root.hpp"
#include "reactx/text_renderer.hpp"

namespace rx {

// ---- input ----

// Turns raw terminal bytes into key events: printable characters (UTF-8),
// "Enter", "Tab" (shift = Shift+Tab), "Backspace", "Delete", "Escape",
// "Up", "Down", "Left", "Right", "Home", "End", Ctrl+letter, Alt+key.
std::vector<KeyEvent> decodeKeys(std::string_view bytes);

// A mouse report (the terminal's SGR mode, which TerminalApp turns on):
// what happened and where, in cells counted from 0.
struct MouseInput {
    enum class Action { Down, Up, Move, WheelUp, WheelDown };
    Action action = Action::Move;
    int x = 0, y = 0;
    int button = 0;     // 0 left, 1 middle, 2 right (like the DOM)
    bool held = false;  // a button is down during this Move (a drag)
    bool ctrl = false, alt = false, shift = false;

    bool operator==(const MouseInput&) const = default;
};
using TerminalInput = std::variant<KeyEvent, MouseInput>;

// Keys and mouse reports, in the order they came.
std::vector<TerminalInput> decodeInput(std::string_view bytes);

// ---- rendering ----

// Backend that draws the tree in a terminal with ANSI escape codes.
// Layout is Flexbox, computed by Yoga (the engine behind React Native).
//
// Tags:
//   box (and any unknown tag)  flex container, column by default
//   text                       its text children; wraps at word boundaries
//                              (wrap=false truncates instead)
//   input                      one-line text field: value, placeholder
//   button                     "[ label ]"
//
// Flex props (like CSS, in terminal cells):
//   direction       row | column | row-reverse | column-reverse
//   justifyContent  start | center | end | space-between | space-around | space-evenly
//   alignItems      start | center | end | stretch     (alignSelf: same + auto)
//   flexGrow, flexShrink, flexBasis, flexWrap (wrap | nowrap)
//   width, height, minWidth, maxWidth, minHeight, maxHeight  (number or "50%")
//   padding, paddingX, paddingY, paddingLeft/Right/Top/Bottom
//   margin,  marginX,  marginY,  marginLeft/Right/Top/Bottom  (or "auto")
//   gap, rowGap, columnGap
//   position (absolute | relative | fixed), top, left, right, bottom
//     fixed: placed on the screen, not in its parent, and drawn over
//     everything else (modals, toasts), even when the content is taller
//     than the terminal
//   display (none | flex), overflow (visible | hidden | scroll; a border always clips)
//     scroll: a vertical scroll container. In a column it takes the room
//     that is left (flexGrow 1, flexShrink 1, flexBasis 0 unless set) and
//     its children keep their height; the wheel over it, PageUp / PageDown
//     and moving the focus scroll it; with a border the right edge shows
//     where you are. onScroll gets a ScrollEvent in rows.
//   opaque (bool): clear what is drawn under the box (modals)
//   trapFocus (bool): while such a node exists, Tab stays inside the last one;
//                     focus returns to where it was when it goes away
//   arrowKeys (bool): when focused, Up / Down go to its onKey instead of
//                     moving the focus (Left / Right always do)
// Look: border (bool), title, color, bg, bold, dim, underline.
// Colors: black red green yellow blue magenta cyan white gray, bright-<color>.
//
// The root fills the terminal. flexShrink defaults to 1 inside rows (text
// gets narrower) and 0 inside columns (what doesn't fit is cut off);
// buttons never shrink by default.
//
// input, button and nodes with focusable=true can take focus. The focused
// node is drawn in reverse video.
//
// Borders use box-drawing characters (┌─┐); setUnicode(false) switches to
// ASCII (+-|) for terminals whose locale isn't UTF-8 — TerminalApp decides
// with std::text_encoding (C++26).
class TerminalRenderer : public TextRenderer {
public:
    using Output = std::function<void(std::string_view)>;

    // Without an output the frame is only kept in memory (see frameText()).
    explicit TerminalRenderer(Output output = {});
    ~TerminalRenderer() override;

    void setSize(int width, int height);
    void setUnicode(bool on);
    bool unicode() const { return unicode_; }
    int width() const { return width_; }
    int height() const { return height_; }

    void commit() override;
    NodeHandle createElement(std::string_view type) override;
    void destroyNode(NodeHandle node) override;
    // scrollIntoView=true: the node is scrolled into view when it gets it.
    void setProp(NodeHandle node, std::string_view name, const PropValue& value) override;
    // The layout is kept between repaints; these tell it what changed.
    void setText(NodeHandle node, std::string_view text) override;
    void removeProp(NodeHandle node, std::string_view name) override;
    void insertChild(NodeHandle parent, NodeHandle child, std::size_t index) override;
    void removeChild(NodeHandle parent, NodeHandle child) override;

    // Lay out and draw now (also called by commit). Writes only changed lines.
    void repaint();
    // Forget what is on screen, so the next repaint redraws every line.
    void invalidate();

    // Last frame as plain text, one string per line, without colors.
    std::vector<std::string> frameLines() const;
    std::string frameText() const;

    // ---- focus ----
    NodeHandle focused() const { return focused_; }
    std::vector<NodeHandle> focusable() const;
    bool focus(NodeHandle node) override;
    void focusNext();
    void focusPrev();

    // Default key handling:
    //   Tab / Down -> next focusable, Shift+Tab / Up -> previous
    //   any key    -> "onKey" of the focused node, then of each ancestor
    //                 (bubbling, like the DOM)
    //   input      -> printable keys and Backspace edit "value" via "onInput"
    //   Enter / Space -> "onClick" of the focused node
    // Returns true if the key did something.
    // When focus moves this way, the node that lost it gets "onBlur" and the
    // one that got it "onFocus".
    bool handleKey(Root& root, const KeyEvent& key);

    // Mouse, like the browser: the topmost node under the pointer gets
    // onPointerDown / onPointerMove / onPointerUp (bubbling; PointerEvent in
    // cells), a press and release over it is an onClick, onMouseEnter /
    // onMouseLeave follow the pointer, and a press focuses the nearest
    // focusable node. After a press inside a node with pointerCapture=true
    // the moves and the release go to that node until the button is up.
    bool handleMouse(Root& root, const MouseInput& mouse);
    // The topmost node drawn at a cell in the last repaint, 0 if none.
    NodeHandle nodeAt(int x, int y) const;

    // ---- scrolling (overflow="scroll" boxes) ----
    // Scroll the boxes around `node` so it is visible (NodeRef::scrollIntoView).
    bool scrollIntoView(NodeHandle node) override;
    // An overflow="scroll" box's position and sizes in rows (NodeRef::scrollMetrics), and moving it.
    std::optional<ScrollEvent> scrollMetrics(NodeHandle box) override;
    bool scrollTo(NodeHandle box, int top) override;
    // How far a scroll box is scrolled, in rows (0 = top).
    int scrollTop(NodeHandle box) const;
    // Scroll a box by `rows` (negative = up), within its content. True if it moved.
    bool scrollBy(NodeHandle box, int rows);
    // Send onScroll to the boxes that scrolled since the last call (handleKey,
    // handleMouse and TerminalApp do it; tests that call scrollBy call it).
    void sendScrollEvents(Root& root);
    // Where the cursor of the focused input or textarea was drawn in the last
    // repaint (column, row), if it is on screen.
    std::optional<std::pair<int, int>> caret() const { return caret_; }
    // The focus is in an input or a textarea (keys that type text belong to it).
    bool editingText() const;

private:
    struct Style {
        std::string fg, bg;
        bool bold = false, dim = false, underline = false, reverse = false;
        bool operator==(const Style&) const = default;
    };
    // The cells of one repaint: std::inplace_vector per cell, std::mdspan over
    // all of them (terminal_renderer.cpp — kept out of this header and the module).
    struct Frame;
    struct Rect {
        int left = 0, top = 0, right = 0, bottom = 0;  // right/bottom exclusive
        Rect intersect(const Rect& o) const;
    };
    struct Layout;  // Yoga tree, kept between repaints and updated where the tree changed (terminal_renderer.cpp)

    Style styleOf(const Node& n, Style inherited) const;
    std::string textOf(const Node& n) const;
    void draw(const Layout& layout, NodeHandle h, float originX, float originY, Style style, Rect clip,
              Frame& frame);
    void put(Frame& frame, int x, int y, const Rect& clip, std::string_view text, const Style& style);
    bool isFocusable(const Node& n) const;
    void collectFocusable(NodeHandle h, std::vector<NodeHandle>& out) const;
    void fixFocus();
    NodeHandle focusTrap(NodeHandle h) const;
    bool bubbleKey(Root& root, const KeyEvent& key);
    bool handleKeyNow(Root& root, const KeyEvent& key);
    bool scrollKey(const KeyEvent& key);
    bool inside(NodeHandle h, NodeHandle ancestor) const;
    void bringIntoView(const Layout& layout, NodeHandle node);
    void propChanged(NodeHandle node, std::string_view name);


    Output output_;
    int width_ = 80;
    int height_ = 24;
    bool unicode_ = true;
    std::unique_ptr<Frame> frame_;  // last repaint
    std::unique_ptr<Layout> layout_;
    std::vector<std::string> shown_;  // ANSI lines currently on screen
    bool fullRedraw_ = true;
    NodeHandle focused_ = 0;
    std::size_t focusIndex_ = 0;  // where focus was, to recover after removal
    NodeHandle trap_ = 0;         // current focus trap
    NodeHandle savedFocus_ = 0;   // focus before the trap appeared
    // How many nodes can take focus / trap it: with none (or a live focused
    // node and no trap) a commit doesn't walk the whole tree for focus.
    std::size_t focusables_ = 0;
    std::size_t traps_ = 0;
    bool attached(NodeHandle h) const;  // in the tree under the root

    // Scroll boxes: the position of each, and its sizes from the last repaint.
    struct ScrollBox {
        int max = 0;       // the furthest it can scroll
        int content = 0;   // rows of content
        int viewport = 0;  // rows that show it
        int width = 0;
    };
    std::unordered_map<NodeHandle, int> scrollTop_;
    std::unordered_map<NodeHandle, ScrollBox> scrollBoxes_;
    std::unordered_map<NodeHandle, ScrollBox> lastScrollBoxes_;  // of the repaint before (followEnd)
    // Text fields (input, textarea): where the cursor is and what is scrolled
    // away. The value comes from the app; if it changed by other means than
    // typing here, the cursor goes to its end.
    struct Edit {
        std::string expect;                    // the value after the last edit made here
        std::size_t cursor = std::string::npos;  // a byte offset into the value
        int scrollX = 0;                       // input: cells hidden on the left
        int scrollY = 0;                       // textarea: lines hidden above
        int width = 0;                         // textarea: the width it was drawn with (Up / Down)
    };
    std::unordered_map<NodeHandle, Edit> edits_;
    std::optional<std::pair<int, int>> caret_;
    std::size_t cursorIn(NodeHandle field, const std::string& value);
    bool editKey(Root& root, NodeHandle field, const KeyEvent& key, bool multiline);
    void drawField(NodeHandle h, const Rect& content, const Rect& clip, Style style, bool multiline, Frame& frame);
    std::vector<NodeHandle> scrolled_;  // onScroll still to send
    NodeHandle scrolledTo_ = 0;         // the focus last scrolled into view
    NodeHandle scrollRequest_ = 0;      // scrollIntoView for the next repaint

    // Mouse: where each node was drawn in the last repaint, in drawing order
    // (later = on top), and what the pointer is doing.
    struct Hit {
        Rect visible;
        NodeHandle node = 0;
    };
    std::vector<Hit> hits_;
    std::unordered_map<NodeHandle, std::pair<int, int>> origins_;  // top-left corner of each node
    std::vector<NodeHandle> hovered_;  // the node under the pointer and its ancestors
    NodeHandle pressed_ = 0;           // where the button went down
    NodeHandle captured_ = 0;          // gets moves and the release (pointer capture)
    std::vector<NodeHandle> pathOf(NodeHandle h) const;
    bool sendPointer(Root& root, const std::vector<NodeHandle>& path, const char* event, PointerEvent e);
    void hover(Root& root, NodeHandle node);
};

// ---- terminal ----

// The real terminal (POSIX): raw mode, alternate screen, hidden cursor.
// Everything is restored in the destructor.
class Terminal {
public:
    Terminal();
    ~Terminal();
    Terminal(const Terminal&) = delete("there is one terminal: pass it by reference");
    Terminal& operator=(const Terminal&) = delete("there is one terminal: pass it by reference");

    static bool isInteractive();  // stdin and stdout are terminals

    void write(std::string_view data);
    int width() const;
    int height() const;
    // Waits up to timeoutMs for input; returns the decoded keys (maybe none).
    std::vector<KeyEvent> readKeys(int timeoutMs);
    // The same with mouse reports (after setMouse(true)).
    std::vector<TerminalInput> readInput(int timeoutMs);
    // Ask the terminal for mouse reports: clicks, drags and moves (SGR mode).
    // While it's on, selecting text needs Shift in most terminals.
    void setMouse(bool on);

private:
    struct State;
    std::unique_ptr<State> state_;
};

// Runs a component in the terminal until quit() is called or Ctrl+C is
// pressed. Pass quit() to the app through props if it needs to exit itself.
class TerminalApp {
public:
    TerminalApp();
    ~TerminalApp();

    void run(Element element);
    void quit() { running_ = false; }
    // The mouse works by default (clicks, hover, drag); setMouse(false) before
    // run() leaves it to the terminal, e.g. for selecting text without Shift.
    void setMouse(bool on) { mouse_ = on; }

    TerminalRenderer& renderer() { return *renderer_; }
    Root& root() { return *root_; }

private:
    Terminal terminal_;
    std::unique_ptr<TerminalRenderer> renderer_;
    std::unique_ptr<Root> root_;
    bool running_ = false;
    bool mouse_ = true;
};

}  // namespace rx
