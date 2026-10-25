#pragma once

#include <memory>
#include <string>
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
struct Fiber : std::enable_shared_from_this<Fiber> {
    ElementKind kind = ElementKind::Fragment;
    std::string type;
    Component component = nullptr;
    Props props;
    std::string key;

    Reconciler* owner = nullptr;
    Fiber* parent = nullptr;
    int depth = 0;
    std::vector<std::shared_ptr<Fiber>> children;

    NodeHandle node = 0;              // host and text only
    std::vector<NodeHandle> attached;  // host only: children currently in the backend

    std::vector<std::unique_ptr<HookBase>> hooks;

    std::shared_ptr<const ContextValueBase> provided;  // Provider only
    std::vector<std::weak_ptr<Fiber>> consumers;         // Provider only: who read it
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

private:
    std::shared_ptr<Fiber> mount(Element element, Fiber& parent);
    void update(Fiber& fiber, Element element);
    void reconcileChildren(Fiber& fiber, std::vector<Element> elements);
    void renderComponent(Fiber& fiber);
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
};

}  // namespace rx::detail
