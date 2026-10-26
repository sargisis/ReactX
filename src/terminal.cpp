// Terminal input decoding, raw mode (POSIX termios) and the app loop.
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <stdexcept>
#include <string>
#include <text_encoding>
#include <utility>

#include "reactx/parse.hpp"
#include "reactx/hooks.hpp"
#include "reactx/terminal.hpp"
#include "utf8.hpp"

namespace rx {

// ---- key decoding ----

namespace {

KeyEvent named(std::string key) { return KeyEvent{.key = std::move(key)}; }

// "1;5" -> modifiers from the second parameter (1 + shift*1 + alt*2 + ctrl*4).
void applyModifiers(std::string_view params, KeyEvent& key) {
    auto semi = params.find(';');
    if (semi == std::string_view::npos) return;
    int m = parseNumber<int>(params.substr(semi + 1)).value_or(1);  // "1;5" -> 5
    if (m <= 1) return;
    m -= 1;
    key.shift = m & 1;
    key.alt = m & 2;
    key.ctrl = m & 4;
}

// Escape sequence starting at bytes[i] == ESC. Sets `len` to the bytes used.
KeyEvent decodeEscape(std::string_view bytes, std::size_t i, std::size_t& len);

// One key starting at bytes[i]. An empty key means "ignore these bytes".
KeyEvent decodeOne(std::string_view bytes, std::size_t i, std::size_t& len) {
    auto c = static_cast<unsigned char>(bytes[i]);
    len = 1;
    if (c == 0x1B) return decodeEscape(bytes, i, len);
    if (c == '\r' || c == '\n') return named("Enter");
    if (c == '\t') return named("Tab");
    if (c == 0x7F || c == 0x08) return named("Backspace");
    if (c == 0) return KeyEvent{.key = " ", .ctrl = true};
    if (c < 27) return KeyEvent{.key = std::string(1, static_cast<char>('a' + c - 1)), .ctrl = true};
    if (c < 0x20) return {};
    len = std::min(detail::utf8SeqLength(c), bytes.size() - i);
    return named(std::string(bytes.substr(i, len)));
}

KeyEvent decodeEscape(std::string_view bytes, std::size_t i, std::size_t& len) {
    if (i + 1 >= bytes.size()) {
        len = 1;
        return named("Escape");
    }
    char next = bytes[i + 1];
    if (next != '[' && next != 'O') {
        if (next == 0x1B) {
            len = 1;
            return named("Escape");
        }
        std::size_t inner = 0;
        KeyEvent key = decodeOne(bytes, i + 1, inner);  // Alt+key arrives as ESC key
        key.alt = true;
        len = 1 + inner;
        return key;
    }

    // CSI / SS3: ESC [ params final  or  ESC O final
    std::size_t j = i + 2;
    while (j < bytes.size() && !(bytes[j] >= 0x40 && bytes[j] <= 0x7E)) ++j;
    if (j >= bytes.size()) {  // incomplete sequence: drop it
        len = bytes.size() - i;
        return named("Escape");
    }
    len = j - i + 1;
    std::string_view params = bytes.substr(i + 2, j - i - 2);
    KeyEvent key;
    switch (bytes[j]) {
        case 'A': key = named("Up"); break;
        case 'B': key = named("Down"); break;
        case 'C': key = named("Right"); break;
        case 'D': key = named("Left"); break;
        case 'H': key = named("Home"); break;
        case 'F': key = named("End"); break;
        case 'Z': return KeyEvent{.key = "Tab", .shift = true};
        case '~': {
            std::string_view num = params.substr(0, params.find(';'));
            if (num == "1" || num == "7") key = named("Home");
            else if (num == "2") key = named("Insert");
            else if (num == "3") key = named("Delete");
            else if (num == "4" || num == "8") key = named("End");
            else if (num == "5") key = named("PageUp");
            else if (num == "6") key = named("PageDown");
            else return {};
            break;
        }
        default: return {};
    }
    applyModifiers(params, key);
    return key;
}

}  // namespace

std::vector<KeyEvent> decodeKeys(std::string_view bytes) {
    std::vector<KeyEvent> keys;
    for (std::size_t i = 0; i < bytes.size();) {
        std::size_t len = 1;
        KeyEvent key = decodeOne(bytes, i, len);
        if (!key.key.empty()) keys.push_back(std::move(key));
        i += std::max<std::size_t>(len, 1);
    }
    return keys;
}

// ---- terminal ----

struct Terminal::State {
    termios original{};
};

bool Terminal::isInteractive() { return ::isatty(STDIN_FILENO) && ::isatty(STDOUT_FILENO); }

Terminal::Terminal() : state_(std::make_unique<State>()) {
    if (!isInteractive()) throw std::runtime_error("ReactX: stdin/stdout is not a terminal");
    if (::tcgetattr(STDIN_FILENO, &state_->original) != 0)
        throw std::runtime_error("ReactX: tcgetattr failed");

    termios raw = state_->original;
    raw.c_iflag &= ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~static_cast<tcflag_t>(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);  // Ctrl+C becomes a key
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    write("\x1b[?1049h\x1b[?25l");  // alternate screen, hide cursor
}

Terminal::~Terminal() {
    write("\x1b[0m\x1b[?25h\x1b[?1049l");  // reset colors, show cursor, main screen
    ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &state_->original);
}

void Terminal::write(std::string_view data) {
    while (!data.empty()) {
        ssize_t n = ::write(STDOUT_FILENO, data.data(), data.size());
        if (n < 0) {
            if (errno == EINTR) continue;
            return;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
}

int Terminal::width() const {
    winsize ws{};
    return ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col ? ws.ws_col : 80;
}

int Terminal::height() const {
    winsize ws{};
    return ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row ? ws.ws_row : 24;
}

std::vector<KeyEvent> Terminal::readKeys(int timeoutMs) {
    pollfd fd{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
    if (::poll(&fd, 1, timeoutMs) <= 0) return {};
    char buf[256];
    ssize_t n = ::read(STDIN_FILENO, buf, sizeof buf);
    if (n <= 0) return {};
    return decodeKeys(std::string_view(buf, static_cast<std::size_t>(n)));
}

// ---- app ----

TerminalApp::TerminalApp()
    : renderer_(std::make_unique<TerminalRenderer>(
          [this](std::string_view data) { terminal_.write(data); })),
      root_(std::make_unique<Root>(*renderer_)) {
    // Box-drawing borders only if the terminal's locale is UTF-8 (C++26 <text_encoding>).
    renderer_->setUnicode(std::text_encoding::environment_is<std::text_encoding::id::UTF8>());
}

TerminalApp::~TerminalApp() = default;

void TerminalApp::run(Element element) {
    renderer_->setSize(terminal_.width(), terminal_.height());
    root_->render(std::move(element));
    running_ = true;
    while (running_) {
        int w = terminal_.width(), h = terminal_.height();
        if (w != renderer_->width() || h != renderer_->height()) {
            renderer_->setSize(w, h);
            detail::notifyResize({w, h});  // useWindowSize; re-rendered by the flush below
            renderer_->repaint();
        }
        for (const KeyEvent& key : terminal_.readKeys(100)) {
            if (key.ctrl && key.key == "c") {
                running_ = false;
                break;
            }
            // App-wide shortcuts (useKeyboard) first; a consumed key stops there.
            if (!detail::dispatchGlobalKey(key, renderer_->editingText())) renderer_->handleKey(*root_, key);
            if (!running_) break;
        }
        root_->flush();  // pick up setState calls made outside of events
    }
}

}  // namespace rx
