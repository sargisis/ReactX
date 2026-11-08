#pragma once

// reactx::jobs — background work for a server (native): send the email after
// the response went out, retry a webhook that failed, clean up every night.
// Jobs run on worker threads; a job that throws is tried again later. Link
// reactx::jobs.
//
//   rx::jobs::Queue jobs({.workers = 4,
//                         .onFailed = [](const rx::jobs::Failure& f) { log::error("job failed", {{"job", f.name}, {"error", f.error}}); }});
//   jobs.add("welcome email", [to] { sendMail(to, "Welcome!"); });           // now, 3 tries
//   jobs.add("webhook", [=] { post(url, body); }, {.attempts = 5, .delay = 2s});
//   jobs.after(10min, "reminder", [] { ... });
//   jobs.every(30s, "health check", [] { ... });
//   jobs.cron("0 3 * * *", "nightly cleanup", [] { ... });                  // 03:00 local time
//   jobs.cancel(id);
//
// A job runs on a worker thread, not the UI thread: guard shared data with a
// mutex, and don't call a component's setter from it. The Queue's destructor
// waits for the jobs that are running and drops the ones still waiting.

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace rx::jobs {

using Work = std::function<void()>;  // throw to fail (any exception; std::exception's what() is the reason)

struct Retry {
    int attempts = 3;                            // tries in all (1 = no retry)
    std::chrono::milliseconds delay{1000};       // before the second try
    double backoff = 2.0;                        // each next delay is this many times longer
    std::chrono::milliseconds maxDelay{std::chrono::minutes(10)};
};

struct Failure {
    std::uint64_t id = 0;
    std::string name{};
    std::string error{};
    int attempts = 0;
};

struct Options {
    int workers = 4;                                   // threads (0 = one per CPU)
    std::function<void(const Failure&)> onFailed{};   // a job failed its last try (called on a worker thread)
};

struct Stats {
    std::size_t waiting = 0;  // queued or scheduled
    std::size_t running = 0;
    std::uint64_t done = 0;
    std::uint64_t failed = 0;  // gave up after the last try
};

class Queue {
public:
    explicit Queue(Options options = {});
    ~Queue();
    Queue(const Queue&) = delete;

    // Runs `work` as soon as a worker is free. Returns the job's id.
    std::uint64_t add(std::string name, Work work, Retry retry = {});
    // Runs it once, after `delay`.
    std::uint64_t after(std::chrono::milliseconds delay, std::string name, Work work, Retry retry = {});
    // Runs it every `period` (the first time after one period). A failed run
    // is reported (onFailed) and the next one still comes.
    std::uint64_t every(std::chrono::milliseconds period, std::string name, Work work);
    // Runs it on a cron schedule in local time: "minute hour day month weekday",
    // each a number, "*", a list "1,15", a range "1-5" or a step "*/10" ("0 3 * * *"
    // = 03:00 every day; "*/5 9-18 * * 1-5" = every 5 minutes in office hours).
    // Sunday is 0 (or 7). A spec it can't read is an error.
    std::expected<std::uint64_t, std::string> cron(std::string_view spec, std::string name, Work work);

    bool cancel(std::uint64_t id);  // false if it is gone already (a running job finishes)
    Stats stats() const;
    // Blocks until every one-off job (add, after — with its retries) has
    // finished; repeating ones (every, cron) don't count. For tests and for a clean shutdown.
    void wait();
    // Stops the workers: running jobs finish, waiting ones are dropped.
    void stop();

    struct State;  // inside

private:
    std::shared_ptr<State> state_;
};

// The next time after `from` that matches a cron spec (local time).
std::expected<std::chrono::system_clock::time_point, std::string> nextCron(std::string_view spec,
                                                                           std::chrono::system_clock::time_point from);

}  // namespace rx::jobs
