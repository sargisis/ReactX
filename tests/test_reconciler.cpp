// Edge cases of the reconciler and the hooks: useState, useEffect (cleanup
// order), useContext, memo. Every test counts renders / effect runs, so a
// change in behaviour shows up as a number, not as a vague diff.
#include <algorithm>
#include <functional>
#include <random>
#include <sstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "reactx/reactx.hpp"
#include "reactx/text_renderer.hpp"

using namespace rx;

namespace {

int failures = 0;
std::vector<std::pair<std::string, std::function<void()>>> tests;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

#define TEST(name)                                                            \
    void name();                                                              \
    const bool name##_registered = (tests.emplace_back(#name, name), true);   \
    void name()

bool has(const TextRenderer& r, const std::string& part) { return r.toString().find(part) != std::string::npos; }

NodeHandle byId(const TextRenderer& r, const std::string& id) {
    auto h = r.findByProp("id", id);
    if (!h) throw std::runtime_error("no node with id " + id);
    return *h;
}

// A log the components write into; tests compare it with what they expect.
std::vector<std::string> events;
void log(std::string e) { events.push_back(std::move(e)); }
std::string joined() {
    std::string out;
    for (const auto& e : events) out += (out.empty() ? "" : " ") + e;
    return out;
}

// Setters captured from components, to call them "from outside".
std::function<void(int)> setOutside;

// =====================================================================
// useState
// =====================================================================

int stateRenders = 0;
Element StateBox(const Props&) {
    ++stateRenders;
    auto [n, setN] = useState(0);
    setOutside = [setN](int v) { setN(v); };
    return h("box", {}, {
        h("text", {{"id", "n"}}, {"n=" + std::to_string(n)}),
        h("button", {{"id", "three"}, {"onClick", [setN] {
             setN([](int v) { return v + 1; });
             setN([](int v) { return v + 1; });
             setN([](int v) { return v + 1; });
         }}}),
        h("button", {{"id", "same"}, {"onClick", [setN, n] { setN(n); }}}),
    });
}

TEST(several_updates_in_one_event_render_once) {
    TextRenderer r;
    Root root(r);
    stateRenders = 0;
    root.render(h(StateBox));
    CHECK(stateRenders == 1);
    root.dispatch(byId(r, "three"), "onClick");
    CHECK(has(r, "n=3"));      // each updater saw the previous result
    CHECK(stateRenders == 2);  // batched: one re-render, not three
}

TEST(setting_the_same_value_does_not_render) {
    TextRenderer r;
    Root root(r);
    stateRenders = 0;
    root.render(h(StateBox));
    root.dispatch(byId(r, "same"), "onClick");
    CHECK(stateRenders == 1);
}

TEST(setter_from_outside_needs_a_flush) {
    TextRenderer r;
    Root root(r);
    root.render(h(StateBox));
    setOutside(5);
    CHECK(has(r, "n=0"));  // not rendered yet
    root.flush();
    CHECK(has(r, "n=5"));
}

TEST(setter_after_unmount_is_ignored) {
    std::function<void(int)> stale;
    {
        TextRenderer r;
        Root root(r);
        root.render(h(StateBox));
        stale = setOutside;
    }
    stale(42);  // the component is gone: nothing happens, no crash
    CHECK(true);
}

int keepRenders = 0;
Element Keeper(const Props& p) {
    ++keepRenders;
    auto [n, setN] = useState(p.get<int>("start"));  // the initial value is used once
    return h("button", {{"id", "keep-" + p.get<std::string>("name")}, {"onClick", [setN] { setN([](int v) { return v + 1; }); }}},
             {p.get<std::string>("name") + "=" + std::to_string(n)});
}

Element KeeperParent(const Props& p) {
    std::vector<Element> kids;
    for (char c : p.get<std::string>("order")) {
        std::string name(1, c);
        kids.push_back(h(Keeper, {{"key", name}, {"name", name}, {"start", 100}}));
    }
    return h("list", {}, std::move(kids));
}

TEST(state_survives_parent_renders_and_moves_with_its_key) {
    TextRenderer r;
    Root root(r);
    root.render(h(KeeperParent, {{"order", "ab"}}));
    root.dispatch(byId(r, "keep-a"), "onClick");
    CHECK(has(r, "a=101"));
    root.render(h(KeeperParent, {{"order", "ba"}}));  // reordered
    CHECK(has(r, "a=101"));                           // the state went with key "a"
    CHECK(has(r, "b=100"));
    root.render(h(KeeperParent, {{"order", "b"}}));   // "a" removed
    root.render(h(KeeperParent, {{"order", "ab"}}));  // and added again: fresh state
    CHECK(has(r, "a=100"));
}

TEST(changing_the_component_type_resets_state) {
    struct Other {
        static Element render(const Props&) {
            auto [n, _] = useState(7);
            return h("text", {}, {"other=" + std::to_string(n)});
        }
    };
    TextRenderer r;
    Root root(r);
    root.render(h(Keeper, {{"name", "x"}, {"start", 1}}));
    root.dispatch(byId(r, "keep-x"), "onClick");
    CHECK(has(r, "x=2"));
    root.render(h(&Other::render));
    CHECK(has(r, "other=7"));
    root.render(h(Keeper, {{"name", "x"}, {"start", 1}}));
    CHECK(has(r, "x=1"));  // a new Keeper, not the old one
}

Element Spinner(const Props& p) {
    auto [n, setN] = useState(0);
    if (p.get<bool>("forever") || n < 3) setN(n + 1);  // setState during render
    return h("text", {}, {"spun=" + std::to_string(n)});
}

TEST(state_set_during_render_settles_or_stops_with_an_error) {
    {
        TextRenderer r;
        Root root(r);
        root.render(h(Spinner, {{"forever", false}}));
        CHECK(has(r, "spun=3"));  // re-rendered until it stopped asking
    }
    TextRenderer r;
    Root root(r);
    bool threw = false;
    try {
        root.render(h(Spinner, {{"forever", true}}));
    } catch (const std::runtime_error& e) {
        threw = std::string(e.what()).find("too many updates") != std::string::npos;
    }
    CHECK(threw);  // an endless loop is caught, not a hang
}

TEST(hooks_in_a_different_order_are_an_error) {
    struct Flaky {
        static Element render(const Props& p) {
            if (p.get<bool>("extra")) (void)useRef(0);
            (void)useState(0);
            return h("box");
        }
    };
    TextRenderer r;
    Root root(r);
    root.render(h(&Flaky::render, {{"extra", false}}));
    bool threw = false;
    try {
        root.render(h(&Flaky::render, {{"extra", true}}));
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
}

// =====================================================================
// useEffect
// =====================================================================

Element Effects(const Props& p) {
    int dep = p.get<int>("dep");
    std::string name = p.get<std::string>("name", "e");
    useEffect([name, dep] {
        log(name + ":run" + std::to_string(dep));
        return [name, dep] { log(name + ":clean" + std::to_string(dep)); };
    }, deps(dep));
    return h("box");
}

TEST(cleanup_of_the_old_effect_runs_before_the_new_one) {
    events.clear();
    TextRenderer r;
    Root root(r);
    root.render(h(Effects, {{"dep", 1}}));
    root.render(h(Effects, {{"dep", 1}}));  // same deps: nothing
    root.render(h(Effects, {{"dep", 2}}));
    CHECK(joined() == "e:run1 e:clean1 e:run2");
}

TEST(cleanup_runs_once_on_unmount) {
    events.clear();
    {
        TextRenderer r;
        Root root(r);
        root.render(h("app", {}, {h(Effects, {{"dep", 1}})}));
        root.render(h("app", {}, {}));  // unmounted here
        CHECK(joined() == "e:run1 e:clean1");
    }  // the Root goes away: no second cleanup
    CHECK(joined() == "e:run1 e:clean1");
}

TEST(effects_without_deps_run_after_every_render) {
    struct Every {
        static Element render(const Props& p) {
            int v = p.get<int>("v");
            useEffect([v] {
                log("run" + std::to_string(v));
                return [v] { log("clean" + std::to_string(v)); };
            });
            return h("box");
        }
    };
    events.clear();
    TextRenderer r;
    Root root(r);
    root.render(h(&Every::render, {{"v", 1}}));
    root.render(h(&Every::render, {{"v", 2}}));
    root.render(h(&Every::render, {{"v", 3}}));
    CHECK(joined() == "run1 clean1 run2 clean2 run3");
}

TEST(empty_deps_run_once) {
    struct Once {
        static Element render(const Props& p) {
            (void)p.get<int>("v");
            useEffect([] { log("once"); }, deps());
            return h("box");
        }
    };
    events.clear();
    TextRenderer r;
    Root root(r);
    for (int v = 0; v < 4; ++v) root.render(h(&Once::render, {{"v", v}}));
    CHECK(joined() == "once");
}

TEST(effects_see_the_committed_tree) {
    struct Probe {
        static Element render(const Props& p) {
            auto* r = reinterpret_cast<TextRenderer*>(p.get<std::int64_t>("renderer"));
            useEffect([r] { log(r->findByProp("id", "probe") ? "mounted" : "missing"); }, deps());
            return h("box", {{"id", "probe"}});
        }
    };
    events.clear();
    TextRenderer r;
    Root root(r);
    root.render(h(&Probe::render, {{"renderer", reinterpret_cast<std::int64_t>(&r)}}));
    CHECK(joined() == "mounted");  // after commit, the node exists in the backend
}

TEST(effect_that_sets_state_renders_again_and_settles) {
    struct Loader {
        static Element render(const Props&) {
            auto [data, setData] = useState(std::string("loading"));
            useEffect([setData] { setData(std::string("loaded")); }, deps());
            return h("text", {}, {data});
        }
    };
    TextRenderer r;
    Root root(r);
    root.render(h(&Loader::render));
    CHECK(has(r, "\"loaded\""));
}

TEST(nested_effects_all_clean_up_when_the_parent_goes) {
    events.clear();
    TextRenderer r;
    Root root(r);
    root.render(h("app", {}, {h("box", {}, {h(Effects, {{"dep", 1}, {"name", "a"}}), h(Effects, {{"dep", 1}, {"name", "b"}})})}));
    root.render(h("app", {}, {}));
    // Both ran and both cleaned up, each exactly once.
    int runs = 0, cleans = 0;
    for (const auto& e : events) {
        runs += e.ends_with(":run1");
        cleans += e.ends_with(":clean1");
    }
    CHECK(runs == 2 && cleans == 2);
}

// =====================================================================
// Keyed lists: the backend gets the right order with the fewest moves
// =====================================================================

// Length of the longest strictly increasing subsequence (the slow, obvious way).
std::size_t lisLength(const std::vector<int>& v) {
    std::vector<std::size_t> best(v.size(), 1);
    std::size_t out = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j)
            if (v[j] < v[i]) best[i] = std::max(best[i], best[j] + 1);
        out = std::max(out, best[i]);
    }
    return out;
}

