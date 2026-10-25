#include "reconciler.hpp"

#include <algorithm>
#include <stdexcept>

#include "reactx/memo.hpp"
#include "render_context.hpp"

namespace rx::detail {

namespace {

constexpr int kMaxUpdatePasses = 100;

// Children match by key, or by position when they have no key.
std::string identity(const std::string& key, std::size_t index) {
    return key.empty() ? "#" + std::to_string(index) : "k:" + key;
}

bool sameType(const Fiber& fiber, const Element& e) {
    if (fiber.kind != e.kind) return false;
    switch (e.kind) {
        case ElementKind::Host: return fiber.type == e.type;
        case ElementKind::Component: return fiber.component == e.component;
        case ElementKind::Provider: return fiber.provided->id == e.provided->id;
        default: return true;
    }
}

// Props equal for memo(): same names, equal values, no children.
bool shallowEqual(const Props& a, const Props& b) {
    if (!a.children().empty() || !b.children().empty()) return false;
    if (a.values().size() != b.values().size()) return false;
    for (const auto& [name, value] : a.values()) {
        const PropValue* other = b.find(name);
        if (!other || !propEquals(value, *other)) return false;
    }
    return true;
}

struct BusyGuard {
    bool& flag;
    explicit BusyGuard(bool& f) : flag(f) { flag = true; }
    ~BusyGuard() { flag = false; }
};

}  // namespace

Reconciler::Reconciler(Renderer& renderer) : renderer_(renderer) {
    root_ = std::make_shared<Fiber>();
    root_->kind = ElementKind::Host;
    root_->type = "#root";
    root_->owner = this;
    root_->node = renderer_.rootNode();
}

Reconciler::~Reconciler() {
    try {
        reconcileChildren(*root_, {});
        effects_.clear();
        commit();
    } catch (...) {
        // Destructors must not throw.
    }
}

void Reconciler::render(Element element) {
    {
        BusyGuard guard(busy_);
        std::vector<Element> top;
        top.push_back(std::move(element));
        reconcileChildren(*root_, std::move(top));
        commit();
    }
    flush();
}

void Reconciler::flush() {
    if (busy_) return;  // already inside a render pass; updates are picked up there
    BusyGuard guard(busy_);
    runUpdates();
}

void Reconciler::runUpdates() {
    for (int pass = 0; !dirty_.empty(); ++pass) {
        if (pass == kMaxUpdatePasses)
            throw std::runtime_error("ReactX: too many updates (state set on every render?)");

        std::vector<std::shared_ptr<Fiber>> batch;
        for (auto& weak : std::exchange(dirty_, {}))
            if (auto f = weak.lock(); f && f->alive && f->dirty) batch.push_back(std::move(f));

        // Parents first: re-rendering a parent also re-renders its children.
        std::ranges::sort(batch, {}, &Fiber::depth);
        for (auto& f : batch) {
            if (!f->alive || !f->dirty) continue;
            renderComponent(*f);
            sync(nearestHost(*f));
        }
        commit();
    }
}

bool Reconciler::dispatch(NodeHandle node, const std::string& event, const EventData& data) {
    auto it = hosts_.find(node);
    if (it == hosts_.end()) return false;
    const PropValue* value = it->second->props.find(event);
    if (!value) return false;

    // Copy the handler and the data: the handler may unmount this fiber.
    std::function<void()> call;
    if (const auto* cb = std::get_if<Callback>(value); cb && *cb) {
        call = *cb;  // plain handlers ignore the data
    } else if (const auto* kh = std::get_if<KeyHandler>(value); kh && *kh) {
        const auto* key = std::get_if<KeyEvent>(&data);
        if (!key) return false;
        call = [h = *kh, k = *key] { h(k); };
    } else if (const auto* ih = std::get_if<InputHandler>(value); ih && *ih) {
        const auto* text = std::get_if<std::string>(&data);
        if (!text) return false;
        call = [h = *ih, t = *text] { h(t); };
    } else {
        return false;
    }
    call();
    flush();
    return true;
}

void Reconciler::schedule(Fiber& fiber) {
    if (!fiber.alive || fiber.dirty) return;
    fiber.dirty = true;
    dirty_.push_back(fiber.weak_from_this());
}

void Reconciler::queueEffect(std::weak_ptr<Fiber> fiber, std::size_t index) {
    effects_.emplace_back(std::move(fiber), index);
}

std::shared_ptr<Fiber> Reconciler::mount(Element e, Fiber& parent) {
    auto f = std::make_shared<Fiber>();
    f->kind = e.kind;
    f->type = std::move(e.type);
    f->component = e.component;
    f->key = std::move(e.key);
    f->owner = this;
    f->parent = &parent;
    f->depth = parent.depth + 1;

    switch (f->kind) {
        case ElementKind::Text:
            f->node = renderer_.createText(f->type);
            break;
        case ElementKind::Host: {
            f->node = renderer_.createElement(f->type);
            hosts_[f->node] = f.get();
            auto children = e.props.takeChildren();
            applyProps(*f, e.props);
            f->props = std::move(e.props);
            reconcileChildren(*f, std::move(children));
            break;
        }
        case ElementKind::Fragment:
            reconcileChildren(*f, e.props.takeChildren());
            break;
        case ElementKind::Provider:
            f->provided = std::move(e.provided);
            reconcileChildren(*f, e.props.takeChildren());
            break;
        case ElementKind::Component:
            f->props = std::move(e.props);
            renderComponent(*f);
            break;
    }
    return f;
}

void Reconciler::update(Fiber& f, Element e) {
    switch (f.kind) {
        case ElementKind::Text:
            if (f.type != e.type) {
                f.type = std::move(e.type);
                renderer_.setText(f.node, f.type);
            }
            break;
        case ElementKind::Host: {
            auto children = e.props.takeChildren();
            applyProps(f, e.props);
            f.props = std::move(e.props);
            reconcileChildren(f, std::move(children));
            break;
        }
        case ElementKind::Fragment:
            reconcileChildren(f, e.props.takeChildren());
            break;
        case ElementKind::Provider: {
            bool changed = !f.provided->equals(*e.provided);
            f.provided = std::move(e.provided);
            if (changed) notifyConsumers(f);
            reconcileChildren(f, e.props.takeChildren());
            break;
        }
        case ElementKind::Component:
            // memo: same props and no pending update -> keep the old output.
            if (!f.dirty && isMemo(f.component) && shallowEqual(f.props, e.props)) break;
            f.props = std::move(e.props);
            renderComponent(f);
            break;
    }
}

// Re-render every component that read this provider's value. They are
// scheduled, so a memo() component in between can't block the update.
void Reconciler::notifyConsumers(Fiber& provider) {
    std::erase_if(provider.consumers, [](const std::weak_ptr<Fiber>& w) {
        auto c = w.lock();
        return !c || !c->alive;
    });
    for (auto& weak : provider.consumers)
        if (auto c = weak.lock()) schedule(*c);
}

void Reconciler::reconcileChildren(Fiber& f, std::vector<Element> elements) {
    std::unordered_map<std::string, std::shared_ptr<Fiber>> old;
    for (std::size_t i = 0; i < f.children.size(); ++i) {
        auto& child = f.children[i];
        auto [it, inserted] = old.try_emplace(identity(child->key, i), child);
        if (!inserted) unmount(*child);  // duplicate key
    }
    f.children.clear();

    for (std::size_t i = 0; i < elements.size(); ++i) {
        Element& e = elements[i];
        auto it = old.find(identity(e.key, i));
        if (it != old.end() && sameType(*it->second, e)) {
            auto child = std::move(it->second);
            old.erase(it);
            update(*child, std::move(e));
            f.children.push_back(std::move(child));
        } else {
            f.children.push_back(mount(std::move(e), f));
        }
    }

    for (auto& [id, child] : old) unmount(*child);
    if (f.kind == ElementKind::Host) sync(f);
}

void Reconciler::renderComponent(Fiber& f) {
    f.dirty = false;
    RenderContext ctx{&f, 0};
    RenderContext* previous = std::exchange(currentContext, &ctx);
    Element out;
    try {
        out = f.component(f.props);
    } catch (...) {
        currentContext = previous;
        throw;
    }
    currentContext = previous;

    if (f.rendered && ctx.hookIndex != f.hooks.size())
        throw std::logic_error("ReactX: component called fewer hooks than on its previous render");
    f.rendered = true;

    std::vector<Element> children;
    children.push_back(std::move(out));
    reconcileChildren(f, std::move(children));
}

void Reconciler::unmount(Fiber& f) {
    f.alive = false;
    for (auto& child : f.children) unmount(*child);
    for (auto& hook : f.hooks) {
        if (auto* effect = dynamic_cast<EffectHook*>(hook.get()); effect && effect->cleanup)
            std::exchange(effect->cleanup, {})();
    }
    if (f.node) {
        hosts_.erase(f.node);
        toDestroy_.push_back(f.node);
    }
}

void Reconciler::applyProps(Fiber& f, const Props& next) {
    const Props& old = f.props;
    for (const auto& [name, value] : next.values()) {
        if (isHandler(value)) continue;  // handled by the core
        const PropValue* prev = old.find(name);
        if (!prev || !propEquals(*prev, value)) renderer_.setProp(f.node, name, value);
    }
    for (const auto& [name, value] : old.values()) {
        if (isHandler(value)) continue;
        const PropValue* now = next.find(name);
        if (!now || isHandler(*now)) renderer_.removeProp(f.node, name);
    }
}

void Reconciler::collectNodes(const Fiber& f, std::vector<NodeHandle>& out) const {
    for (const auto& child : f.children) {
        if (child->node) out.push_back(child->node);
        else collectNodes(*child, out);
    }
}

// Make the backend children of `host` match the fiber tree.
void Reconciler::sync(Fiber& host) {
    std::vector<NodeHandle> wanted;
    collectNodes(host, wanted);

    std::vector<NodeHandle> current;
    for (NodeHandle n : host.attached) {
        if (std::ranges::find(wanted, n) == wanted.end()) renderer_.removeChild(host.node, n);
        else current.push_back(n);
    }

    for (std::size_t i = 0; i < wanted.size(); ++i) {
        if (i < current.size() && current[i] == wanted[i]) continue;
        renderer_.insertChild(host.node, wanted[i], i);
        auto pos = std::ranges::find(current, wanted[i]);
        if (pos != current.end()) current.erase(pos);
        current.insert(current.begin() + static_cast<std::ptrdiff_t>(i), wanted[i]);
    }
    host.attached = std::move(wanted);
}

Fiber& Reconciler::nearestHost(Fiber& f) {
    Fiber* p = f.parent;
    while (p->kind != ElementKind::Host) p = p->parent;
    return *p;
}

void Reconciler::commit() {
    for (NodeHandle n : std::exchange(toDestroy_, {})) renderer_.destroyNode(n);
    renderer_.commit();

    for (auto& [weak, index] : std::exchange(effects_, {})) {
        auto f = weak.lock();
        if (!f || !f->alive) continue;
        auto& hook = static_cast<EffectHook&>(*f->hooks[index]);
        if (!hook.pending) continue;
        if (hook.cleanup) std::exchange(hook.cleanup, {})();
        hook.cleanup = std::exchange(hook.pending, {})();
    }
}

}  // namespace rx::detail
