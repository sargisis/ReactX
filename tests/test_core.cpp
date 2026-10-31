// Minimal tests without a framework: each TEST is a function, CHECK records failures.
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "reactx/dsl.hpp"
#include "reactx/reactx.hpp"
#include "reactx/text_renderer.hpp"
#include "reactx/contract.hpp"
#include "reactx/router.hpp"
#include "reactx/version.hpp"

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

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

// Counts how many nodes the core asked to create.
struct CountingRenderer : TextRenderer {
    int created = 0;
    NodeHandle createElement(std::string_view t) override { ++created; return TextRenderer::createElement(t); }
    NodeHandle createText(std::string_view t) override { ++created; return TextRenderer::createText(t); }
};

NodeHandle byId(const TextRenderer& r, const std::string& id) {
    auto h = r.findByProp("id", id);
    if (!h) throw std::runtime_error("no node with id " + id);
    return *h;
}

// ---- components used by tests ----

Element Counter(const Props&) {
    auto [count, setCount] = useState(0);
    return h("box", {}, {
        h("text", {{"id", "label"}}, {"count=" + std::to_string(count)}),
        h("button", {{"id", "inc"}, {"onClick", [setCount] { setCount([](int c) { return c + 1; }); }}}),
    });
}

Element List(const Props& p) {
    std::vector<Element> items;
    for (char c : p.get<std::string>("order")) {
        std::string s(1, c);
        items.push_back(h("item", {{"key", s}, {"id", s}}, {s}));
    }
    return h("list", {}, std::move(items));
}

int effectRuns = 0;
int cleanupRuns = 0;

Element WithEffect(const Props& p) {
    int dep = p.get<int>("dep");
    useEffect([] {
        ++effectRuns;
        return [] { ++cleanupRuns; };
    }, deps(dep));
    return h("box");
}

Element Toggle(const Props& p) {
    if (p.get<bool>("show")) return h(WithEffect, {{"dep", 1}});
    return fragment({});
}

// Text field: onInput stores the text, Enter submits it, Escape clears it.
Element Field(const Props&) {
    auto [draft, setDraft] = useState(std::string{});
    auto [submitted, setSubmitted] = useState(std::string{});
    return h("box", {}, {
        h("input", {{"id", "field"},
                    {"value", draft},
                    {"onInput", [setDraft](const std::string& t) { setDraft(t); }},
                    {"onKey", [setDraft, setSubmitted, draft](const KeyEvent& e) {
                         if (e.key == "Enter") {
                             setSubmitted(draft);
                             setDraft(std::string{});
                         } else if (e.key == "Escape") {
                             setDraft(std::string{});
                         }
                     }}}),
        h("text", {{"id", "out"}}, {"submitted=" + submitted}),
    });
}

// ---- step 3: memo, useMemo, useCallback, context ----

int memoComputes = 0;
int itemRenders = 0;
int consumerRenders = 0;

Element Item(const Props& p) {
    ++itemRenders;
    return h("item", {}, {p.get<std::string>("label")});
}

// Parent with a counter; renders a memo<Item>. `stable` picks useCallback vs
// a fresh lambda for the item's handler.
Element MemoParent(const Props& p) {
    auto [count, setCount] = useState(0);
    auto [label, setLabel] = useState(std::string("a"));
    const auto& doubled = useMemo([&] { ++memoComputes; return label + label; }, deps(label));
    Callback stableClick = useCallback([setCount] { setCount([](int c) { return c + 1; }); }, deps());
    Callback freshClick = [setCount] { setCount([](int c) { return c + 1; }); };
    return h("box", {}, {
        h("button", {{"id", "inc"}, {"onClick", stableClick}}),
        h("button", {{"id", "relabel"}, {"onClick", [setLabel] { setLabel(std::string("b")); }}}),
        h("text", {{"id", "info"}}, {"count=" + std::to_string(count) + " doubled=" + doubled}),
        h(memo<Item>, {{"label", label}, {"onClick", p.get<bool>("stable") ? stableClick : freshClick}}),
    });
}

const auto Theme = createContext<std::string>("light");

Element ThemeLabel(const Props&) {
    ++consumerRenders;
    return h("text", {{"id", "theme"}}, {"theme=" + useContext(Theme)});
}