Element keyedList(const std::vector<int>& ids) {
    std::vector<Element> items;
    for (int id : ids) items.push_back(h("i", {{"key", std::to_string(id)}}, {std::to_string(id)}));
    return h("list", {{"id", "list"}}, std::move(items));
}

TEST(keyed_lists_reorder_with_the_fewest_moves) {
    std::mt19937 rng(7);
    for (int round = 0; round < 400; ++round) {
        std::vector<int> pool(40);
        for (int i = 0; i < 40; ++i) pool[static_cast<std::size_t>(i)] = i;
        auto pick = [&] {
            std::ranges::shuffle(pool, rng);
            std::vector<int> ids(pool.begin(), pool.begin() + std::uniform_int_distribution<int>(0, 30)(rng));
            if (rng() % 3 == 0) std::ranges::sort(ids);  // often mostly in order, like real lists
            return ids;
        };
        std::vector<int> before = pick(), after = pick();
        if (rng() % 2) {  // or a small edit of `before`
            after = before;
            if (!after.empty() && rng() % 2) after.erase(after.begin() + static_cast<long>(rng() % after.size()));
            if (after.size() > 1) std::swap(after[rng() % after.size()], after[rng() % after.size()]);
            if (rng() % 2) after.insert(after.begin() + static_cast<long>(rng() % (after.size() + 1)), 100 + round);
        }

        std::ostringstream log;
        TextRenderer r(&log);
        Root root(r);
        root.render(keyedList(before));
        std::string list = "#" + std::to_string(byId(r, "list"));
        log.str("");
        root.render(keyedList(after));

        TextRenderer fresh;  // the same list mounted from scratch
        Root freshRoot(fresh);
        freshRoot.render(keyedList(after));
        CHECK(r.toString() == fresh.toString());

        // Inserts into the list: every new item once, plus the fewest moves
        // of kept items (all but a longest run that is already in order).
        std::vector<int> keptNewPositions;
        std::size_t added = 0;
        for (int id : after)
            if (std::ranges::find(before, id) == before.end()) ++added;
        for (int id : before)
            if (auto it = std::ranges::find(after, id); it != after.end())
                keptNewPositions.push_back(static_cast<int>(it - after.begin()));
        std::size_t inserts = 0;
        std::istringstream lines(log.str());
        for (std::string line; std::getline(lines, line);)
            if (line.find("] insert ") != std::string::npos && line.find(" into " + list + " ") != std::string::npos) ++inserts;
        CHECK(inserts == added + keptNewPositions.size() - lisLength(keptNewPositions));
        if (failures) {
            std::cerr << "  round " << round << " inserts " << inserts << " added " << added << " kept "
                      << keptNewPositions.size() << " lis " << lisLength(keptNewPositions) << "\n" << log.str().substr(0, 600);
            return;
        }
    }
}

