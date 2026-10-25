// Minimal tests without a framework: each TEST is a function, CHECK records failures.
#include <functional>
#include <iostream>
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

// ---- tests ----

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
