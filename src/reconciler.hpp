#pragma once

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "reactx/element.hpp"
#include "reactx/hooks.hpp"
#include "reactx/renderer.hpp"

namespace rx::detail {

class Reconciler;

// A mounted element. Lives between renders and holds hook state and the
// backend node (for host and text elements).
//
// Only what every kind needs is stored inline; host elements and components
// keep their own data in a part allocated for them. A text node (a third of
// a typical tree) is then about half the size it would be otherwise.
struct Fiber : std::enable_shared_from_this<Fiber> {
    ElementKind kind = ElementKind::Fragment;
    std::string type;  // host tag ("box") or the text of a text node
    std::string key;

    Reconciler* owner = nullptr;
    Fiber* parent = nullptr;
    int depth = 0;
    std::vector<std::shared_ptr<Fiber>> children;
    NodeHandle node = 0;  // host and text only

    struct HostPart {
        Props props;
        std::vector<NodeHandle> attached;  // children currently in the backend
    };
    struct ComponentPart {
        Component component = nullptr;
        std::unique_ptr<Props> props;             // a component taking Props (typed ones keep theirs in `typed`)
        std::shared_ptr<const TypedProps> typed;  // typed component: its struct of props
        std::vector<std::unique_ptr<HookBase>> hooks;
        std::vector<Fiber*> providers;  // the providers it reads (ancestors; usually 0-2)
    };
    std::unique_ptr<HostPart> hostPart;     // Host only
    std::unique_ptr<ComponentPart> compPart;  // Component only

    // What only context providers and error boundaries need. Kept out of the
    // other fibers (most of them: text, host elements, components), which
    // would otherwise each carry ~120 unused bytes.
    struct Rare {
        std::shared_ptr<const ContextValueBase> provided;  // Provider
        // Provider: who read it. Keyed by address for O(1) (un)subscribe;
        // the weak_ptr guards against a fiber that died without an unmount.
        std::unordered_map<const Fiber*, std::weak_ptr<Fiber>> consumers;
        std::shared_ptr<const ErrorFallback> fallback;  // Boundary
        std::vector<Element> boundaryChildren;          // Boundary: what retry() renders again
        std::exception_ptr error;                       // Boundary: set while it shows the fallback
    };
    std::unique_ptr<Rare> rare;  // Provider and Boundary only
    Rare& more() {
        if (!rare) rare = std::make_unique<Rare>();
        return *rare;
    }
    bool failed() const { return rare && rare->error; }  // a boundary showing its fallback
    bool rendered = false;  // component rendered at least once
    bool dirty = false;
    bool alive = true;
};

class Reconciler {
public:
    explicit Reconciler(Renderer& renderer);
    ~Reconciler();

    void render(Element element);
    void flush();
    bool dispatch(NodeHandle node, const std::string& event, const EventData& data);

    void schedule(Fiber& fiber);
    void queueEffect(std::weak_ptr<Fiber> fiber, std::size_t index);

    // renderToString: render without running effects (no page to touch yet).
    void skipEffects() { runEffects_ = false; }

    // The UI thread: the one that created the Root.
    bool onOwnThread() const { return std::this_thread::get_id() == thread_; }

private:
    std::shared_ptr<Fiber> mount(Element element, Fiber& parent);
    void update(Fiber& fiber, Element element);
    void reconcileChildren(Fiber& fiber, std::vector<Element> elements);
    void renderComponent(Fiber& fiber);
    void renderBoundary(Fiber& boundary, std::vector<Element> children);
    void showFallback(Fiber& boundary);
    void unmount(Fiber& fiber);
    void applyProps(Fiber& fiber, const Props& next);
    void sync(Fiber& host);
    void collectNodes(const Fiber& fiber, std::vector<NodeHandle>& out) const;
    Fiber& nearestHost(Fiber& fiber);
    void commit();
    void runUpdates();
    void notifyConsumers(Fiber& provider);

    Renderer& renderer_;
    std::shared_ptr<Fiber> root_;
    std::vector<std::weak_ptr<Fiber>> dirty_;
    std::vector<std::pair<std::weak_ptr<Fiber>, std::size_t>> effects_;
    std::vector<NodeHandle> toDestroy_;
    std::unordered_map<NodeHandle, Fiber*> hosts_;
    bool busy_ = false;
    bool runEffects_ = true;
    std::thread::id thread_ = std::this_thread::get_id();
};

}  // namespace rx::detail
