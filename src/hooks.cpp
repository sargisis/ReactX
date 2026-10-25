#include "reactx/hooks.hpp"

#include <algorithm>
#include <mutex>
#include <unordered_set>

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

HookBase* hookAt(Fiber& fiber, std::size_t index) { return fiber.hooks.at(index).get(); }

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

std::unordered_set<Component>& memoSet() {
    static std::unordered_set<Component> set;
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

}  // namespace rx::detail