// Random trees: rendering one over another gives what rendering it from
// scratch gives (mixes keyed and unkeyed children, fragments, text and
// components, with types changing under the same key).
Element Wrap(const Props& p) {
    auto [n, setN] = useState(0);  // state that must not leak into another instance
    (void)setN;
    return h("w", {{"tag", p.get<std::string>("tag", "")}}, {"n=" + std::to_string(n)});
}

Element randomTree(std::mt19937& rng, int depth) {
    auto pick = [&](int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); };
    std::vector<Element> children;
    int count = depth > 3 ? 0 : pick(5);
    std::vector<int> keys = {0, 1, 2, 3, 4, 5};
    std::ranges::shuffle(keys, rng);
    for (int i = 0; i < count; ++i) {
        std::string key = pick(3) ? std::to_string(keys[static_cast<std::size_t>(i)]) : "";  // some unkeyed
        switch (pick(4)) {
            case 0: children.push_back(h("t", {{"key", key}}, {"text" + std::to_string(pick(3))})); break;
            case 1: children.push_back(h(Wrap, {{"key", key}, {"tag", std::to_string(pick(3))}})); break;
            case 2: children.push_back(fragment({randomTree(rng, depth + 1), "frag"}, key)); break;
            default: {
                Element e = randomTree(rng, depth + 1);
                e.key = key;
                children.push_back(std::move(e));
            }
        }
    }
    return h(pick(2) ? "a" : "b", {{"v", static_cast<std::int64_t>(pick(3))}}, std::move(children));
}

TEST(random_trees_render_over_each_other_like_from_scratch) {
    std::mt19937 rng(2026);
    TextRenderer r;
    Root root(r);
    for (int round = 0; round < 1500; ++round) {
        Element tree = randomTree(rng, 0);
        root.render(tree);
        TextRenderer fresh;
        Root freshRoot(fresh);
        freshRoot.render(tree);
        CHECK(r.toString() == fresh.toString());
        if (failures) {
            std::cerr << "  round " << round << "\n";
            return;
        }
    }
}

// =====================================================================
// errorBoundary: a component that throws doesn't take the app down
// =====================================================================

bool explode = false;
int bombCleanups = 0;

struct BombProps {
    int at = 1;  // throws once the counter reaches this
};

Element Bomb(const BombProps& p) {
    auto [n, setN] = useState(0);
    useEffect([] { return [] { ++bombCleanups; }; }, deps());
    if (explode || n >= p.at) throw std::runtime_error("boom at " + std::to_string(n));
    return h("box", {}, {h("button", {{"id", "bump"}, {"onClick", [setN] { setN([](int v) { return v + 1; }); }}}, {"bump"}),
                         h("text", {}, {"n=" + std::to_string(n)})});
}

Element showError(const RenderError& e) {
    return h("box", {{"id", "fallback"}},
             {h("text", {}, {"error: " + e.message}), h("button", {{"id", "retry"}, {"onClick", e.retry}}, {"retry"})});
}

TEST(boundary_shows_the_fallback_when_the_first_render_throws) {
    explode = true;
    TextRenderer r;
    Root root(r);
    root.render(h("app", {}, {h("text", {}, {"header"}), errorBoundary(showError, {h(Bomb, {.at = 1})})}));
    CHECK(has(r, "header"));
    CHECK(has(r, "error: boom at 0"));
    explode = false;
}

TEST(boundary_catches_a_throw_on_a_state_update_and_retry_recovers) {
    bombCleanups = 0;
    TextRenderer r;
    Root root(r);
    root.render(h("app", {}, {errorBoundary(showError, {h(Bomb, {.at = 2})}), h("text", {}, {"footer"})}));
    root.dispatch(byId(r, "bump"), "onClick");
    CHECK(has(r, "n=1"));
    root.dispatch(byId(r, "bump"), "onClick");  // n = 2: Bomb throws while re-rendering
    CHECK(has(r, "error: boom at 2"));
    CHECK(has(r, "footer") && !has(r, "n="));
    CHECK(bombCleanups == 1);  // the broken subtree was unmounted properly
    CHECK(!r.findByProp("id", "bump"));

    root.dispatch(byId(r, "retry"), "onClick");  // a fresh Bomb, state from 0
    CHECK(has(r, "n=0") && !has(r, "error:"));
    root.dispatch(byId(r, "bump"), "onClick");
    CHECK(has(r, "n=1"));
}

TEST(a_throwing_fallback_goes_to_the_next_boundary_up) {
    explode = true;
    TextRenderer r;
    Root root(r);
    auto badFallback = [](const RenderError&) -> Element { throw std::logic_error("fallback broke too"); };
    root.render(errorBoundary(showError, {h("box", {}, {errorBoundary(badFallback, {h(Bomb, {})})})}));
    CHECK(has(r, "error: fallback broke too"));
    explode = false;
}

