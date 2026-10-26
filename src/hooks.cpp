#include "reactx/hooks.hpp"

#include "reactx/contract.hpp"

#include <algorithm>
#include <flat_set>
#include <map>
#include <mutex>
#include <unistd.h>
#include <sys/ioctl.h>

#include "reactx/memo.hpp"

#include "render_context.hpp"
#include "reconciler.hpp"

namespace rx::detail {

thread_local RenderContext* currentContext = nullptr;

HookSlot nextHook(const std::function<std::unique_ptr<HookBase>()>& make) {
    if (!currentContext)
        throw std::logic_error("ReactX: hooks can only be called while rendering a component");
    Fiber& fiber = *currentContext->fiber;
    std::size_t index = currentContext->hookIndex++;
    if (index == fiber.hooks.size()) {
        if (fiber.rendered)
            throw std::logic_error("ReactX: component called more hooks than on its previous render");
        fiber.hooks.push_back(make());
    }
    return {fiber.hooks[index].get(), fiber.weak_from_this(), index};
}

HookBase* hookAt(Fiber& fiber, std::size_t index) {
    REACTX_PRE(index < fiber.hooks.size());  // a setter always points at an existing hook
    return fiber.hooks[index].get();
}

void scheduleUpdate(Fiber& fiber) {
    if (fiber.alive && fiber.owner) fiber.owner->schedule(fiber);
}

void useEffectImpl(EffectFn effect, std::unique_ptr<DepsBase> deps) {
    auto slot = nextHook([] { return std::make_unique<EffectHook>(); });
    auto& hook = checkedHook<EffectHook>(slot.hook);
    // No deps: every render. First render: hook.deps is still empty, so it runs.
    bool run = !deps || !hook.deps || !hook.deps->equals(*deps);
    if (!run) return;
    hook.deps = std::move(deps);
    hook.pending = std::move(effect);
    currentContext->fiber->owner->queueEffect(slot.fiber, slot.index);
}

const ContextValueBase* readContext(const void* id) {
    if (!currentContext)
        throw std::logic_error("ReactX: useContext can only be called while rendering a component");
    Fiber& self = *currentContext->fiber;
    for (Fiber* p = self.parent; p; p = p->parent) {
        if (p->kind != ElementKind::Provider || !p->provided || p->provided->id != id) continue;
        auto me = self.weak_from_this();
        bool subscribed = std::ranges::any_of(p->consumers, [&](const std::weak_ptr<Fiber>& w) {
            return !w.owner_before(me) && !me.owner_before(w);  // same fiber
        });
        if (!subscribed) p->consumers.push_back(std::move(me));
        return p->provided.get();
    }
    return nullptr;
}

namespace {

std::flat_set<Component>& memoSet() {  // a handful of components: a sorted array (C++23)
    static std::flat_set<Component> set;
    return set;
}

std::mutex& memoMutex() {
    static std::mutex m;
    return m;
}

}  // namespace

Component registerMemo(Component component) {
    std::scoped_lock lock(memoMutex());
    memoSet().insert(component);
    return component;
}

bool isMemo(Component component) {
    std::scoped_lock lock(memoMutex());
    return memoSet().contains(component);
}

namespace {
std::flat_set<std::uintptr_t>& typedMemoSet() {
    static std::flat_set<std::uintptr_t> set;
    return set;
}
}  // namespace

void registerTypedMemo(std::uintptr_t component) {
    std::scoped_lock lock(memoMutex());
    typedMemoSet().insert(component);
}

bool isTypedMemo(std::uintptr_t component) {
    std::scoped_lock lock(memoMutex());
    return typedMemoSet().contains(component);
}

#if defined(__EMSCRIPTEN__)
// Global callback map for JS setTimeout
static std::map<int, std::function<void()>>& jsTimeouts() {
    static std::map<int, std::function<void()>> m;
    return m;
}

EM_JS(void, rx_js_set_timeout, (int id, int ms), {
    setTimeout(function() {
        Module.ccall("rx_timeout_done", null, ["number"], [id]);
    }, ms);
});

void setTimeoutJs(int delayMs, std::function<void()> cb) {
    static int nextId = 1;
    int id = nextId++;
    jsTimeouts()[id] = std::move(cb);
    rx_js_set_timeout(id, delayMs);
}

EM_JS(char*, rx_js_storage_get, (const char* key), {
    try {
        const v = localStorage.getItem(UTF8ToString(key));
        if (v === null) return stringToNewUTF8("");
        return stringToNewUTF8(v);
    } catch (e) {
        return stringToNewUTF8("");
    }
});

EM_JS(void, rx_js_storage_set, (const char* key, const char* val), {
    try {
        localStorage.setItem(UTF8ToString(key), UTF8ToString(val));
    } catch (e) {}
});

std::string storageGet(std::string_view key) {
    char* s = rx_js_storage_get(std::string(key).c_str());
    std::string out(s);
    std::free(s);
    return out;
}

void storageSet(std::string_view key, std::string_view value) {
    rx_js_storage_set(std::string(key).c_str(), std::string(value).c_str());
}

EM_JS(int, rx_js_win_w, (), { return window.innerWidth || (document.documentElement && document.documentElement.clientWidth) || 800; });
EM_JS(int, rx_js_win_h, (), { return window.innerHeight || (document.documentElement && document.documentElement.clientHeight) || 600; });

#else

// Process-level memory storage for native/terminal builds
static std::map<std::string, std::string, std::less<>>& nativeStorage() {
    static std::map<std::string, std::string, std::less<>> s;
    return s;
}

static std::mutex& storageMutex() {
    static std::mutex m;
    return m;
}

std::string storageGet(std::string_view key) {
    std::scoped_lock lock(storageMutex());
    auto& s = nativeStorage();
    auto it = s.find(key);
    return it == s.end() ? std::string{} : it->second;
}

void storageSet(std::string_view key, std::string_view value) {
    std::scoped_lock lock(storageMutex());
    nativeStorage()[std::string(key)] = std::string(value);
}

#endif

}  // namespace rx::detail

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE void rx_timeout_done(int id) {
    auto& m = rx::detail::jsTimeouts();
    auto it = m.find(id);
    if (it == m.end()) return;
    auto cb = std::move(it->second);
    m.erase(it);
    cb();
    rx::flushDom();
}
#endif

namespace rx {

WindowSize useWindowSize() {
#if defined(__EMSCRIPTEN__)
    auto [size, setSize] = useState(WindowSize{detail::rx_js_win_w(), detail::rx_js_win_h()});
    useEffect([setSize] {
        // Updated through DomRenderer or window resize
        return [] {};
    }, deps());
    return size;
#else
    // Native terminal dimensions
    winsize ws{};
    int w = 80, h = 24;
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) {
        w = ws.ws_col;
        h = ws.ws_row;
    }
    return WindowSize{.width = w, .height = h};
#endif
}

}  // namespace rx