// memo barrier with no props: never re-rendered by its parent.
Element Barrier(const Props&) { return h("box", {}, {h(ThemeLabel)}); }

Element ThemeApp(const Props&) {
    auto [theme, setTheme] = useState(std::string("dark"));
    auto [ticks, setTicks] = useState(0);
    return h("box", {}, {
        h("button", {{"id", "toggle"}, {"onClick", [setTheme, theme] {
             setTheme(std::string(theme == "dark" ? "light" : "dark"));
         }}}),
        h("button", {{"id", "tick"}, {"onClick", [setTicks] { setTicks([](int t) { return t + 1; }); }}}),
        h("text", {}, {"ticks=" + std::to_string(ticks)}),
        Theme.provide(theme, {h(memo<Barrier>)}),
    });
}

// ---- tests ----

// Runs `fn` in a child process; true if it aborted and printed `message`.
bool diesWith(const std::function<void()>& fn, const std::string& message) {
    int fds[2];
    if (::pipe(fds) != 0) return false;
    pid_t pid = ::fork();
    if (pid == 0) {
        ::dup2(fds[1], 2);
        ::close(fds[0]);
        fn();
        ::_exit(0);  // did not die
    }
    ::close(fds[1]);
    std::string err;
    char buf[512];
    for (ssize_t n; (n = ::read(fds[0], buf, sizeof buf)) > 0;) err.append(buf, static_cast<std::size_t>(n));
    ::close(fds[0]);
    int status = 0;
    ::waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT && err.find(message) != std::string::npos;
}

// C++26 contracts (GCC 16 -fcontracts) report "contract violation ...: <condition>";
// without them REACTX_PRE prints "precondition failed: <condition>".
#if defined(__cpp_contracts)
constexpr std::string_view kViolation = ": ";
#else
constexpr std::string_view kViolation = "precondition failed: ";
#endif

TEST(precondition_violation_stops_with_a_message) {
    CHECK(diesWith([] { REACTX_PRE(1 + 1 == 3); }, std::string(kViolation) + "1 + 1 == 3"));
    CHECK(diesWith([] { router::navigate("todos/7"); }, std::string(kViolation) + "path.starts_with('/')"));
    CHECK(!diesWith([] { REACTX_PRE(2 > 1); }, ""));  // a true condition is free
}

Setter<int> leakedSetter;  // default-constructed: does nothing until a render fills it
Element LeaksSetter(const Props&) {
    auto [n, setN] = useState(0);
    leakedSetter = setN;
    return text("n=" + std::to_string(n));
}

TEST(setter_from_another_thread_stops) {
    leakedSetter(5);  // empty: ignored
    CHECK(diesWith(
        [] {
            TextRenderer r;
            Root root(r);
            root.render(h(LeaksSetter));
            std::thread([] { leakedSetter(1); }).join();
        },
        "onOwnThread"));
    TextRenderer r;  // on the UI thread it works
    Root root(r);
    root.render(h(LeaksSetter));
    leakedSetter(2);
    root.flush();
    CHECK(r.toString().find("n=2") != std::string::npos);
}

TEST(default_dispatch_does_nothing) {
    Dispatch<int, int> dispatch;
    dispatch(1);
    ScrollEvent e{.top = 7, .left = 3};
    CHECK(e.scrollTop() == 7 && e.scrollLeft() == 3);
    std::vector<bool> seen;
    InputHandler onChange = boolHandler([&seen](bool on) { seen.push_back(on); });
    onChange("true");
    onChange("false");
    CHECK((seen == std::vector<bool>{true, false}));
}

TEST(version_is_semver) {
    std::string v = rx::version();
    CHECK(v == std::to_string(REACTX_VERSION_MAJOR) + "." + std::to_string(REACTX_VERSION_MINOR) + "." +
                   std::to_string(REACTX_VERSION_PATCH));
    CHECK(std::count(v.begin(), v.end(), '.') == 2);  // three numbers, never four
}

TEST(mount_renders_tree) {
    TextRenderer r;
    Root root(r);
    root.render(h("box", {{"title", "hi"}}, {"hello", h("leaf")}));
    CHECK(r.toString() == "<root>\n  <box title=\"hi\">\n    \"hello\"\n    <leaf/>\n  </box>\n</root>\n");
}

