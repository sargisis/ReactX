// Stress tests of reactx::jobs: many jobs on many threads, random failures
// and retries, cancel racing the workers, stopping a busy queue, many
// repeating jobs, and cron checked against a minute-by-minute search.
// Runs under ThreadSanitizer in CI. `ctest -L stress`.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "reactx/testing.hpp"
#include "rx/jobs.hpp"

using namespace rx;
using namespace std::chrono_literals;
using Clock = std::chrono::system_clock;

namespace {
std::mt19937 rng(7);
}  // namespace

RX_TEST(twenty_thousand_jobs_run_exactly_once) {
    constexpr int n = 20000;
    auto runs = std::make_unique<std::atomic<int>[]>(n);
    {
        jobs::Queue q({.workers = 8});
        // Added from several threads at once, while the workers already run.
        std::vector<std::jthread> adders;
        for (int t = 0; t < 4; ++t)
            adders.emplace_back([&, t] {
                for (int i = t; i < n; i += 4) q.add("job", [&runs, i] { ++runs[i]; });
            });
        adders.clear();
        q.wait();
        auto s = q.stats();
        CHECK(s.done == n);
        CHECK(s.running == 0 && s.waiting == 0 && s.failed == 0);
    }
    int wrong = 0;
    for (int i = 0; i < n; ++i) wrong += runs[i] != 1;
    CHECK(wrong == 0);
}

RX_TEST(random_failures_are_retried_and_reported_once) {
    constexpr int n = 2000;
    auto tries = std::make_unique<std::atomic<int>[]>(n);
    auto succeeded = std::make_unique<std::atomic<int>[]>(n);
    auto reported = std::make_unique<std::atomic<int>[]>(n);
    std::mutex m;
    std::map<std::uint64_t, int> idToJob;
    std::atomic<int> oddReports{0};  // checked on the test's thread: onFailed runs on a worker
    {
        jobs::Queue q({.workers = 6, .onFailed = [&](const jobs::Failure& f) {
                           std::lock_guard lock(m);
                           ++reported[idToJob.at(f.id)];
                           if (f.attempts != 3 || f.error != "flaky") ++oddReports;
                       }});
        for (int i = 0; i < n; ++i) {
            const unsigned seed = static_cast<unsigned>(i) * 2654435761u;
            std::lock_guard lock(m);  // so onFailed finds the id
            auto id = q.add(
                "flaky",
                [&, i, seed] {
                    const int attempt = ++tries[i];
                    if (((seed >> attempt) & 1u) != 0) throw std::runtime_error("flaky");  // fails about half the time
                    ++succeeded[i];
                },
                {.attempts = 3, .delay = 1ms, .backoff = 1.5});
            idToJob[id] = i;
        }
        q.wait();
    }
    int bad = 0, gaveUp = 0;
    for (int i = 0; i < n; ++i) {
        // Either it succeeded once (and was never reported), or it used all 3 tries and was reported once.
        const bool ok = succeeded[i] == 1 && reported[i] == 0 && tries[i] <= 3;
        const bool failed = succeeded[i] == 0 && reported[i] == 1 && tries[i] == 3;
        bad += !(ok || failed);
        gaveUp += failed;
    }
    std::printf("  %d of %d jobs gave up after 3 tries\n", gaveUp, n);
    CHECK(oddReports == 0);
    CHECK(bad == 0);
    CHECK(gaveUp > 0 && gaveUp < n);
}

RX_TEST(cancel_racing_the_workers) {
    constexpr int n = 5000;
    auto runs = std::make_unique<std::atomic<int>[]>(n);
    auto cancelled = std::make_unique<std::atomic<bool>[]>(n);
    std::vector<std::uint64_t> ids(n);
    {
        jobs::Queue q({.workers = 4});
        std::uniform_int_distribution<int> delay(0, 20);
        for (int i = 0; i < n; ++i)
            ids[i] = q.after(std::chrono::milliseconds(delay(rng)), "job", [&runs, i] {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                ++runs[i];
            });
        std::jthread canceller([&] {
            for (int i = 0; i < n; i += 2) cancelled[i] = q.cancel(ids[i]);
        });
        canceller.join();
        q.wait();  // must not return before every job that wasn't cancelled has run
        int missing = 0, twice = 0;
        for (int i = 0; i < n; ++i) {
            if (!cancelled[i] && runs[i] != 1) ++missing;
            if (runs[i] > 1) ++twice;
        }
        CHECK(missing == 0);
        CHECK(twice == 0);
        CHECK(!q.cancel(ids[1]));  // done jobs can't be cancelled
        std::this_thread::sleep_for(40ms);
        int late = 0;  // a job cancelled before it started never runs later
        for (int i = 0; i < n; i += 2) late += cancelled[i] && runs[i] > 1;
        CHECK(late == 0);
    }
}

