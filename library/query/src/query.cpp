// reactx::query: the cache. Everything runs on the UI thread (fetch callbacks
// and timers arrive there), so there are no locks.
#include "rx/query.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <chrono>

namespace rx::query::impl {

using Clock = std::chrono::steady_clock;

struct Entry {
    std::string key;
    Store<std::uint64_t> version{0};  // bumped on every change; components read it

    AnyData data;
    const void* type = nullptr;
    bool hasData = false;
    std::string error;
    bool fetching = false;  // a request in flight or a retry waiting
    bool invalid = false;   // invalidate(): load again even if it is fresh
    Clock::time_point updatedAt{};

    AnyLoader loader;  // from the latest useQuery render
    const void* loaderType = nullptr;
    Settings settings{.cacheTime = std::chrono::minutes(5), .retry = 2};

    int observers = 0;
    int failures = 0;
    std::uint64_t generation = 0;  // the answer of an older request is dropped
    Cleanup cancel;                // the request in flight
    Cleanup retryTimer;
    Cleanup gcTimer;
};

namespace {

std::map<std::string, std::shared_ptr<Entry>, std::less<>>& cache() {
    static std::map<std::string, std::shared_ptr<Entry>, std::less<>> entries;
    return entries;
}

void bump(Entry& e) { e.version.update([](std::uint64_t v) { return v + 1; }); }

void stopTimer(Cleanup& timer) {
    if (timer) {
        Cleanup t = std::move(timer);
        timer = {};
        t();
    }
}

// "todos" covers "todos" and "todos/5", not "todosArchive".
bool covers(std::string_view prefix, std::string_view key) {
    if (!key.starts_with(prefix)) return false;
    return key.size() == prefix.size() || prefix.empty() || key[prefix.size()] == '/';
}

std::chrono::milliseconds backoff(int failures) {
    const int shift = std::min(failures - 1, 6);
    return std::min(std::chrono::milliseconds(500) * (1 << shift), std::chrono::milliseconds(30000));
}

// Starts a request now (an older one in flight is forgotten).
void start(const std::shared_ptr<Entry>& e) {
    if (!e->loader) return;
    stopTimer(e->retryTimer);
    stopTimer(e->cancel);
    e->fetching = true;
    const std::uint64_t gen = ++e->generation;
    bump(*e);
    std::weak_ptr<Entry> weak = e;
    Cleanup cancel = e->loader([weak, gen](AnyData data, std::string error) {
        auto e = weak.lock();
        if (!e || e->generation != gen) return;
        e->cancel = {};
        if (data) {
            e->data = std::move(data);
            e->type = e->loaderType;
            e->hasData = true;
            e->error.clear();
            e->invalid = false;
            e->failures = 0;
            e->updatedAt = Clock::now();
            e->fetching = false;
        } else if (++e->failures <= e->settings.retry) {
            // Still fetching: the next try comes after a pause.
            e->retryTimer = rx::detail::setTimer(backoff(e->failures), [weak] {
                if (auto e = weak.lock()) {
                    e->retryTimer = {};
                    start(e);
                }
            });
            return;
        } else {
            e->error = std::move(error);
            e->failures = 0;
            e->fetching = false;
        }
        bump(*e);
    });
    // The answer may have come already (an in-app fetch handler in a terminal build).
    if (e->generation == gen && e->fetching && !e->retryTimer) e->cancel = std::move(cancel);
}

void load(const std::shared_ptr<Entry>& e) {
    if (!e->fetching) start(e);
}

void forget(const std::shared_ptr<Entry>& e) {
    stopTimer(e->cancel);
    stopTimer(e->retryTimer);
    stopTimer(e->gcTimer);
    auto it = cache().find(e->key);
    if (it != cache().end() && it->second == e) cache().erase(it);
}

// After the last component using it is gone, the entry stays cacheTime.
void scheduleGc(const std::shared_ptr<Entry>& e) {
    stopTimer(e->gcTimer);
    if (e->observers > 0) return;
    if (e->settings.cacheTime.count() <= 0) {
        forget(e);
        return;
    }
    std::weak_ptr<Entry> weak = e;
    e->gcTimer = rx::detail::setTimer(e->settings.cacheTime, [weak] {
        if (auto e = weak.lock()) {
            e->gcTimer = {};
            if (e->observers == 0) forget(e);
        }
    });
}

std::vector<std::shared_ptr<Entry>> matching(std::string_view prefix) {
    std::vector<std::shared_ptr<Entry>> out;
    for (auto& [key, e] : cache())
        if (covers(prefix, key)) out.push_back(e);
    return out;
}

}  // namespace

std::shared_ptr<Entry> entry(std::string_view key) {
    auto& entries = cache();
    if (auto it = entries.find(key); it != entries.end()) return it->second;
    auto e = std::make_shared<Entry>();
    e->key = std::string(key);
    entries.emplace(e->key, e);
    return e;
}

const Store<std::uint64_t>& versionOf(const Entry& e) { return e.version; }

void setLoader(Entry& e, AnyLoader loader, const void* type, Settings settings) {
    e.loader = std::move(loader);
    e.loaderType = type;
    e.settings = settings;
}

void observe(const std::shared_ptr<Entry>& e, bool enabled, std::chrono::milliseconds staleTime) {
    ++e->observers;
    stopTimer(e->gcTimer);
    if (!enabled) return;
    // Components that mount within 2 s of a load share it (like SWR's
    // dedupingInterval): a list and its counter don't load the same data twice.
    const auto fresh = std::max(staleTime, std::chrono::milliseconds(2000));
    const bool stale = !e->hasData || e->invalid || Clock::now() - e->updatedAt >= fresh;
    if (stale) load(e);
}

void unobserve(const std::shared_ptr<Entry>& e) {
    if (e->observers > 0) --e->observers;
    if (e->observers == 0) scheduleGc(e);
}

void refetch(const std::shared_ptr<Entry>& e) {
    e->invalid = true;
    if (e->retryTimer) start(e);  // don't wait for the pause
    else load(e);
}

Cleanup every(std::chrono::milliseconds period, std::function<void()> fn) {
    struct Timer {
        bool alive = true;
        Cleanup cancel;
        std::function<void()> tick;
    };
    auto t = std::make_shared<Timer>();
    std::weak_ptr<Timer> weak = t;
    t->tick = [weak, period, fn = std::move(fn)] {
        auto t = weak.lock();
        if (!t || !t->alive) return;
        fn();
        if (t->alive) t->cancel = rx::detail::setTimer(period, t->tick);
    };
    t->cancel = rx::detail::setTimer(period, t->tick);
    return [t] {
        t->alive = false;
        stopTimer(t->cancel);
    };
}

Snapshot snapshot(const Entry& e) {
    return {.data = e.data,
            .type = e.type,
            .error = e.error,
            .hasData = e.hasData,
            .fetching = e.fetching,
            .updatedAt = e.updatedAt};
}

void setAny(std::string_view key, AnyData data, const void* type) {
    auto e = entry(key);
    e->data = std::move(data);
    e->type = type;
    e->hasData = true;
    e->error.clear();
    e->invalid = false;
    e->updatedAt = Clock::now();
    if (e->observers == 0 && !e->gcTimer) scheduleGc(e);
    bump(*e);
}

Snapshot get(std::string_view key) {
    auto it = cache().find(key);
    if (it == cache().end()) return {};
    return snapshot(*it->second);
}

}  // namespace rx::query::impl

namespace rx::query {

void invalidate(std::string_view key) {
    for (auto& e : impl::matching(key)) {
        e->invalid = true;
        // A request already in flight may have started before the change: start over.
        if (e->observers > 0) impl::start(e);
        if (!e->fetching) impl::bump(*e);
    }
}

void remove(std::string_view key) {
    for (auto& e : impl::matching(key)) {
        if (e->observers > 0) {  // still on screen: empty it and load it again
            e->data = nullptr;
            e->type = nullptr;
            e->hasData = false;
            e->error.clear();
            impl::start(e);
            if (!e->fetching) impl::bump(*e);
        } else {
            impl::forget(e);
        }
    }
}

void clear() { remove(""); }

}  // namespace rx::query