TEST(set_state_updates_only_what_changed) {
    CountingRenderer r;
    Root root(r);
    root.render(h(Counter));
    int createdAfterMount = r.created;
    CHECK(r.toString().find("count=0") != std::string::npos);

    CHECK(root.dispatch(byId(r, "inc"), "onClick"));
    CHECK(root.dispatch(byId(r, "inc"), "onClick"));
    CHECK(r.toString().find("count=2") != std::string::npos);
    CHECK(r.created == createdAfterMount);  // text was updated in place
}

TEST(same_state_skips_rerender) {
    TextRenderer r;
    Root root(r);
    root.render(h(Counter));
    auto before = r.operationCount();
    root.flush();
    CHECK(r.operationCount() == before);
}

TEST(keyed_children_are_moved_not_recreated) {
    CountingRenderer r;
    Root root(r);
    root.render(h(List, {{"order", "abc"}}));
    NodeHandle a = byId(r, "a");
    int created = r.created;

    root.render(h(List, {{"order", "cba"}}));
    CHECK(byId(r, "a") == a);
    CHECK(r.created == created);
    auto s = r.toString();
    CHECK(s.find("id=\"c\"") < s.find("id=\"b\""));
    CHECK(s.find("id=\"b\"") < s.find("id=\"a\""));

    root.render(h(List, {{"order", "ca"}}));
    CHECK(!r.findByProp("id", "b"));
    CHECK(r.nodeCount() == 1 + 1 + 2 * 2);  // root, list, 2 items with text
}

TEST(host_type_change_replaces_node) {
    TextRenderer r;
    Root root(r);
    root.render(h("a", {{"id", "x"}}));
    NodeHandle first = byId(r, "x");
    root.render(h("b", {{"id", "x"}}));
    CHECK(byId(r, "x") != first);
    CHECK(r.toString() == "<root>\n  <b id=\"x\"/>\n</root>\n");
}

TEST(removed_props_are_unset) {
    TextRenderer r;
    Root root(r);
    root.render(h("box", {{"a", 1}, {"b", true}}));
    root.render(h("box", {{"a", 2}}));
    CHECK(r.toString() == "<root>\n  <box a=\"2\"/>\n</root>\n");
}

TEST(effect_runs_on_dep_change_and_cleans_up_on_unmount) {
    effectRuns = cleanupRuns = 0;
    TextRenderer r;
    Root root(r);
    root.render(h(WithEffect, {{"dep", 1}}));
    CHECK(effectRuns == 1);
    root.render(h(WithEffect, {{"dep", 1}}));
    CHECK(effectRuns == 1);  // same deps
    root.render(h(WithEffect, {{"dep", 2}}));
    CHECK(effectRuns == 2);
    CHECK(cleanupRuns == 1);

    root.render(h(Toggle, {{"show", false}}));  // different component: unmount
    CHECK(cleanupRuns == 2);
}

TEST(fragments_and_nested_components_place_nodes_in_order) {
    TextRenderer r;
    Root root(r);
    root.render(h("box", {}, {"start", h(Toggle, {{"show", true}}), "end"}));
    CHECK(r.toString() == "<root>\n  <box>\n    \"start\"\n    <box/>\n    \"end\"\n  </box>\n</root>\n");
    root.render(h("box", {}, {"start", h(Toggle, {{"show", false}}), "end"}));
    CHECK(r.toString() == "<root>\n  <box>\n    \"start\"\n    \"end\"\n  </box>\n</root>\n");
}

