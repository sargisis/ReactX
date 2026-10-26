#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
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
//   position (absolute | relative), top, left, right, bottom
//   display (none | flex), overflow (visible | hidden; a border always clips)
//   opaque (bool): clear what is drawn under the box (modals)
//   trapFocus (bool): while such a node exists, Tab stays inside the last one;
//                     focus returns to where it was when it goes away
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
    void destroyNode(NodeHandle node) override;

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
    bool focus(NodeHandle node);
    void focusNext();
    void focusPrev();

    // Default key handling:
    //   Tab / Down -> next focusable, Shift+Tab / Up -> previous
    //   any key    -> "onKey" of the focused node, then of each ancestor
    //                 (bubbling, like the DOM)
    //   input      -> printable keys and Backspace edit "value" via "onInput"
    //   Enter / Space -> "onClick" of the focused node
    // Returns true if the key did something.
    bool handleKey(Root& root, const KeyEvent& key);

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
    struct Layout;  // Yoga tree for one repaint (terminal_renderer.cpp)

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


    Output output_;
    int width_ = 80;
    int height_ = 24;
    bool unicode_ = true;
    std::unique_ptr<Frame> frame_;  // last repaint
    std::vector<std::string> shown_;  // ANSI lines currently on screen
    bool fullRedraw_ = true;
    NodeHandle focused_ = 0;
    std::size_t focusIndex_ = 0;  // where focus was, to recover after removal
    NodeHandle trap_ = 0;         // current focus trap
    NodeHandle savedFocus_ = 0;   // focus before the trap appeared
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

    TerminalRenderer& renderer() { return *renderer_; }
    Root& root() { return *root_; }

private:
    Terminal terminal_;
    std::unique_ptr<TerminalRenderer> renderer_;
    std::unique_ptr<Root> root_;
    bool running_ = false;
};

}  // namespace rx
