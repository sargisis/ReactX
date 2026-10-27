#include "reactx/hooks.hpp"

#include "reactx/contract.hpp"

#include <algorithm>
#include <flat_set>
#include <mutex>

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
        if (!std::ranges::contains(self.providers, p)) {  // first read: subscribe (unmount unsubscribes)
            p->consumers.emplace(&self, self.weak_from_this());
            self.providers.push_back(p);
        }
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

}  // namespace rx::detail