TEST(non_std_exceptions_are_caught_too) {
    TextRenderer r;
    Root root(r);
    struct Odd {};
    root.render(errorBoundary(showError, {h([](const Props&) -> Element { throw Odd{}; })}));
    CHECK(has(r, "error: unknown error"));
}

TEST(without_a_boundary_a_failed_mount_leaves_nothing_behind) {
    explode = true;
    bombCleanups = 0;
    TextRenderer r;
    Root root(r);
    bool threw = false;
    try {
        root.render(h("app", {}, {h("text", {}, {"before"}), h("box", {}, {h(Bomb, {})})}));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    explode = false;
    CHECK(threw);
    root.render(h("app", {}, {h("text", {}, {"recovered"})}));
    CHECK(has(r, "recovered") && !has(r, "before"));
    CHECK(r.nodeCount() <= 4);  // root, app, text and its string: the failed nodes are gone
}

std::function<void()> bumpBomb, bumpDeep;

Element BombHandle(const Props&) {
    auto [n, setN] = useState(0);
    bumpBomb = [setN] { setN([](int v) { return v + 1; }); };
    if (n >= 1) throw std::runtime_error("bomb");
    return h("text", {}, {"bomb=" + std::to_string(n)});
}

Element DeepCounter(const Props&) {
    auto [n, setN] = useState(0);
    bumpDeep = [setN] { setN([](int v) { return v + 1; }); };
    return h("text", {}, {"deep=" + std::to_string(n)});
}

TEST(without_a_boundary_a_failed_update_keeps_the_last_output_and_the_other_updates) {
    TextRenderer r;
    Root root(r);
    root.render(h("app", {}, {h(BombHandle), h("box", {}, {h("box", {}, {h(DeepCounter)})})}));
    bumpBomb();  // one batch: BombHandle (renders first, throws), then DeepCounter (deeper)
    bumpDeep();
    bool threw = false;
    try {
        root.flush();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(has(r, "bomb=0"));  // the last good output stays
    root.flush();             // the counter's update wasn't lost with the failed batch
    CHECK(has(r, "deep=1"));
    bumpDeep();
    root.flush();
    CHECK(has(r, "deep=2"));
}

// =====================================================================
// createStore / useStore, useId
// =====================================================================

struct CartItem {
    std::string name;
    int qty = 1;
    bool operator==(const CartItem&) const = default;
};
const auto Cart = createStore<std::vector<CartItem>>({});
int listRenders = 0, countRenders = 0, otherRenders = 0;

Element CartList(const Props&) {
    ++listRenders;
    std::string names;
    for (const auto& item : useStore(Cart)) names += item.name + ";";
    return h("text", {}, {"items=" + names});
}

Element CartCount(const Props&) {
    ++countRenders;
    auto n = useStore(Cart, [](const std::vector<CartItem>& items) { return items.size(); });
    return h("text", {}, {"count=" + std::to_string(n)});
}

Element NotReading(const Props&) {
    ++otherRenders;
    return h("text", {}, {"other"});
}

TEST(store_readers_render_on_change_and_selectors_only_when_their_part_changes) {
    Cart.set({});
    listRenders = countRenders = otherRenders = 0;
    TextRenderer r;
    Root root(r);
    root.render(h("box", {}, {h(CartList), h(CartCount), h(NotReading)}));
    CHECK(listRenders == 1 && countRenders == 1 && otherRenders == 1);

    Cart.update([](auto items) {
        items.push_back({"milk"});
        return items;
    });
    root.flush();
    CHECK(has(r, "items=milk;") && has(r, "count=1"));
    CHECK(listRenders == 2 && countRenders == 2 && otherRenders == 1);

    Cart.update([](auto items) {  // same size: the counter doesn't render
        items[0].qty = 2;
        return items;
    });
    root.flush();
    CHECK(listRenders == 3 && countRenders == 2);

    Cart.set(Cart.get());  // an equal value changes nothing
    root.flush();
    CHECK(listRenders == 3);
}

TEST(store_readers_unsubscribe_on_unmount) {
    Cart.set({});
    listRenders = 0;
    TextRenderer r;
    Root root(r);
    root.render(h("box", {}, {h(CartList)}));
    root.render(h("box", {}, {}));
    Cart.update([](auto items) {
        items.push_back({"tea"});
        return items;
    });
    root.flush();
    CHECK(listRenders == 1);
}

TEST(store_change_between_render_and_subscribe_is_not_missed) {
    Cart.set({});
    TextRenderer r;
    Root root(r);
    struct Sneaky {
        static Element render(const Props&) {
            useEffect([] {  // runs before CartList's effect subscribes (siblings: first one first)
                Cart.update([](auto items) {
                    items.push_back({"late"});
                    return items;
                });
            }, deps());
            return h("text", {}, {"sneaky"});
        }
    };
    root.render(h("box", {}, {h(Sneaky::render), h(CartList), h(CartCount)}));
    CHECK(has(r, "items=late;") && has(r, "count=1"));
}

TEST(use_id_is_unique_and_stable) {
    struct Ids {
        static Element render(const Props&) { return h("label", {{"for", useId()}}, {}); }
    };
    TextRenderer r;
    Root root(r);
    root.render(h("box", {}, {h(Ids::render), h(Ids::render)}));
    std::string first = r.toString();
    root.render(h("box", {}, {h(Ids::render), h(Ids::render)}));
    CHECK(r.toString() == first);  // the same ids on the next render
    auto a = first.find("for=\"rx-"), b = first.find("for=\"rx-", a + 1);
    CHECK(a != std::string::npos && b != std::string::npos);
    CHECK(first.substr(a, 12) != first.substr(b, 12));
}

// =====================================================================
// Lambdas as typed components (found by an AI agent writing PulseBoard)
// =====================================================================

struct BadgeProps {
    std::string label{};
    int start = 0;
    std::string key{};
};
const auto Badge = [](const BadgeProps& p) -> Element {
    auto [clicks, setClicks] = useState(p.start);
    return h("badge", {{"id", "badge-" + p.label}, {"onClick", [setClicks] { setClicks([](int c) { return c + 1; }); }}},
             {p.label + "=" + std::to_string(clicks)});
};

struct PanelProps {
    std::string title{};
    std::vector<Element> children{};
};
const auto Panel = [](const PanelProps& p) -> Element { return h("panel", {{"title", p.title}}, p.children); };

TEST(a_lambda_works_as_a_typed_component) {
    TextRenderer r;
    Root root(r);
    auto app = [](const std::string& extra) {
        return h("div", {}, {
            errorBoundary(showError, {h(Badge, {.label = "a", .start = 5})}),  // the reported case
            h(Panel, {.title = "p"}, {h(Badge, {.label = extra, .start = 0, .key = extra})}),
            h(Badge),  // default props
        });
    };
    root.render(app("b"));
    CHECK(has(r, "a=5") && has(r, "b=0") && has(r, "=0"));
    root.dispatch(byId(r, "badge-a"), "onClick");
    root.render(app("b"));  // the same lambda is the same component: state stays
    CHECK(has(r, "a=6"));
    CHECK(has(r, "title=\"p\""));
}

// =====================================================================
// useContext
// =====================================================================

const auto Color = createContext<std::string>("none");
const auto Size = createContext<int>(0);
int colorReads = 0;
int quietRenders = 0;

Element ColorLabel(const Props& p) {
    ++colorReads;
    return h("text", {{"id", p.get<std::string>("id", "color")}}, {"color=" + useContext(Color)});
}

Element Quiet(const Props&) {  // does not read any context
    ++quietRenders;
    return h("text", {}, {"quiet"});
}

Element QuietWall(const Props&) { return h("box", {}, {h(memo<Quiet>), h(ColorLabel)}); }

TEST(context_value_change_reaches_readers_through_memo) {
    TextRenderer r;
    Root root(r);
    colorReads = quietRenders = 0;
    root.render(Color.provide("red", {h(memo<QuietWall>)}));
    CHECK(has(r, "color=red") && colorReads == 1 && quietRenders == 1);
    root.render(Color.provide("red", {h(memo<QuietWall>)}));  // same value
    CHECK(colorReads == 1);
    root.render(Color.provide("blue", {h(memo<QuietWall>)}));
    CHECK(has(r, "color=blue"));
    CHECK(colorReads == 2);    // the reader re-rendered
    CHECK(quietRenders == 1);  // the memo sibling that doesn't read it did not
}

TEST(nearest_provider_wins_and_contexts_are_independent) {
    struct Both {
        static Element render(const Props&) {
            return h("text", {}, {useContext(Color) + "/" + std::to_string(useContext(Size))});
        }
    };
    TextRenderer r;
    Root root(r);
    root.render(Color.provide("outer", {Size.provide(1, {
        h(&Both::render),
        Color.provide("inner", {h(&Both::render)}),
    })}));
    CHECK(has(r, "\"outer/1\""));
    CHECK(has(r, "\"inner/1\""));
}

TEST(reader_that_moves_out_of_a_provider_gets_the_default) {
    TextRenderer r;
    Root root(r);
    root.render(h("app", {}, {Color.provide("green", {h(ColorLabel)})}));
    CHECK(has(r, "color=green"));
    root.render(h("app", {}, {h(ColorLabel)}));
    CHECK(has(r, "color=none"));
}

TEST(context_with_a_value_that_cannot_be_compared_always_updates) {
    struct Blob {
        std::function<int()> get;  // std::function has no ==
    };
    static const auto BlobCtx = createContext<Blob>(Blob{[] { return 0; }});
    struct Reader {
        static Element render(const Props&) { return h("text", {}, {"v=" + std::to_string(useContext(BlobCtx).get())}); }
    };
    TextRenderer r;
    Root root(r);
    root.render(BlobCtx.provide(Blob{[] { return 1; }}, {h(memo<&Reader::render>)}));
    CHECK(has(r, "v=1"));
    root.render(BlobCtx.provide(Blob{[] { return 2; }}, {h(memo<&Reader::render>)}));
    CHECK(has(r, "v=2"));
}

// =====================================================================
// memo
// =====================================================================

int memoRenders = 0;
Element Counted(const Props& p) {
    ++memoRenders;
    return h("text", {}, {"label=" + p.get<std::string>("label")});
}

void renderMemo(Root& root, PropList props) { root.render(h("app", {}, {h(memo<Counted>, props)})); }

TEST(memo_compares_every_prop_type) {
    TextRenderer r;
    Root root(r);
    memoRenders = 0;
    renderMemo(root, {{"label", "a"}, {"n", 1}, {"x", 0.5}, {"on", true}});
    renderMemo(root, {{"label", "a"}, {"n", 1}, {"x", 0.5}, {"on", true}});
    CHECK(memoRenders == 1);  // all equal: skipped
    renderMemo(root, {{"label", "a"}, {"n", 2}, {"x", 0.5}, {"on", true}});
    CHECK(memoRenders == 2);  // int changed
    renderMemo(root, {{"label", "a"}, {"n", 2}, {"x", 0.25}, {"on", true}});
    CHECK(memoRenders == 3);  // double changed
    renderMemo(root, {{"label", "a"}, {"n", 2}, {"x", 0.25}, {"on", false}});
    CHECK(memoRenders == 4);  // bool changed
    renderMemo(root, {{"label", "b"}, {"n", 2}, {"x", 0.25}, {"on", false}});
    CHECK(memoRenders == 5 && has(r, "label=b"));  // string changed
}

TEST(memo_notices_added_removed_and_retyped_props) {
    TextRenderer r;
    Root root(r);
    memoRenders = 0;
    renderMemo(root, {{"label", "a"}});
    renderMemo(root, {{"label", "a"}, {"extra", 1}});
    CHECK(memoRenders == 2);  // added
    renderMemo(root, {{"label", "a"}});
    CHECK(memoRenders == 3);  // removed
    renderMemo(root, {{"label", "a"}, {"extra", 1}});
    renderMemo(root, {{"label", "a"}, {"extra", "1"}});
    CHECK(memoRenders == 5);  // same text, different type: int 1 vs string "1"
}

TEST(memo_with_handlers) {
    struct Parent {
        static Element render(const Props& p) {
            auto [tick, setTick] = useState(0);
            Callback stable = useCallback([] {}, deps());
            Callback fresh = [] {};
            return h("box", {}, {
                h("button", {{"id", "tick"}, {"onClick", [setTick] { setTick([](int t) { return t + 1; }); }}}),
                h(memo<Counted>, {{"label", "x"}, {"onClick", p.get<bool>("stable") ? stable : fresh}}),
            });
        }
    };
    for (bool stable : {true, false}) {
        TextRenderer r;
        Root root(r);
        memoRenders = 0;
        root.render(h(&Parent::render, {{"stable", stable}}));
        root.dispatch(byId(r, "tick"), "onClick");
        root.dispatch(byId(r, "tick"), "onClick");
        CHECK(memoRenders == (stable ? 1 : 3));  // useCallback keeps memo working; a fresh lambda doesn't
    }
}

TEST(memo_with_children_always_renders) {
    TextRenderer r;
    Root root(r);
    memoRenders = 0;
    root.render(h("app", {}, {h(memo<Counted>, {{"label", "a"}}, {"child"})}));
    root.render(h("app", {}, {h(memo<Counted>, {{"label", "a"}}, {"child"})}));
    CHECK(memoRenders == 2);  // children can't be compared cheaply, so no skipping
}

TEST(memo_of_different_components_are_different_types) {
    struct A {
        static Element render(const Props&) {
            auto [n, setN] = useState(0);
            return h("button", {{"id", "a"}, {"onClick", [setN] { setN(1); }}}, {"A" + std::to_string(n)});
        }
    };
    struct B {
        static Element render(const Props&) { return h("text", {}, {"B"}); }
    };
    TextRenderer r;
    Root root(r);
    root.render(h(memo<&A::render>));
    root.dispatch(byId(r, "a"), "onClick");
    CHECK(has(r, "A1"));
    root.render(h(memo<&B::render>));
    CHECK(has(r, "\"B\""));
    root.render(h(memo<&A::render>));
    CHECK(has(r, "A0"));  // switched type: a fresh A
}

// =====================================================================
// typed components: h(Counter, {.start = 5})
// =====================================================================

struct CounterProps {
    int start = 0;
    std::string label = "count";
    bool operator==(const CounterProps&) const = default;
};

int typedRenders = 0;
Element TypedCounter(const CounterProps& p) {
    ++typedRenders;
    auto [n, setN] = useState(p.start);
    return h("box", {}, {
        h("text", {{"id", "t"}}, {p.label + "=" + std::to_string(n)}),
        h("button", {{"id", "inc"}, {"onClick", [setN] { setN([](int v) { return v + 1; }); }}}),
    });
}

struct OtherProps {
    int start = 0;
};
Element OtherCounter(const OtherProps& p) {  // same struct shape, different component
    auto [n, setN] = useState(p.start * 100);
    (void)setN;
    return h("text", {{"id", "t"}}, {"other=" + std::to_string(n)});
}

TEST(typed_component_renders_its_struct) {
    TextRenderer r;
    Root root(r);
    root.render(h(TypedCounter, {.start = 5, .label = "clicks"}));
    CHECK(has(r, "clicks=5"));
    root.render(h(TypedCounter, {.start = 2}));  // defaults for the rest
    CHECK(has(r, "count=5"));                   // state kept: same component
}

TEST(typed_component_keeps_state_and_sees_new_props) {
    TextRenderer r;
    Root root(r);
    root.render(h(TypedCounter, {.label = "a"}));
    root.dispatch(byId(r, "inc"), "onClick");
    CHECK(has(r, "a=1"));
    root.render(h(TypedCounter, {.label = "b"}));
    CHECK(has(r, "b=1"));
}

TEST(different_typed_components_are_different_types) {
    TextRenderer r;
    Root root(r);
    root.render(h(TypedCounter, {.start = 1}));
    root.dispatch(byId(r, "inc"), "onClick");
    CHECK(has(r, "count=2"));
    root.render(h(OtherCounter, {.start = 1}));  // new component: new state
    CHECK(has(r, "other=100"));
    root.render(h(TypedCounter, {.start = 7}));
    CHECK(has(r, "count=7"));
}

struct CardProps {
    std::string title{};
    std::vector<Element> children{};
};
Element Card(const CardProps& p) {
    std::vector<Element> body{h("text", {}, {p.title})};
    for (const auto& c : p.children) body.push_back(c);
    return h("box", {{"id", "card"}}, std::move(body));
}

TEST(typed_component_gets_children_through_its_struct) {
    TextRenderer r;
    Root root(r);
    root.render(h(Card, {.title = "Hi"}, {h("text", {}, {"first"}), h("text", {}, {"second"})}));
    CHECK(has(r, "\"Hi\"") && has(r, "\"first\"") && has(r, "\"second\""));
}

struct ItemProps {
    std::string key{};  // becomes the element key
    std::string text{};
};
Element Item(const ItemProps& p) {
    auto [clicks, setClicks] = useState(0);
    return h("button", {{"id", p.key}, {"onClick", [setClicks] { setClicks([](int c) { return c + 1; }); }}},
             {p.text + ":" + std::to_string(clicks)});
}

TEST(typed_key_field_keeps_state_when_items_move) {
    TextRenderer r;
    Root root(r);
    root.render(h("box", {}, {h(Item, {.key = "a", .text = "A"}), h(Item, {.key = "b", .text = "B"})}));
    root.dispatch(byId(r, "b"), "onClick");
    CHECK(has(r, "B:1"));
    root.render(h("box", {}, {h(Item, {.key = "b", .text = "B"}), h(Item, {.key = "a", .text = "A"})}));
    CHECK(has(r, "B:1") && has(r, "A:0"));  // state moved with the key
}

TEST(memo_of_a_typed_component_compares_structs) {
    TextRenderer r;
    Root root(r);
    typedRenders = 0;
    root.render(h("box", {}, {h(memo<TypedCounter>, {.start = 1})}));
    CHECK(typedRenders == 1);
    root.render(h("box", {}, {h(memo<TypedCounter>, {.start = 1})}));  // equal struct: skipped
    CHECK(typedRenders == 1);
    root.render(h("box", {}, {h(memo<TypedCounter>, {.start = 1, .label = "x"})}));
    CHECK(typedRenders == 2 && has(r, "x=1"));
    root.dispatch(byId(r, "inc"), "onClick");  // own state still renders it
    CHECK(typedRenders == 3 && has(r, "x=2"));

    // Without memo the same parent render calls it every time.
    root.render(h("box", {}, {h(TypedCounter, {.start = 1})}));
    int before = typedRenders;
    root.render(h("box", {}, {h(TypedCounter, {.start = 1})}));
    CHECK(typedRenders == before + 1);
}

Element OldStyle(const Props& p) { return h("text", {}, {"old " + p.get<std::string>("name")}); }

TEST(typed_and_props_components_mix) {
    TextRenderer r;
    Root root(r);
    root.render(h("box", {}, {h(OldStyle, {{"name", "x"}}), h(TypedCounter, {.label = "new"})}));
    CHECK(has(r, "old x") && has(r, "new=0"));
    root.render(h("box", {}, {h(TypedCounter, {.label = "new"}), h(OldStyle, {{"name", "y"}})}));
    CHECK(has(r, "old y") && has(r, "new=0"));
}

struct DebounceProps {
    std::string text;
};

Element DebounceBox(const DebounceProps& p) {
    std::string debounced = useDebounce(p.text, std::chrono::milliseconds(50));
    return h("text", {}, {"debounced=" + debounced});
}

TEST(use_debounce_hook) {
    TextRenderer r;
    Root root(r);
    root.render(h(DebounceBox, {.text = "initial"}));
    CHECK(has(r, "debounced=initial"));

    root.render(h(DebounceBox, {.text = "updated"}));
    // Immediately after update, debounce has not expired yet
    CHECK(has(r, "debounced=initial"));

    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    root.flush();
    CHECK(has(r, "debounced=updated"));
}

struct StorageProps {
    std::string key;
};

std::function<void(std::string)> setStoredOutside;

Element StorageBox(const StorageProps& p) {
    auto [val, setVal] = useLocalStorage(p.key, std::string("default_val"));
    setStoredOutside = setVal;
    return h("text", {}, {"stored=" + val});
}

TEST(use_local_storage_hook) {
    TextRenderer r;
    Root root(r);
    root.render(h(StorageBox, {.key = "test_key_1"}));
    CHECK(has(r, "stored=default_val"));

    setStoredOutside("custom_val");
    root.flush();
    CHECK(has(r, "stored=custom_val"));

    // Mounting a fresh instance with the same key reads the persisted value
    TextRenderer r2;
    Root root2(r2);
    root2.render(h(StorageBox, {.key = "test_key_1"}));
    CHECK(has(r2, "stored=custom_val"));
}

Element WindowSizeBox(const Props&) {
    WindowSize ws = useWindowSize();
    return h("text", {}, {std::to_string(ws.width) + "x" + std::to_string(ws.height)});
}

TEST(use_window_size_hook) {
    TextRenderer r;
    Root root(r);
    root.render(h(WindowSizeBox));
    CHECK(has(r, "x"));
    detail::notifyResize({120, 40});  // what TerminalApp / the browser report
    root.flush();
    CHECK(has(r, "120x40"));
}

TEST(debounce_waits_for_the_value_to_settle) {
    TextRenderer r;
    Root root(r);
    root.render(h(DebounceBox, {.text = "a"}));
    root.render(h(DebounceBox, {.text = "ab"}));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    root.render(h(DebounceBox, {.text = "abc"}));  // restarts the 50 ms wait
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    root.flush();  // 60 ms after "ab", 30 after "abc": nothing yet
    CHECK(has(r, "debounced=a\""));
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    root.flush();
    CHECK(has(r, "debounced=abc"));  // "ab" was never shown
}

TEST(debounce_timer_is_cancelled_on_unmount) {
    TextRenderer r;
    Root root(r);
    root.render(h(DebounceBox, {.text = "x"}));
    root.render(h(DebounceBox, {.text = "y"}));
    root.render(h("box"));  // unmounted with a timer pending
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    root.flush();  // the cancelled timer must not run (nor touch the dead state)
    CHECK(!has(r, "debounced"));
}

Element NumberStore(const Props&) {
    auto [n, setN] = useLocalStorage("test_number", 7);
    setOutside = [setN](int v) { setN(v); };
    return h("text", {}, {"n=" + std::to_string(n)});
}

TEST(local_storage_keeps_numbers) {
    {
        TextRenderer r;
        Root root(r);
        root.render(h(NumberStore));
        CHECK(has(r, "n=7"));
        setOutside(42);
        root.flush();
    }
    TextRenderer r;
    Root root(r);
    root.render(h(NumberStore));  // a new mount reads the stored 42
    CHECK(has(r, "n=42"));
}

struct PageProps {
    std::string title = "home";
};
Element Page(const PageProps& p) { return h("text", {}, {"page " + p.title}); }

TEST(typed_component_without_props_gets_defaults) {
    TextRenderer r;
    Root root(r);
    root.render(h("box", {}, {h(Page)}));  // like router::routes({{"/", h(Home)}})
    CHECK(has(r, "page home"));
}

// =====================================================================
// v0.5: useReducer, useTimeout, useInterval, useKeyboard, onScroll
// =====================================================================

enum class CounterAction { Inc, Dec, Reset };
std::function<void(CounterAction)> dispatchOutside;

struct ReducerProps {
    int step = 1;  // the reducer captures it: the newest reducer must be used
};
Element ReducerBox(const ReducerProps& p) {
    auto [n, dispatch] = useReducer(
        [step = p.step](const int& v, CounterAction a) {
            switch (a) {
                case CounterAction::Inc: return v + step;
                case CounterAction::Dec: return v - step;
                default: return 0;
            }
        },
        0);
    dispatchOutside = [dispatch](CounterAction a) { dispatch(a); };
    return h("text", {}, {"n=" + std::to_string(n)});
}

TEST(use_reducer_runs_actions_with_the_newest_reducer) {
    TextRenderer r;
    Root root(r);
    root.render(h(ReducerBox, {.step = 1}));
    dispatchOutside(CounterAction::Inc);
    dispatchOutside(CounterAction::Inc);
    root.flush();
    CHECK(has(r, "n=2"));
    root.render(h(ReducerBox, {.step = 10}));
    dispatchOutside(CounterAction::Inc);
    root.flush();
    CHECK(has(r, "n=12"));
    dispatchOutside(CounterAction::Reset);
    root.flush();
    CHECK(has(r, "n=0"));
    root.render(h("box"));  // unmounted: dispatch is ignored, no crash
    dispatchOutside(CounterAction::Inc);
    root.flush();
}

int timeoutFired = 0;
struct TimeoutProps {
    std::optional<std::chrono::milliseconds> delay{};
};
Element TimeoutBox(const TimeoutProps& p) {
    useTimeout([] { ++timeoutFired; }, p.delay);
    return h("text", {}, {"t"});
}

TEST(use_timeout_fires_once_and_can_be_switched_off) {
    using namespace std::chrono_literals;
    timeoutFired = 0;
    TextRenderer r;
    Root root(r);
    root.render(h(TimeoutBox, {.delay = 20ms}));
    root.flush();
    CHECK(timeoutFired == 0);
    std::this_thread::sleep_for(40ms);
    root.flush();
    CHECK(timeoutFired == 1);
    std::this_thread::sleep_for(40ms);
    root.flush();
    CHECK(timeoutFired == 1);  // once

    root.render(h(TimeoutBox, {.delay = std::nullopt}));  // off
    root.render(h(TimeoutBox, {.delay = 20ms}));           // a new delay: a new timer
    root.render(h(TimeoutBox, {}));                        // ...cancelled right away
    std::this_thread::sleep_for(40ms);
    root.flush();
    CHECK(timeoutFired == 1);

    root.render(h(TimeoutBox, {.delay = 20ms}));
    root.render(h("box"));  // unmount cancels
    std::this_thread::sleep_for(40ms);
    root.flush();
    CHECK(timeoutFired == 1);
}

struct IntervalProps {
    std::optional<std::chrono::milliseconds> period{};
};
Element IntervalBox(const IntervalProps& p) {
    auto [ticks, setTicks] = useState(0);
    useInterval([setTicks] { setTicks([](int t) { return t + 1; }); }, p.period);
    return h("text", {}, {"ticks=" + std::to_string(ticks)});
}

int ticksShown(const TextRenderer& r) {
    auto s = r.toString();
    auto at = s.find("ticks=");
    if (at == std::string::npos) return -1;
    std::string_view rest = std::string_view(s).substr(at + 6);
    return rx::parseNumber<int>(rest.substr(0, rest.find('"'))).value_or(-1);
}

TEST(use_interval_repeats_until_paused_or_unmounted) {
    using namespace std::chrono_literals;
    TextRenderer r;
    Root root(r);
    root.render(h(IntervalBox, {.period = 10ms}));
    for (int i = 0; i < 4; ++i) {
        std::this_thread::sleep_for(15ms);
        root.flush();
    }
    int afterRunning = ticksShown(r);
    CHECK(afterRunning >= 3 && afterRunning <= 4);
    root.render(h(IntervalBox, {}));  // paused
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(15ms);
        root.flush();
    }
    CHECK(ticksShown(r) == afterRunning);
    root.render(h(IntervalBox, {.period = 10ms}));
    root.render(h("box"));  // unmounted: the chain stops
    std::this_thread::sleep_for(30ms);
    root.flush();
}

std::vector<std::string> shortcuts;
struct ShortcutProps {
    std::string name{};
};
Element Shortcuts(const ShortcutProps& p) {
    useKeyboard([name = p.name](const KeyEvent& k) {
        if (k.ctrl && k.key == "z") {
            shortcuts.push_back(name + ":undo");
            return true;
        }
        if (k.key == "/") {
            shortcuts.push_back(name + ":search");
            return true;
        }
        return false;
    });
    return h("text", {}, {"s"});
}

TEST(use_keyboard_hears_app_wide_shortcuts) {
    shortcuts.clear();
    TextRenderer r;
    Root root(r);
    root.render(h(Shortcuts, {.name = "a"}));
    CHECK(detail::dispatchGlobalKey(KeyEvent{.key = "z", .ctrl = true}, false));
    CHECK(!detail::dispatchGlobalKey(KeyEvent{.key = "x"}, false));  // not consumed
    // While typing in a field, "/" is text, not a shortcut; Ctrl+Z still is.
    CHECK(!detail::dispatchGlobalKey(KeyEvent{.key = "/"}, true));
    CHECK(detail::dispatchGlobalKey(KeyEvent{.key = "z", .ctrl = true}, true));
    CHECK(detail::dispatchGlobalKey(KeyEvent{.key = "/"}, false));
    root.render(h(Shortcuts, {.name = "b"}));  // the newest handler is used
    detail::dispatchGlobalKey(KeyEvent{.key = "z", .ctrl = true}, false);
    root.render(h("box"));  // unmounted: unsubscribed
    CHECK(!detail::dispatchGlobalKey(KeyEvent{.key = "z", .ctrl = true}, false));
    CHECK(shortcuts == (std::vector<std::string>{"a:undo", "a:undo", "a:search", "b:undo"}));
}

ScrollEvent lastScroll;
Element ScrollBox(const Props&) {
    auto [more, setMore] = useState(false);
    return h("box", {{"id", "scroller"}, {"onScroll", [setMore](const ScrollEvent& e) {
                         lastScroll = e;
                         if (e.nearBottom()) setMore(true);
                     }}},
             {more ? "loading more" : "list"});
}

TEST(on_scroll_gets_the_position) {
    TextRenderer r;
    Root root(r);
    root.render(h(ScrollBox));
    CHECK(root.dispatch(byId(r, "scroller"), "onScroll",
                        ScrollEvent{.top = 100, .scrollHeight = 2000, .clientHeight = 500}));
    CHECK(lastScroll.top == 100 && !has(r, "loading more"));
    root.dispatch(byId(r, "scroller"), "onScroll", ScrollEvent{.top = 1450, .scrollHeight = 2000, .clientHeight = 500});
    CHECK(has(r, "loading more"));                              // within 100 px of the end
    CHECK(!root.dispatch(byId(r, "scroller"), "onScroll"));   // no position: not called
}

}  // namespace

int main() {
    for (auto& [name, fn] : tests) {
        std::cout << name << '\n';
        try {
            fn();
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "  EXCEPTION: " << e.what() << '\n';
        }
    }
    std::cout << (failures ? "FAILED: " : "OK, all tests passed") << (failures ? std::to_string(failures) : "")
              << '\n';
    return failures ? 1 : 0;
}
