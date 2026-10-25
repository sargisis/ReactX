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
//
// Tags:
//   box (and any unknown tag)  children stacked vertically, or side by side
//                              with direction="row"
//   text                       its text children on one line ("\n" splits lines)
//   input                      one-line text field: value, placeholder
//   button                     "[ label ]"
// Props on any tag: border (bool), title, padding, gap, width, color, bg,
// bold, dim, underline, focusable (bool).
// Colors: black red green yellow blue magenta cyan white gray, bright-<color>.
//
// input, button and nodes with focusable=true can take focus. The focused
// node is drawn in reverse video.
class TerminalRenderer : public TextRenderer {
public:
    using Output = std::function<void(std::string_view)>;

    // Without an output the frame is only kept in memory (see frameText()).
    explicit TerminalRenderer(Output output = {});

    void setSize(int width, int height);
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
    //   any key    -> "onKey" of the focused node
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
    struct Cell {
        std::string ch = " ";
        Style style;
    };
    using Canvas = std::vector<std::vector<Cell>>;

    Style styleOf(const Node& n, Style inherited) const;
    std::string textOf(const Node& n) const;
    int naturalWidth(NodeHandle h) const;
    int draw(NodeHandle h, int x, int y, int width, Style style, Canvas& canvas);
    void put(Canvas& canvas, int x, int y, int maxX, std::string_view text, const Style& style);
    bool isFocusable(const Node& n) const;
    void collectFocusable(NodeHandle h, std::vector<NodeHandle>& out) const;
    void fixFocus();
    static std::string lineToAnsi(const std::vector<Cell>& line);

    Output output_;
    int width_ = 80;
    int height_ = 24;
    Canvas frame_;
    std::vector<std::string> shown_;  // ANSI lines currently on screen
    bool fullRedraw_ = true;
    NodeHandle focused_ = 0;
    std::size_t focusIndex_ = 0;  // where focus was, to recover after removal
};

// ---- terminal ----

// The real terminal (POSIX): raw mode, alternate screen, hidden cursor.
// Everything is restored in the destructor.
class Terminal {
public:
    Terminal();
    ~Terminal();
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

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
