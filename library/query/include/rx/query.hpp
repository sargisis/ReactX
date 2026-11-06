// reactx::query — data from a server with a shared cache, like React Query /
// SWR: one request per key however many components ask, cached data shown at
// once and refreshed in the background, retries, invalidation after a change,
// optimistic updates. Browser and terminal (both have rx::fetch).
//
//   auto todos = rx::query::useQuery({.key = "todos", .url = "/api/todos"});   // json::Value
//   if (todos.loading) return text("Loading...");
//   if (!todos.error.empty()) return text(todos.error);
//   for (auto& t : todos.data->asArray()) ...
//
//   auto add = rx::query::useMutation<std::string>({
//       .request = [](const std::string& title) {
//           return FetchRequest{.method = "POST", .url = "/api/todos", .body = json::object({{"title", title}}).dump()};
//       },
//       .invalidates = {"todos"}});
//   add.mutate("Buy milk");   // then every "todos" query loads again
//
// Comes with ReactX (library/query); link it in CMakeLists.txt:
//   target_link_libraries(my_app PRIVATE reactx::query)
#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "reactx/fetch.hpp"
#include "reactx/json.hpp"
#include "reactx/reactx.hpp"

namespace rx::query {

// What a loader hands back: the value or why it failed.
template <class T>
using Result = std::expected<T, std::string>;
template <class T>
using Resolve = std::function<void(Result<T>)>;
// Your own way to load (a computation, a WebSocket, several requests): call
// `resolve` once, now or later; return a function that cancels it (or {}).
template <class T>
using Loader = std::function<Cleanup(Resolve<T>)>;

namespace impl {

// The cache entry of one key, without its type.
struct Entry;
using AnyData = std::shared_ptr<const void>;
using AnyLoader = std::function<Cleanup(std::function<void(AnyData, std::string)>)>;

struct Settings {
    std::chrono::milliseconds cacheTime{};
    int retry = 0;
};

std::shared_ptr<Entry> entry(std::string_view key);
const Store<std::uint64_t>& versionOf(const Entry& e);
void setLoader(Entry& e, AnyLoader loader, const void* type, Settings settings);
// An observer (a mounted useQuery) came or went; the first one may start a load.
void observe(const std::shared_ptr<Entry>& e, bool enabled, std::chrono::milliseconds staleTime);
void unobserve(const std::shared_ptr<Entry>& e);
void refetch(const std::shared_ptr<Entry>& e);
// A repeating timer on the UI thread.
Cleanup every(std::chrono::milliseconds period, std::function<void()> fn);

struct Snapshot {
    AnyData data;
    const void* type = nullptr;
    std::string error;
    bool hasData = false;
    bool fetching = false;
    std::chrono::steady_clock::time_point updatedAt{};
};
Snapshot snapshot(const Entry& e);
void setAny(std::string_view key, AnyData data, const void* type);
Snapshot get(std::string_view key);

// One address per type: data saved as T is read back only as T.
template <class T>
const void* typeTag() {
    static const char tag = 0;
    return &tag;
}

// The response as a T: json::Value and std::string need no parse function.
template <class T>
Result<T> decode(const FetchResponse& r, const std::function<T(const json::Value&)>& parse) {
    if (!r.ok()) return std::unexpected(r.errorText());
    if constexpr (std::same_as<T, std::string>) {
        if (!parse) return r.body;
    }
    if constexpr (std::same_as<T, json::Value>) {
        if (!parse) {
            if (r.body.empty()) return json::Value{};
            auto v = r.tryJson();
            if (!v) return std::unexpected("invalid JSON: " + std::string(v.error().what()));
            return std::move(*v);
        }
    }
    if (!parse) return std::unexpected(std::string("no .parse for this type of data"));
    auto v = r.tryJson();
    if (!v) return std::unexpected("invalid JSON: " + std::string(v.error().what()));
    try {
        return parse(*v);
    } catch (const std::exception& e) {  // json::Error from as*() on a wrong field
        return std::unexpected(std::string(e.what()));
    }
}

template <class T>
Loader<T> httpLoader(FetchRequest request, std::function<T(const json::Value&)> parse) {
    return [request = std::move(request), parse = std::move(parse)](Resolve<T> resolve) -> Cleanup {
        return fetch(request, [parse, resolve = std::move(resolve)](const FetchResponse& r) { resolve(decode<T>(r, parse)); });
    };
}

}  // namespace impl

template <class T = json::Value>
struct QueryOptions {
    std::string key{};          // the cache key: "todos", "todos/5" (invalidate("todos") reaches both)
    std::string url{};          // GET this URL ...
    FetchRequest request{};     // ... or this request (headers, a POST to search) when .url is ""
    Loader<T> load{};           // ... or load it your way (wins over both)
    std::function<T(const json::Value&)> parse{};  // the JSON as a T; json::Value and std::string need none
    std::chrono::milliseconds staleTime{0};        // fresh this long: a new component shows it without loading
                                                   // again (at least 2 s: components mounting together share a load)
    std::chrono::milliseconds cacheTime{std::chrono::minutes(5)};     // kept this long after the last component using it is gone
    std::chrono::milliseconds refetchInterval{0};  // load again every ...; 0 = no polling
    int retry = 2;              // tries again after an error, waiting 0.5 s, 1 s, 2 s, ...
    bool enabled = true;        // false: don't load (yet), e.g. until a search has text
};

template <class T>
struct Query {
    std::shared_ptr<const T> data{};  // nullptr until loaded; the last good value while refreshing or after an error
    std::string error{};              // "" or why the last load failed (after the retries)
    bool loading = false;             // the first load: no data yet
    bool fetching = false;            // any load in flight, background refreshes too
    std::function<void()> refetch{};  // load again now
    bool ok() const { return data && error.empty(); }
};

// The data for options.key; the component re-renders when it changes.
template <class T = json::Value>
Query<T> useQuery(QueryOptions<T> options) {
    std::shared_ptr<impl::Entry> e = impl::entry(options.key);
    (void)useStore(impl::versionOf(*e));

    Loader<T> load = std::move(options.load);
    if (!load) {
        FetchRequest request = options.url.empty() ? std::move(options.request) : FetchRequest{.url = options.url};
        load = impl::httpLoader<T>(std::move(request), std::move(options.parse));
    }
    impl::setLoader(*e,
                    [load](std::function<void(impl::AnyData, std::string)> done) -> Cleanup {
                        return load([done](Result<T> r) {
                            if (r) done(std::make_shared<const T>(std::move(*r)), {});
                            else done(nullptr, r.error());
                        });
                    },
                    impl::typeTag<T>(), {.cacheTime = options.cacheTime, .retry = options.retry});

    const std::string key = options.key;
    const bool enabled = options.enabled;
    const auto stale = options.staleTime.count();
    useEffect(
        [key, enabled, stale] {
            auto entry = impl::entry(key);
            impl::observe(entry, enabled, std::chrono::milliseconds(stale));
            return [entry] { impl::unobserve(entry); };
        },
        deps(key, enabled));
    const auto period = options.refetchInterval.count();
    useEffect(
        [key, enabled, period]() -> Cleanup {
            if (!enabled || period <= 0) return {};
            return impl::every(std::chrono::milliseconds(period), [key] { impl::refetch(impl::entry(key)); });
        },
        deps(key, enabled, period));

    impl::Snapshot s = impl::snapshot(*e);
    Query<T> q;
    if (s.data && s.type == impl::typeTag<T>()) q.data = std::static_pointer_cast<const T>(s.data);
    else if (s.data) q.error = "the key \"" + key + "\" holds another type of data";
    if (q.error.empty()) q.error = s.error;
    q.loading = enabled && s.fetching && !s.hasData;
    q.fetching = s.fetching;
    q.refetch = [key] { impl::refetch(impl::entry(key)); };
    return q;
}

// ---- the cache from outside components (handlers, effects, mutations) ----

// Marks `key` and the keys under it ("todos" -> "todos", "todos/5") as old:
// the ones on screen load again, the others when they are next shown.
void invalidate(std::string_view key);
// Forgets `key` and the keys under it.
void remove(std::string_view key);
// Forgets everything (e.g. after signing out).
void clear();

// The cached value as T, or nullptr.
template <class T>
std::shared_ptr<const T> getData(std::string_view key) {
    impl::Snapshot s = impl::get(key);
    if (!s.data || s.type != impl::typeTag<T>()) return nullptr;
    return std::static_pointer_cast<const T>(s.data);
}
// Puts a value into the cache (it counts as just loaded); components showing
// `key` re-render. For optimistic updates and for data you already have.
template <class T>
void setData(std::string_view key, T value) {
    impl::setAny(key, std::make_shared<const T>(std::move(value)), impl::typeTag<T>());
}
// setData(key, f(current)); does nothing when nothing of type T is cached.
template <class T, class F>
    requires std::is_invocable_r_v<T, F, const T&>
void updateData(std::string_view key, F&& f) {
    if (auto current = getData<T>(key)) setData<T>(key, std::invoke(std::forward<F>(f), *current));
}

// ---- changes: POST / PUT / DELETE ----

template <class Vars, class R = json::Value>
struct MutationOptions {
    std::function<FetchRequest(const Vars&)> request{};          // the HTTP request for these values ...
    std::function<Cleanup(const Vars&, Resolve<R>)> run{};       // ... or do it your way (wins)
    std::function<R(const json::Value&)> parse{};                // the answer as R; json::Value and std::string need none
    // Before the request: change the cache right away (setData / updateData)
    // and return how to undo it; it is undone if the request fails.
    std::function<Cleanup(const Vars&)> onMutate{};
    std::function<void(const R&, const Vars&)> onSuccess{};
    std::function<void(const std::string&, const Vars&)> onError{};
    std::vector<std::string> invalidates{};  // keys to load again after success (and after a failure: the cache may be off)
};

template <class Vars, class R>
struct Mutation {
    std::function<void(Vars)> mutate{};
    bool pending = false;
    std::string error{};
    std::shared_ptr<const R> data{};  // the last answer
    std::function<void()> reset{};    // clears error and data
};

template <class Vars, class R = json::Value>
Mutation<Vars, R> useMutation(MutationOptions<Vars, R> options) {
    struct State {
        bool pending = false;
        std::string error;
        std::shared_ptr<const R> data;
        std::uint64_t serial = 0;  // only the latest mutate() writes its result
    };
    auto [state, setState] = useState(std::make_shared<const State>());
    auto& calls = useRef(std::make_shared<std::uint64_t>(0));  // mutate() calls, across renders
    auto opts = std::make_shared<const MutationOptions<Vars, R>>(std::move(options));

    Mutation<Vars, R> m;
    m.pending = state->pending;
    m.error = state->error;
    m.data = state->data;
    m.reset = [setState] { setState(std::make_shared<const State>()); };
    m.mutate = [opts, setState, calls = calls](Vars vars) {
        const std::uint64_t mine = ++*calls;
        setState(std::make_shared<const State>(State{.pending = true, .error = {}, .data = {}, .serial = mine}));
        Cleanup undo = opts->onMutate ? opts->onMutate(vars) : Cleanup{};
        auto finish = [opts, setState, mine, vars, undo](Result<R> r) {
            if (r) {
                auto data = std::make_shared<const R>(std::move(*r));
                if (opts->onSuccess) opts->onSuccess(*data, vars);
                setState([mine, data](const std::shared_ptr<const State>& s) {
                    if (s->serial != mine) return s;
                    return std::make_shared<const State>(State{.pending = false, .error = {}, .data = data, .serial = mine});
                });
            } else {
                if (undo) undo();
                if (opts->onError) opts->onError(r.error(), vars);
                std::string error = r.error();
                setState([mine, error](const std::shared_ptr<const State>& s) {
                    if (s->serial != mine) return s;
                    return std::make_shared<const State>(State{.pending = false, .error = error, .data = {}, .serial = mine});
                });
            }
            for (const auto& key : opts->invalidates) invalidate(key);
        };
        if (opts->run) {
            (void)opts->run(vars, finish);
        } else if (opts->request) {
            fetch(opts->request(vars), [opts, finish](const FetchResponse& r) { finish(impl::decode<R>(r, opts->parse)); });
        } else {
            finish(std::unexpected(std::string("useMutation: no .request or .run")));
        }
    };
    return m;
}

}  // namespace rx::query
