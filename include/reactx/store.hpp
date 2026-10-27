#pragma once

// State that lives outside the tree and is shared by any component that
// reads it (a cart, the signed-in user, settings) — like a tiny Zustand.
//
//   const auto Cart = createStore<std::vector<std::string>>({});
//
//   Cart.update([](auto items) { items.push_back("milk"); return items; });  // in a handler, effect, fetch callback
//   const auto& items = useStore(Cart);                                        // re-renders on every change
//   std::size_t n = useStore(Cart, [](const auto& items) { return items.size(); });  // only when the size changes
//
// Like component state, a store changes only on the UI thread. Outside an
// event handler call Root::flush() (TerminalApp and the browser do it for you).

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "reactx/hooks.hpp"

namespace rx {

template <class T>
class Store {
public:
    explicit Store(T initial) : state_(std::make_shared<State>(std::move(initial))) {}

    const T& get() const { return state_->value; }

    // A new value; readers re-render. An equal value changes nothing.
    void set(T value) const {
        if constexpr (std::equality_comparable<T>) {
            if (value == state_->value) return;
        }
        state_->value = std::move(value);
        ++state_->version;
        auto listeners = state_->listeners;  // a listener may unsubscribe
        for (auto& [id, listener] : listeners) listener();
    }

    // set(f(get())): Cart.update([](auto items) { items.push_back(x); return items; })
    template <class F>
        requires std::is_invocable_r_v<T, F, const T&>
    void update(F&& f) const {
        set(std::invoke(std::forward<F>(f), std::as_const(state_->value)));
    }

    // Grows by one on every change.
    std::uint64_t version() const { return state_->version; }

    // Calls `listener` after every change; call the result to stop.
    Cleanup subscribe(std::function<void()> listener) const {
        std::uint64_t id = state_->nextId++;
        state_->listeners.emplace_back(id, std::move(listener));
        return [weak = std::weak_ptr<State>(state_), id] {
            if (auto s = weak.lock()) std::erase_if(s->listeners, [id](const auto& l) { return l.first == id; });
        };
    }

    // Copies of a Store share the state; equal means the same store.
    bool operator==(const Store& other) const { return state_ == other.state_; }

private:
    struct State {
        T value;
        std::uint64_t version = 0;
        std::uint64_t nextId = 0;
        std::vector<std::pair<std::uint64_t, std::function<void()>>> listeners;
        explicit State(T v) : value(std::move(v)) {}
    };
    std::shared_ptr<State> state_;
};

template <class T>
Store<T> createStore(T initial) {
    return Store<T>(std::move(initial));
}

inline Store<std::string> createStore(const char* initial) { return Store<std::string>(initial); }

// The store's value; the component re-renders after every change. The
// reference stays valid until the store changes.
template <class T>
const T& useStore(const Store<T>& store) {
    auto [seen, setSeen] = useState(store.version());
    (void)seen;
    useEffect(
        [store, setSeen] {
            setSeen(store.version());  // a change between this render and now
            return store.subscribe([store, setSeen] { setSeen(store.version()); });
        },
        deps(store));
    return store.get();
}

// Part of the store's value; the component re-renders only when that part
// changes (compared with ==), not on every change of the store.
template <class T, class F>
    requires std::is_invocable_v<F&, const T&>
std::decay_t<std::invoke_result_t<F&, const T&>> useStore(const Store<T>& store, F&& select) {
    using R = std::decay_t<std::invoke_result_t<F&, const T&>>;
    R now = std::invoke(select, store.get());
    // The newest selector (it may capture props) and what this render showed.
    auto& selector = useRef<std::function<R(const T&)>>();
    selector = std::forward<F>(select);
    auto& shown = useRef<std::optional<R>>();
    shown = now;
    auto [tick, setTick] = useState(std::uint64_t{0});
    (void)tick;
    auto check = [store, &selector, &shown, setTick] {  // the refs live as long as the subscription
        R next = selector(store.get());
        if constexpr (std::equality_comparable<R>) {
            if (shown && *shown == next) return;
        }
        setTick([](std::uint64_t t) { return t + 1; });
    };
    useEffect(
        [store, check] {
            check();
            return store.subscribe(check);
        },
        deps(store));
    return now;
}

}  // namespace rx