TEST(hooks_outside_component_throw) {
    bool threw = false;
    try {
        useState(1);
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(root_destructor_runs_cleanups) {
    effectRuns = cleanupRuns = 0;
    TextRenderer r;
    {
        Root root(r);
        root.render(h(WithEffect, {{"dep", 1}}));
    }
    CHECK(cleanupRuns == 1);
    CHECK(r.nodeCount() == 1);
}

TEST(input_handler_receives_text) {
    TextRenderer r;
    Root root(r);
    root.render(h(Field));
    CHECK(root.dispatch(byId(r, "field"), "onInput", std::string("hello")));
    CHECK(r.toString().find("value=\"hello\"") != std::string::npos);
    CHECK(root.dispatch(byId(r, "field"), "onInput", "bye"));  // const char* works too
    CHECK(r.toString().find("value=\"bye\"") != std::string::npos);
}

TEST(key_handler_receives_key_event) {
    TextRenderer r;
    Root root(r);
    root.render(h(Field));
    root.dispatch(byId(r, "field"), "onInput", std::string("milk"));
    CHECK(root.dispatch(byId(r, "field"), "onKey", KeyEvent{"Enter"}));
    CHECK(r.toString().find("submitted=milk") != std::string::npos);
    CHECK(r.toString().find("value=\"\"") != std::string::npos);

    root.dispatch(byId(r, "field"), "onInput", std::string("x"));
    CHECK(root.dispatch(byId(r, "field"), "onKey", KeyEvent{.key = "Escape", .shift = true}));
    CHECK(r.toString().find("value=\"\"") != std::string::npos);
    CHECK(r.toString().find("submitted=milk") != std::string::npos);
}

TEST(mismatched_event_data_is_rejected) {
    TextRenderer r;
    Root root(r);
    root.render(h(Field));
    auto before = r.operationCount();
    CHECK(!root.dispatch(byId(r, "field"), "onKey"));                          // no data
    CHECK(!root.dispatch(byId(r, "field"), "onKey", std::string("Enter")));    // text, not a key
    CHECK(!root.dispatch(byId(r, "field"), "onInput", KeyEvent{"a"}));         // key, not text
    CHECK(!root.dispatch(byId(r, "field"), "onMissing", std::string("x")));
    CHECK(r.operationCount() == before);
}

TEST(plain_callback_ignores_event_data) {
    TextRenderer r;
    Root root(r);
    root.render(h(Counter));
    CHECK(root.dispatch(byId(r, "inc"), "onClick", KeyEvent{"Enter"}));
    CHECK(r.toString().find("count=1") != std::string::npos);
}

TEST(handlers_are_not_sent_to_renderer) {
    TextRenderer r;
    Root root(r);
    root.render(h(Field));
    auto tree = r.toString();
    CHECK(tree.find("onKey") == std::string::npos);
    CHECK(tree.find("onInput") == std::string::npos);
}

TEST(use_memo_recomputes_only_when_deps_change) {
    TextRenderer r;
    Root root(r);
    memoComputes = 0;
    root.render(h(MemoParent, {{"stable", true}}));
    CHECK(memoComputes == 1);
    root.dispatch(byId(r, "inc"), "onClick");
    CHECK(memoComputes == 1);  // count changed, label did not
    CHECK(r.toString().find("count=1 doubled=aa") != std::string::npos);
    root.dispatch(byId(r, "relabel"), "onClick");
    CHECK(memoComputes == 2);
    CHECK(r.toString().find("doubled=bb") != std::string::npos);
}

TEST(use_callback_keeps_identity_until_deps_change) {
    TextRenderer r;
    Root root(r);
    std::vector<PropValue> seen;
    struct Capture {
        static Element render(const Props& p) {
            auto [n, setN] = useState(0);
            auto [dep, setDep] = useState(0);
            Callback cb = useCallback([] {}, deps(dep));
            auto* out = reinterpret_cast<std::vector<PropValue>*>(p.get<std::int64_t>("out"));
            out->push_back(cb);
            return h("box", {}, {
                h("button", {{"id", "n"}, {"onClick", [setN] { setN([](int v) { return v + 1; }); }}}),
                h("button", {{"id", "dep"}, {"onClick", [setDep] { setDep([](int v) { return v + 1; }); }}}),
            });
        }
    };
    root.render(h(&Capture::render, {{"out", reinterpret_cast<std::int64_t>(&seen)}}));
    root.dispatch(byId(r, "n"), "onClick");
    root.dispatch(byId(r, "dep"), "onClick");
    CHECK(seen.size() == 3);
    CHECK(propEquals(seen[0], seen[1]));   // same deps: same handler
    CHECK(!propEquals(seen[1], seen[2]));  // deps changed: new handler
    CHECK(!propEquals(PropValue(Callback([] {})), PropValue(Callback([] {}))));
}

TEST(memo_skips_rerender_with_equal_props) {
    TextRenderer r;
    Root root(r);
    itemRenders = 0;
    root.render(h(MemoParent, {{"stable", true}}));
    CHECK(itemRenders == 1);
    root.dispatch(byId(r, "inc"), "onClick");
    root.dispatch(byId(r, "inc"), "onClick");
    CHECK(itemRenders == 1);  // parent re-rendered twice, item skipped
    root.dispatch(byId(r, "relabel"), "onClick");
    CHECK(itemRenders == 2);  // label changed
    CHECK(r.toString().find("\"b\"") != std::string::npos);
}

TEST(memo_rerenders_when_handler_is_a_fresh_lambda) {
    TextRenderer r;
    Root root(r);
    itemRenders = 0;
    root.render(h(MemoParent, {{"stable", false}}));
    root.dispatch(byId(r, "inc"), "onClick");
    CHECK(itemRenders == 2);
}

TEST(memo_component_still_updates_on_own_state) {
    struct Self {
        static Element render(const Props&) {
            auto [n, setN] = useState(0);
            return h("button", {{"id", "self"}, {"onClick", [setN] { setN([](int v) { return v + 1; }); }}},
                     {"n=" + std::to_string(n)});
        }
    };
    TextRenderer r;
    Root root(r);
    root.render(h("box", {}, {h(memo<&Self::render>)}));
    root.dispatch(byId(r, "self"), "onClick");
    CHECK(r.toString().find("n=1") != std::string::npos);
}

TEST(context_default_without_provider) {
    TextRenderer r;
    Root root(r);
    root.render(h(ThemeLabel));
    CHECK(r.toString().find("theme=light") != std::string::npos);
}

TEST(nearest_provider_wins) {
    TextRenderer r;
    Root root(r);
    root.render(Theme.provide("outer", {h("box", {}, {h(ThemeLabel), Theme.provide("inner", {h(ThemeLabel)})})}));
    auto tree = r.toString();
    CHECK(tree.find("theme=outer") != std::string::npos);
    CHECK(tree.find("theme=inner") != std::string::npos);
}

TEST(context_change_passes_through_memo_barrier) {
    TextRenderer r;
    Root root(r);
    consumerRenders = 0;
    root.render(h(ThemeApp));
    CHECK(r.toString().find("theme=dark") != std::string::npos);
    CHECK(consumerRenders == 1);

    root.dispatch(byId(r, "tick"), "onClick");  // same theme: nothing below re-renders
    CHECK(consumerRenders == 1);

    root.dispatch(byId(r, "toggle"), "onClick");  // Barrier skips, ThemeLabel still updates
    CHECK(r.toString().find("theme=light") != std::string::npos);
    CHECK(consumerRenders == 2);
}

TEST(use_context_outside_component_throws) {
    bool threw = false;
    try {
        (void)useContext(Theme);
    } catch (const std::logic_error&) {
        threw = true;
    }
    CHECK(threw);
}
// TEST: DSL tag builder and literals
TEST(dsl_tag_builder_and_literals) {
    using namespace rx::dsl;
    // Short DSL syntax with chaining and variadic children
    Element el = tag::div.cls("card").id("box1")(
        tag::h1("Title"),
        tag::p.cls("lead")("Some text"),
        "custom_tag"_tag.cls("fancy")("inner text")
    );

    CHECK(el.type == "div");
    CHECK(el.props.get<std::string>("className") == "card");
    CHECK(el.props.get<std::string>("id") == "box1");
    CHECK(el.props.children().size() == 3);

    const auto& children = el.props.children();
    CHECK(children[0].type == "h1");
    CHECK(children[1].type == "p");
    CHECK(children[1].props.get<std::string>("className") == "lead");
    CHECK(children[2].type == "custom_tag");
    CHECK(children[2].props.get<std::string>("className") == "fancy");

    // Rendering via TextRenderer
    TextRenderer r;
    Root root(r);
    root.render(el);
    CHECK(r.toString().find("Title") != std::string::npos);
    CHECK(r.toString().find("Some text") != std::string::npos);
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
