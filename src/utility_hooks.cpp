// Timers, window size and storage behind useDebounce, useWindowSize and
// useLocalStorage. Everything runs on the UI thread: in the browser through
// setTimeout / the resize event, natively through Root::flush() and
// TerminalApp — so callbacks may set state like event handlers do.
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "reactx/hooks.hpp"

#include "utf8.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

#include <cstdlib>

#include "reactx/dom.hpp"
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace rx::detail {

namespace {

using Clock = std::chrono::steady_clock;

std::mutex& stateMutex() {
    static std::mutex m;
    return m;
}

struct Timer {
    Clock::time_point due;
    std::function<void()> fn;
};

std::map<int, Timer>& timers() {  // id -> timer
    static std::map<int, Timer> all;
    return all;
}

std::map<int, std::function<void(WindowSize)>>& resizeListeners() {
    static std::map<int, std::function<void(WindowSize)>> all;
    return all;
}

std::map<int, std::function<bool(const KeyEvent&)>>& keyListeners() {
    static std::map<int, std::function<bool(const KeyEvent&)>> all;
    return all;
}

// Keys that change the text of a focused field (typed characters, Backspace,
// cursor moves). Shortcuts don't get them while the user types.
bool editsText(const KeyEvent& k) {
    if (k.ctrl || k.alt) return false;
    if (rx::detail::isPrintableKey(k.key)) return true;
    for (std::string_view name : {"Backspace", "Delete", "Left", "Right", "Home", "End"})
        if (k.key == name) return true;
    return false;
}

int nextId = 1;  // timers and listeners, under stateMutex()

// Removes timer `id` and returns its callback (empty if it was cancelled).
std::function<void()> takeTimer(int id) {
    std::scoped_lock lock(stateMutex());
    auto it = timers().find(id);
    if (it == timers().end()) return {};
    auto fn = std::move(it->second.fn);
    timers().erase(it);
    return fn;
}

#if defined(__EMSCRIPTEN__)
EM_JS(void, rx_js_set_timeout, (int id, int ms), {
    setTimeout(function() { Module.ccall("rx_timer_fired", null, ["number"], [id]); }, ms);
});
EM_JS(int, rx_js_window_width, (), { return window.innerWidth | 0; });
EM_JS(int, rx_js_window_height, (), { return window.innerHeight | 0; });
EM_JS(void, rx_js_listen_resize, (), {
    if (Module.rxResizeListening) return;
    Module.rxResizeListening = true;
    window.addEventListener("resize", function() {
        Module.ccall("rx_window_resized", null, ["number", "number"], [window.innerWidth | 0, window.innerHeight | 0]);
    });
});
// Capture phase on the window: runs before the elements' own handlers, and
// also when nothing on the page has focus.
EM_JS(void, rx_js_listen_keys, (), {
    if (Module.rxKeysListening) return;
    Module.rxKeysListening = true;
    const names = { ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right", Esc: "Escape" };
    window.addEventListener("keydown", function(e) {
        if (e.isComposing) return;
        const t = e.target;
        const tag = t && t.tagName ? t.tagName.toLowerCase() : "";
        const textInput = tag === "input" && !["checkbox", "radio", "button", "submit", "reset", "range", "color", "file"].includes(t.type);
        const editing = textInput || tag === "textarea" || tag === "select" || !!(t && t.isContentEditable);
        const consumed = Module.ccall("rx_global_key", "number",
            ["string", "number", "number", "number", "number"],
            [names[e.key] || e.key, e.ctrlKey || e.metaKey ? 1 : 0, e.altKey ? 1 : 0, e.shiftKey ? 1 : 0, editing ? 1 : 0]);
        if (consumed) { e.preventDefault(); e.stopPropagation(); }
    }, true);
});
// "v" + the value, "n" if there is none, "e" + the reason if storage failed.
EM_JS(char*, rx_js_storage_get, (const char* key), {
    try {
        const v = localStorage.getItem(UTF8ToString(key));
        return stringToNewUTF8(v === null ? "n" : "v" + v);
    } catch (e) {
        return stringToNewUTF8("e" + String((e && e.name) || e) + ": " + String((e && e.message) || ""));
    }
});
// "" on success, otherwise the reason (QuotaExceededError, SecurityError, ...).
EM_JS(char*, rx_js_storage_set, (const char* key, const char* value, int remove), {
    try {
        if (remove) localStorage.removeItem(UTF8ToString(key));
        else localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
        return stringToNewUTF8("");
    } catch (e) {
        return stringToNewUTF8(String((e && e.name) || e) + ": " + String((e && e.message) || ""));
    }
});
std::string takeJsString(char* raw) {
    std::string value(raw);
    std::free(raw);
    return value;
}
#else
std::map<std::string, std::string, std::less<>>& memoryStorage() {
    static std::map<std::string, std::string, std::less<>> all;
    return all;
}
#endif

}  // namespace

// ---- timers ----

Cleanup setTimer(std::chrono::milliseconds delay, std::function<void()> fn) {
    int id = 0;
    {
        std::scoped_lock lock(stateMutex());
        id = nextId++;
        timers().emplace(id, Timer{Clock::now() + delay, std::move(fn)});
    }
#if defined(__EMSCRIPTEN__)
    rx_js_set_timeout(id, static_cast<int>(delay.count()));
#endif
    return [id] { takeTimer(id); };
}

void runDueTimers() {
#if !defined(__EMSCRIPTEN__)
    // The due ones in deadline order; run outside the lock (they set state,
    // and may start or cancel timers).
    std::vector<std::pair<Clock::time_point, int>> due;
    {
        std::scoped_lock lock(stateMutex());
        auto now = Clock::now();
        for (const auto& [id, timer] : timers())
            if (timer.due <= now) due.emplace_back(timer.due, id);
    }
    std::ranges::sort(due);
    for (const auto& [_, id] : due)
        if (auto fn = takeTimer(id)) fn();  // a timer cancelled by an earlier one is gone
#endif
}

// ---- window size ----

WindowSize currentWindowSize() {
#if defined(__EMSCRIPTEN__)
    return {rx_js_window_width(), rx_js_window_height()};
#else
    winsize ws{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) return {ws.ws_col, ws.ws_row};
    return {80, 24};  // not a terminal (tests, pipes)
#endif
}

Cleanup onResize(std::function<void(WindowSize)> fn) {
    int id = 0;
    {
        std::scoped_lock lock(stateMutex());
        id = nextId++;
        resizeListeners().emplace(id, std::move(fn));
    }
#if defined(__EMSCRIPTEN__)
    rx_js_listen_resize();
#endif
    return [id] {
        std::scoped_lock lock(stateMutex());
        resizeListeners().erase(id);
    };
}

void notifyResize(WindowSize size) {
    std::vector<std::function<void(WindowSize)>> listeners;
    {
        std::scoped_lock lock(stateMutex());
        for (const auto& [id, fn] : resizeListeners()) listeners.push_back(fn);
    }
    for (const auto& fn : listeners) fn(size);
}

// ---- keyboard shortcuts ----

Cleanup onGlobalKey(std::function<bool(const KeyEvent&)> handler) {
    int id = 0;
    {
        std::scoped_lock lock(stateMutex());
        id = nextId++;
        keyListeners().emplace(id, std::move(handler));
    }
#if defined(__EMSCRIPTEN__)
    rx_js_listen_keys();
#endif
    return [id] {
        std::scoped_lock lock(stateMutex());
        keyListeners().erase(id);
    };
}

bool dispatchGlobalKey(const KeyEvent& key, bool editing) {
    if (editing && editsText(key)) return false;
    std::vector<std::function<bool(const KeyEvent&)>> handlers;
    {
        std::scoped_lock lock(stateMutex());
        for (const auto& [id, fn] : keyListeners()) handlers.push_back(fn);
    }
    bool consumed = false;
    for (const auto& fn : handlers) consumed = fn(key) || consumed;  // every subscriber hears the key
    return consumed;
}

// ---- storage ----

std::string storageGet(std::string_view key) { return rx::storageRead(key).value_or(std::nullopt).value_or(""); }

void storageSet(std::string_view key, std::string_view value) { (void)rx::storageWrite(key, value); }

}  // namespace rx::detail

namespace rx {

std::expected<std::optional<std::string>, std::string> storageRead(std::string_view key) {
#if defined(__EMSCRIPTEN__)
    std::string answer = detail::takeJsString(detail::rx_js_storage_get(std::string(key).c_str()));
    if (answer.starts_with('v')) return answer.substr(1);
    if (answer.starts_with('e')) return std::unexpected(answer.substr(1));
    return std::optional<std::string>{};
#else
    std::scoped_lock lock(detail::stateMutex());
    auto it = detail::memoryStorage().find(key);
    if (it == detail::memoryStorage().end()) return std::optional<std::string>{};
    return it->second;
#endif
}

std::expected<void, std::string> storageWrite(std::string_view key, std::string_view value) {
#if defined(__EMSCRIPTEN__)
    std::string error = detail::takeJsString(detail::rx_js_storage_set(std::string(key).c_str(), std::string(value).c_str(), 0));
    if (!error.empty()) return std::unexpected(std::move(error));
#else
    std::scoped_lock lock(detail::stateMutex());
    detail::memoryStorage().insert_or_assign(std::string(key), std::string(value));
#endif
    return {};
}

std::expected<void, std::string> storageRemove(std::string_view key) {
#if defined(__EMSCRIPTEN__)
    std::string error = detail::takeJsString(detail::rx_js_storage_set(std::string(key).c_str(), "", 1));
    if (!error.empty()) return std::unexpected(std::move(error));
#else
    std::scoped_lock lock(detail::stateMutex());
    if (auto it = detail::memoryStorage().find(key); it != detail::memoryStorage().end()) detail::memoryStorage().erase(it);
#endif
    return {};
}

WindowSize useWindowSize() {
    auto [size, setSize] = useState(detail::currentWindowSize());
    useEffect([setSize] { return detail::onResize([setSize](WindowSize s) { setSize(s); }); }, deps());
    return size;
}

}  // namespace rx

#if defined(__EMSCRIPTEN__)
// Called by the browser (see the EM_JS above); the DOM is updated right after.
extern "C" EMSCRIPTEN_KEEPALIVE void rx_timer_fired(int id) {
    if (auto fn = rx::detail::takeTimer(id)) {
        fn();
        rx::flushDom();
    }
}

// From the window's keydown listener; 1 = consumed (the page then calls preventDefault).
extern "C" EMSCRIPTEN_KEEPALIVE int rx_global_key(const char* key, int ctrl, int alt, int shift, int editing) {
    bool consumed = rx::detail::dispatchGlobalKey(
        rx::KeyEvent{.key = key, .ctrl = ctrl != 0, .alt = alt != 0, .shift = shift != 0}, editing != 0);
    rx::flushDom();
    return consumed ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void rx_window_resized(int width, int height) {
    rx::detail::notifyResize({width, height});
    rx::flushDom();
}
#endif
