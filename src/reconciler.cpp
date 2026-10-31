#include "reconciler.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

#include "reactx/memo.hpp"
#include "render_context.hpp"

namespace rx::detail {

namespace {

constexpr int kMaxUpdatePasses = 100;

bool sameType(const Fiber& fiber, const Element& e) {
    if (fiber.kind != e.kind) return false;
    switch (e.kind) {
        case ElementKind::Host: return fiber.type == e.type;
        case ElementKind::Component:
            if (fiber.compPart->component != e.component || bool(fiber.compPart->typed) != bool(e.typed)) return false;
            return !e.typed || fiber.compPart->typed->component() == e.typed->component();
        case ElementKind::Provider: return fiber.rare->provided->id == e.provided->id;
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

// Counts marked positions before an index in O(log n) (a Fenwick tree).
class PositionCounter {
public:
    explicit PositionCounter(std::size_t n) : tree_(n + 1, 0) {
        for (std::size_t i = 1; i <= n; ++i) {  // every position starts marked
            tree_[i] += 1;
            if (std::size_t parent = i + (i & -i); parent <= n) tree_[parent] += tree_[i];
        }
    }
    void unmark(std::size_t pos) {
        for (std::size_t i = pos + 1; i < tree_.size(); i += i & -i) --tree_[i];
    }
    std::size_t before(std::size_t pos) const {  // marked positions in [0, pos)
        std::size_t sum = 0;
        for (std::size_t i = pos; i > 0; i -= i & -i) sum += tree_[i];
        return sum;
    }

private:
    std::vector<std::size_t> tree_;
};

// Indices (into `seq`) of one longest strictly increasing subsequence.
std::vector<std::size_t> longestIncreasing(const std::vector<std::size_t>& seq) {
    std::vector<std::size_t> tails;  // tails[k]: index of the smallest end of a run of length k + 1
    std::vector<std::size_t> prev(seq.size(), SIZE_MAX);
    for (std::size_t i = 0; i < seq.size(); ++i) {
        auto it = std::ranges::lower_bound(tails, seq[i], {}, [&](std::size_t j) { return seq[j]; });
        if (it != tails.begin()) prev[i] = *(it - 1);
        if (it == tails.end()) tails.push_back(i);
        else *it = i;
    }
    std::vector<std::size_t> out(tails.size());
    for (std::size_t k = tails.size(), i = tails.empty() ? SIZE_MAX : tails.back(); k > 0; i = prev[i]) out[--k] = i;
    return out;
}

const std::vector<std::unique_ptr<HookBase>> noHooks;  // for fibers that aren't components

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
    root_->hostPart = std::make_unique<Fiber::HostPart>();
}

Reconciler::~Reconciler() {
    try {
        reconcileChildren(*root_, {});
        effects_.clear();
        // The app is going away: user callbacks aren't called, but NodeRefs
        // forget the node, so focus() can't reach a destroyed backend.
        for (auto& ref : std::exchange(refsOff_, {}))
            if (const auto* r = ref.target<NodeRef>()) r->attach(0, nullptr);
        refsOn_.clear();
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
        for (std::size_t k = 0; k < batch.size(); ++k) {
            auto& f = batch[k];
            if (!f->alive || !f->dirty) continue;
            try {
                if (f->kind == ElementKind::Boundary) renderBoundary(*f, f->rare->boundaryChildren);
                else renderComponent(*f);
                sync(nearestHost(*f));
            } catch (...) {
                // A state update inside a boundary failed: the nearest boundary
                // that isn't already showing its fallback takes over.
                Fiber* b = f->parent;
                while (b && (b->kind != ElementKind::Boundary || b->failed())) b = b->parent;
                if (!b) {
                    // Nobody catches it. Keep the rest of the batch queued, or
                    // their setters would never schedule them again.
                    for (std::size_t rest = k + 1; rest < batch.size(); ++rest)
                        if (batch[rest]->alive && batch[rest]->dirty) dirty_.push_back(batch[rest]);
                    throw;
                }
                b->more().error = std::current_exception();
                showFallback(*b);
                sync(nearestHost(*b));
            }
        }
        commit();
    }
}

bool Reconciler::dispatch(NodeHandle node, const std::string& event, const EventData& data) {
    auto it = hosts_.find(node);
    if (it == hosts_.end()) return false;
    const PropValue* value = it->second->hostPart->props.find(event);
    if (!value) return false;

    // Copy the handler and the data: the handler may unmount this fiber.
    std::function<void()> call;
    if (const auto* cb = std::get_if<Callback>(value); cb && *cb) {
        call = *cb;  // plain handlers ignore the data
    } else if (const auto* kh = std::get_if<KeyHandler>(value); kh && *kh) {
        const auto* key = std::get_if<KeyEvent>(&data);
        if (!key) return false;
        call = [h = *kh, k = *key] { h(k); };
    } else if (const auto* sh = std::get_if<ScrollHandler>(value); sh && *sh) {
        const auto* pos = std::get_if<ScrollEvent>(&data);
        if (!pos) return false;
        call = [h = *sh, p = *pos] { h(p); };
    } else if (const auto* ih = std::get_if<InputHandler>(value); ih && *ih) {
        const auto* text = std::get_if<std::string>(&data);
        if (!text) return false;
        call = [h = *ih, t = *text] { h(t); };
    } else if (const auto* ph = std::get_if<PointerHandler>(value); ph && *ph) {
        const auto* pointer = std::get_if<PointerEvent>(&data);
        if (!pointer) return false;
        call = [h = *ph, p = *pointer] { h(p); };
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
    f->key = std::move(e.key);
    f->owner = this;
    f->parent = &parent;
    f->depth = parent.depth + 1;

    try {
        switch (f->kind) {
            case ElementKind::Text:
                f->node = renderer_.createText(f->type);
                break;
            case ElementKind::Host: {
                f->node = renderer_.createElement(f->type);
                hosts_[f->node] = f.get();
                f->hostPart = std::make_unique<Fiber::HostPart>();
                auto children = e.props.takeChildren();
                applyProps(*f, e.props);
                f->hostPart->props = std::move(e.props);
                reconcileChildren(*f, std::move(children));
                break;
            }
            case ElementKind::Fragment:
                reconcileChildren(*f, e.props.takeChildren());
                break;
            case ElementKind::Provider:
                f->more().provided = std::move(e.provided);
                reconcileChildren(*f, e.props.takeChildren());
                break;
            case ElementKind::Component:
                f->compPart = std::make_unique<Fiber::ComponentPart>();
                f->compPart->component = e.component;
                if (!e.typed) f->compPart->props = std::make_unique<Props>(std::move(e.props));
                f->compPart->typed = std::move(e.typed);
                renderComponent(*f);
                break;
            case ElementKind::Boundary:
                f->more().fallback = std::move(e.fallback);
                renderBoundary(*f, e.props.takeChildren());
                break;
        }
    } catch (...) {
        unmount(*f);  // nothing of it may stay registered (hosts_, context readers)
        throw;
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
            f.hostPart->props = std::move(e.props);
            reconcileChildren(f, std::move(children));
            break;
        }
        case ElementKind::Fragment:
            reconcileChildren(f, e.props.takeChildren());
            break;
        case ElementKind::Provider: {
            bool changed = !f.rare->provided->equals(*e.provided);
            f.rare->provided = std::move(e.provided);
            if (changed) notifyConsumers(f);
            reconcileChildren(f, e.props.takeChildren());
            break;
        }
        case ElementKind::Boundary:
            f.more().fallback = std::move(e.fallback);
            renderBoundary(f, e.props.takeChildren());
            break;
        case ElementKind::Component:
            // memo: same props and no pending update -> keep the old output.
            auto& c = *f.compPart;
            if (!f.dirty && c.typed && isTypedMemo(c.typed->component()) && c.typed->equals(*e.typed)) break;
            if (!f.dirty && !c.typed && isMemo(c.component) && shallowEqual(*c.props, e.props)) break;
            if (!e.typed) *c.props = std::move(e.props);
            c.typed = std::move(e.typed);
            renderComponent(f);
            break;
    }
}

// Re-render every component that read this provider's value. They are
// scheduled, so a memo() component in between can't block the update.
void Reconciler::notifyConsumers(Fiber& provider) {
    std::erase_if(provider.more().consumers, [](const auto& entry) {
        auto c = entry.second.lock();
        return !c || !c->alive;
    });
    for (auto& [address, weak] : provider.rare->consumers)
        if (auto c = weak.lock()) schedule(*c);
}

// Children match by key, or by position when they have no key. Old
// children that match nothing are unmounted after the new ones rendered.
void Reconciler::reconcileChildren(Fiber& f, std::vector<Element> elements) {
    std::vector<std::shared_ptr<Fiber>> old = std::exchange(f.children, {});
    f.children.reserve(elements.size());
    std::vector<bool> used(old.size(), false);
    auto place = [&](std::size_t j, Element& e) {  // reuse old[j] if it can show e
        if (j < old.size() && !used[j] && sameType(*old[j], e)) {
            used[j] = true;
            f.children.push_back(old[j]);  // first: if update() throws, it is unmounted with the others
            update(*old[j], std::move(e));
        } else {
            f.children.push_back(mount(std::move(e), f));
        }
    };

    try {
        // Most renders keep every child where it was: match position by position
        // while the keys agree, without building a map.
        std::size_t i = 0;
        for (; i < elements.size() && i < old.size() && old[i]->key == elements[i].key; ++i) place(i, elements[i]);

        if (i < elements.size()) {
            std::unordered_map<std::string_view, std::size_t> byKey;  // the rest of the old keyed children
            for (std::size_t j = i; j < old.size(); ++j)
                if (!old[j]->key.empty()) byKey.try_emplace(old[j]->key, j);  // a duplicate key matches nothing
            for (; i < elements.size(); ++i) {
                Element& e = elements[i];
                std::size_t j = SIZE_MAX;
                if (e.key.empty()) {
                    if (i < old.size() && old[i]->key.empty()) j = i;
                } else if (auto it = byKey.find(e.key); it != byKey.end()) {
                    j = it->second;
                }
                place(j, e);
            }
        }
    } catch (...) {
        // A child threw while rendering. Unmount everything below `f`, so no
        // half-built fiber stays registered; an errorBoundary above (or the
        // caller) decides what to show instead.
        for (std::size_t j = 0; j < old.size(); ++j)
            if (!used[j]) unmount(*old[j]);
        for (auto& child : std::exchange(f.children, {})) unmount(*child);
        throw;
    }

    for (std::size_t j = 0; j < old.size(); ++j)
        if (!used[j]) unmount(*old[j]);
    if (f.kind == ElementKind::Host) sync(f);
}

void Reconciler::renderBoundary(Fiber& b, std::vector<Element> children) {
    b.dirty = false;
    b.more().boundaryChildren = children;  // kept for retry()
    if (b.failed()) {
        showFallback(b);
        return;
    }
    try {
        reconcileChildren(b, std::move(children));
    } catch (...) {
        b.rare->error = std::current_exception();
        showFallback(b);  // if the fallback throws too, the next boundary up gets it
    }
}

void Reconciler::showFallback(Fiber& b) {
    RenderError error{.message = "unknown error", .exception = b.rare->error, .retry = {}};
    try {
        std::rethrow_exception(b.rare->error);
    } catch (const std::exception& e) {
        error.message = e.what();
    } catch (...) {
    }
    error.retry = [weak = b.weak_from_this()] {
        auto boundary = weak.lock();
        if (!boundary || !boundary->alive || !boundary->failed()) return;
        boundary->rare->error = nullptr;
        boundary->owner->schedule(*boundary);
    };
    std::vector<Element> shown;
    shown.push_back((*b.rare->fallback)(error));
    reconcileChildren(b, std::move(shown));
}

void Reconciler::renderComponent(Fiber& f) {
    f.dirty = false;
    RenderContext ctx{&f, 0};
    RenderContext* previous = std::exchange(currentContext, &ctx);
    Element out;
    try {
        out = f.compPart->typed ? f.compPart->typed->render() : f.compPart->component(*f.compPart->props);
    } catch (...) {
        currentContext = previous;
        throw;
    }
    currentContext = previous;

    if (f.rendered && ctx.hookIndex != f.compPart->hooks.size())
        throw std::logic_error("ReactX: component called fewer hooks than on its previous render");
    f.rendered = true;

    std::vector<Element> children;
    children.push_back(std::move(out));
    reconcileChildren(f, std::move(children));
}

void Reconciler::unmount(Fiber& f) {
    f.alive = false;
    // Providers are ancestors, so they are still alive here. Without this a
    // provider whose value never changes would remember every reader forever.
    if (f.compPart)
        for (Fiber* p : std::exchange(f.compPart->providers, {})) p->rare->consumers.erase(&f);
    for (auto& child : f.children) unmount(*child);
    for (auto& hook : f.compPart ? f.compPart->hooks : noHooks) {
        if (auto* effect = dynamic_cast<EffectHook*>(hook.get()); effect && effect->cleanup)
            std::exchange(effect->cleanup, {})();
    }
    if (f.node) {
        hosts_.erase(f.node);
        toDestroy_.push_back(f.node);
    }
    if (f.hostPart && f.hostPart->hasRef)
        if (const PropValue* ref = f.hostPart->props.find("ref"))
            if (const auto* off = std::get_if<RefHandler>(ref); off && *off) refsOff_.push_back(*off);
}

void Reconciler::applyProps(Fiber& f, const Props& next) {
    const Props& old = f.hostPart->props;
    // "ref" is a handler too; noticed in the loops, not looked up separately.
    const PropValue* now = nullptr;
    const PropValue* was = nullptr;
    for (const auto& [name, value] : next.values()) {
        if (isHandler(value)) {  // handled by the core
            if (name == "ref") now = &value;
            continue;
        }
        const PropValue* prev = old.find(name);
        if (!prev || !propEquals(*prev, value)) renderer_.setProp(f.node, name, value);
    }
    for (const auto& [name, value] : old.values()) {
        if (isHandler(value)) {
            if (name == "ref") was = &value;
            continue;
        }
        const PropValue* kept = next.find(name);
        if (!kept || isHandler(*kept)) renderer_.removeProp(f.node, name);
    }

    // "ref": the old one learns the node is gone, the new one gets it (in the commit).
    if (!was && !now) return;
    f.hostPart->hasRef = now != nullptr;
    if (was && now && propEquals(*was, *now)) return;
    if (const auto* off = was ? std::get_if<RefHandler>(was) : nullptr; off && *off) refsOff_.push_back(*off);
    if (const auto* on = now ? std::get_if<RefHandler>(now) : nullptr; on && *on) refsOn_.emplace_back(*on, f.node);
}

void Reconciler::collectNodes(const Fiber& f, std::vector<NodeHandle>& out) const {
    for (const auto& child : f.children) {
        if (child->node) out.push_back(child->node);
        else collectNodes(*child, out);
    }
}

// Make the backend children of `host` match the fiber tree with as few
// moves as possible: nodes on a longest increasing run of their new positions
// stay where they are, every other node is inserted once. Swapping two rows
// of a long list moves two nodes, removing rows moves none.
void Reconciler::sync(Fiber& host) {
    std::vector<NodeHandle> wanted;
    collectNodes(host, wanted);
    auto& attached = host.hostPart->attached;
    if (wanted == attached) return;  // the usual case: nothing moved

    std::unordered_map<NodeHandle, std::size_t> wantedIndex;
    wantedIndex.reserve(wanted.size());
    for (std::size_t i = 0; i < wanted.size(); ++i) wantedIndex.emplace(wanted[i], i);

    // Detach what is gone; the rest keeps its backend order.
    std::vector<std::size_t> seq;  // seq[j]: new position of the j-th remaining node
    std::vector<std::size_t> oldPos(wanted.size(), SIZE_MAX);  // by new position; SIZE_MAX = a new node
    for (NodeHandle n : attached) {
        auto it = wantedIndex.find(n);
        if (it == wantedIndex.end()) {
            renderer_.removeChild(host.node, n);
            continue;
        }
        oldPos[it->second] = seq.size();
        seq.push_back(it->second);
    }

    std::vector<bool> stays(wanted.size(), false);
    for (std::size_t j : longestIncreasing(seq)) stays[seq[j]] = true;

    // From the end: each node that doesn't stay goes right before the part
    // already in place. Its index = the old nodes still in front of that part,
    // i.e. those with a smaller new position, standing before the first
    // staying node of that part.
    PositionCounter inFront(seq.size());
    std::size_t anchor = seq.size();  // old position of the first staying node after i (none: the end)
    for (std::size_t i = wanted.size(); i-- > 0;) {
        if (oldPos[i] != SIZE_MAX) inFront.unmark(oldPos[i]);
        if (stays[i]) {
            anchor = oldPos[i];
            continue;
        }
        renderer_.insertChild(host.node, wanted[i], inFront.before(anchor));
    }
    attached = std::move(wanted);
}

Fiber& Reconciler::nearestHost(Fiber& f) {
    Fiber* p = f.parent;
    while (p->kind != ElementKind::Host) p = p->parent;
    return *p;
}

// A NodeRef also learns the backend (for NodeRef::focus); other refs get the node.
void Reconciler::callRef(const RefHandler& ref, NodeHandle node) {
    if (const auto* r = ref.target<NodeRef>()) r->attach(node, &renderer_);
    else ref(node);
}

void Reconciler::commit() {
    for (NodeHandle n : std::exchange(toDestroy_, {})) renderer_.destroyNode(n);
    renderer_.commit();
    if (!runEffects_) {
        effects_.clear();
        refsOff_.clear();
        refsOn_.clear();
        return;
    }

    // Refs before effects, so an effect can use the node. Off first: a keyed
    // element that was replaced hands its ref to the new node.
    for (auto& ref : std::exchange(refsOff_, {})) callRef(ref, 0);
    for (auto& [ref, node] : std::exchange(refsOn_, {}))
        if (hosts_.contains(node)) callRef(ref, node);  // skip a node removed in the same batch

    for (auto& [weak, index] : std::exchange(effects_, {})) {
        auto f = weak.lock();
        if (!f || !f->alive) continue;
        auto& hook = static_cast<EffectHook&>(*f->compPart->hooks[index]);
        if (!hook.pending) continue;
        if (hook.cleanup) std::exchange(hook.cleanup, {})();
        hook.cleanup = std::exchange(hook.pending, {})();
    }
}

}  // namespace rx::detail