RX_TEST(stopping_a_busy_queue_is_quick) {
    std::atomic<int> ran{0};
    auto t0 = std::chrono::steady_clock::now();
    {
        jobs::Queue q({.workers = 4});
        for (int i = 0; i < 2000; ++i) q.add("slow", [&] {
            std::this_thread::sleep_for(2ms);
            ++ran;
        });
        std::this_thread::sleep_for(50ms);
    }  // the destructor: running jobs finish, the rest is dropped
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  stopped after %.0f ms with %d of 2000 jobs run\n", ms, ran.load());
    CHECK(ran > 0 && ran < 2000);
    CHECK(ms < 2000);
}

RX_TEST(many_repeating_jobs_stop_when_cancelled) {
    std::atomic<int> ticks{0};
    jobs::Queue q({.workers = 4});
    std::vector<std::uint64_t> ids;
    for (int i = 0; i < 100; ++i) ids.push_back(q.every(5ms, "tick", [&] { ++ticks; }));
    std::this_thread::sleep_for(300ms);
    for (auto id : ids) CHECK(q.cancel(id));
    std::this_thread::sleep_for(30ms);  // a run in progress may still finish
    const int after = ticks;
    std::this_thread::sleep_for(100ms);
    std::printf("  100 repeating jobs ran %d times in 300 ms\n", after);
    CHECK(after > 100);
    CHECK(ticks == after);
    CHECK(q.stats().waiting == 0);
}

RX_TEST(cron_matches_a_minute_by_minute_search) {
    // Specs whose next time is never more than a few days away, so a plain search can check them.
    const char* specs[] = {"*/7 * * * *", "0 * * * *", "15,45 9-17 * * *", "5/20 3 * * *", "0 0 * * 1-5",
                           "30 12 * * 0,6", "59 23 * * *", "0 6,18 * * *", "*/30 */4 * * *", "0 0 * * 3"};
    auto matches = [](const std::string& spec, std::time_t t) {
        // Is t (a minute) the next time from t - 1 minute? Then nextCron agrees it matches.
        auto r = jobs::nextCron(spec, Clock::from_time_t(t) - 1min);
        return r && Clock::to_time_t(*r) == t;
    };
    std::uniform_int_distribution<std::time_t> start(1'700'000'000, 1'900'000'000);
    int checked = 0;
    for (const char* spec : specs)
        for (int k = 0; k < 6; ++k) {
            const auto from = Clock::from_time_t(start(rng));
            auto next = jobs::nextCron(spec, from);
            CHECK(next.has_value());
            if (!next) continue;
            CHECK(*next > from);
            const std::time_t got = Clock::to_time_t(*next);
            std::tm tm{};
            localtime_r(&got, &tm);
            CHECK(tm.tm_sec == 0);
            // No earlier minute after `from` matches: step through them.
            std::time_t t = (Clock::to_time_t(from) / 60 + 1) * 60;
            int earlier = 0;
            for (; t < got; t += 60) earlier += matches(spec, t);
            CHECK(earlier == 0);
            ++checked;
        }
    std::printf("  %d cron cases checked minute by minute\n", checked);
    // Bad specs are errors, not crashes.
    for (const char* bad : {"", "* * * *", "60 * * * *", "* 24 * * *", "* * 0 * *", "* * * 13 *", "* * * * 8",
                            "*/0 * * * *", "a b c d e", "5-1 * * * *", "* * 31 2 *"})
        CHECK(!jobs::nextCron(bad, Clock::now()).has_value());
}
