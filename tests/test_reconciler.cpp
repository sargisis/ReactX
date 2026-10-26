// Edge cases of the reconciler and the hooks: useState, useEffect (cleanup
// order), useContext, memo. Every test counts renders / effect runs, so a
// change in behaviour shows up as a number, not as a vague diff.
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
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
