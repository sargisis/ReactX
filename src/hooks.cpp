#include "reactx/hooks.hpp"

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

}  // namespace rx::detail
