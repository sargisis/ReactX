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
EM_JS(char*, rx_js_storage_get, (const char* key), {
    try {
        const v = localStorage.getItem(UTF8ToString(key));
        return stringToNewUTF8(v === null ? "" : v);
    } catch (e) {
        return stringToNewUTF8("");  // storage disabled (private mode, sandbox)
    }
});
EM_JS(void, rx_js_storage_set, (const char* key, const char* value), {
    try { localStorage.setItem(UTF8ToString(key), UTF8ToString(value)); } catch (e) {}
});
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

// ---- storage ----

std::string storageGet(std::string_view key) {
#if defined(__EMSCRIPTEN__)
    char* raw = rx_js_storage_get(std::string(key).c_str());
    std::string value(raw);
    std::free(raw);
    return value;
#else
    std::scoped_lock lock(stateMutex());
    auto it = memoryStorage().find(key);
    return it == memoryStorage().end() ? std::string{} : it->second;
#endif
}

void storageSet(std::string_view key, std::string_view value) {
#if defined(__EMSCRIPTEN__)
    rx_js_storage_set(std::string(key).c_str(), std::string(value).c_str());
#else
    std::scoped_lock lock(stateMutex());
    memoryStorage().insert_or_assign(std::string(key), std::string(value));
#endif
}

}  // namespace rx::detail

namespace rx {

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

extern "C" EMSCRIPTEN_KEEPALIVE void rx_window_resized(int width, int height) {
    rx::detail::notifyResize({width, height});
    rx::flushDom();
}
#endif
