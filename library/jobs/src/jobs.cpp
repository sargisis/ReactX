#include "rx/jobs.hpp"

#include <algorithm>
#include <atomic>
#include <bitset>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include "reactx/parse.hpp"

namespace rx::jobs {

using Clock = std::chrono::system_clock;
using namespace std::chrono;

// ---- cron ----

namespace {

struct CronSpec {
    std::bitset<60> minute;
    std::bitset<24> hour;
    std::bitset<32> day;    // 1..31
    std::bitset<13> month;  // 1..12
    std::bitset<7> weekday; // 0 = Sunday
    bool anyDay = false, anyWeekday = false;
};

template <std::size_t N>
std::expected<bool, std::string> parseField(std::string_view text, int lo, int hi, std::bitset<N>& out, bool weekday = false) {
    bool star = false;
    while (!text.empty()) {
        std::size_t comma = text.find(',');
        std::string_view part = text.substr(0, comma);
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
        int step = 1;
        if (auto slash = part.find('/'); slash != std::string_view::npos) {
            auto s = parseNumber<int>(part.substr(slash + 1));
            if (!s || *s < 1) return std::unexpected("a bad step in \"" + std::string(part) + "\"");
            step = *s;
            part = part.substr(0, slash);
        }
        int from = lo, to = hi;
        if (part == "*") {
            star = step == 1;
        } else if (auto dash = part.find('-'); dash != std::string_view::npos) {
            auto a = parseNumber<int>(part.substr(0, dash)), b = parseNumber<int>(part.substr(dash + 1));
            if (!a || !b) return std::unexpected("a bad range \"" + std::string(part) + "\"");
            from = *a;
            to = *b;
        } else {
            auto a = parseNumber<int>(part);
            if (!a) return std::unexpected("a bad value \"" + std::string(part) + "\"");
            from = *a;
            to = step > 1 ? hi : *a;  // "5/15" = from 5 on, every 15
        }
        const int top = weekday ? 7 : hi;
        if (from < lo || to > top || from > to)
            return std::unexpected("\"" + std::string(part) + "\" is outside " + std::to_string(lo) + "-" + std::to_string(top));
        for (int v = from; v <= to; v += step) out.set(static_cast<std::size_t>(weekday && v == 7 ? 0 : v));
    }
    return star;
}

std::expected<CronSpec, std::string> parseCron(std::string_view spec) {
    std::vector<std::string_view> fields;
    for (std::size_t i = 0; i < spec.size();) {
        while (i < spec.size() && spec[i] == ' ') ++i;
        std::size_t j = i;
        while (j < spec.size() && spec[j] != ' ') ++j;
        if (j > i) fields.push_back(spec.substr(i, j - i));
        i = j;
    }
    if (fields.size() != 5)
        return std::unexpected("cron \"" + std::string(spec) + "\": needs 5 fields (minute hour day month weekday)");
    CronSpec c;
    auto fail = [&](const std::string& why) { return std::unexpected("cron \"" + std::string(spec) + "\": " + why); };
    if (auto r = parseField(fields[0], 0, 59, c.minute); !r) return fail(r.error());
    if (auto r = parseField(fields[1], 0, 23, c.hour); !r) return fail(r.error());
    auto day = parseField(fields[2], 1, 31, c.day);
    if (!day) return fail(day.error());
    if (auto r = parseField(fields[3], 1, 12, c.month); !r) return fail(r.error());
    auto wd = parseField(fields[4], 0, 6, c.weekday, true);
    if (!wd) return fail(wd.error());
    c.anyDay = *day;
    c.anyWeekday = *wd;
    return c;
}

// Like cron: when both the day and the weekday are limited, either one matches.
bool dayMatches(const CronSpec& c, year_month_day ymd) {
    const bool d = c.day.test(static_cast<unsigned>(ymd.day()));
    const bool w = c.weekday.test(weekday(sys_days(ymd)).c_encoding());
    if (c.anyDay && c.anyWeekday) return true;
    if (c.anyDay) return w;
    if (c.anyWeekday) return d;
    return d || w;
}

std::expected<Clock::time_point, std::string> next(const CronSpec& c, Clock::time_point from) {
    const time_zone* zone = current_zone();
    local_time<minutes> t = floor<minutes>(zone->to_local(from)) + minutes(1);
    const auto limit = t + days(366 * 5);
    while (t < limit) {
        const local_days day = floor<days>(t);
        const year_month_day ymd{sys_days(day.time_since_epoch())};
        if (!c.month.test(static_cast<unsigned>(ymd.month()))) {  // to the 1st of the next month
            const year_month ym = ymd.year() / ymd.month() + months(1);
            t = local_days(sys_days(ym / 1).time_since_epoch());
            continue;
        }
        if (!dayMatches(c, ymd)) {
            t = day + days(1);
            continue;
        }
        const hh_mm_ss hms{t - day};
        if (!c.hour.test(static_cast<std::size_t>(hms.hours().count()))) {
            t = day + hms.hours() + hours(1);
            continue;
        }
        if (!c.minute.test(static_cast<std::size_t>(hms.minutes().count()))) {
            t += minutes(1);
            continue;
        }
        return zone->to_sys(t, choose::earliest);
    }
    return std::unexpected("cron: no time matches in the next 5 years");
}

}  // namespace

std::expected<Clock::time_point, std::string> nextCron(std::string_view spec, Clock::time_point from) {
    auto c = parseCron(spec);
    if (!c) return std::unexpected(c.error());
    return next(*c, from);
}

// ---- the queue ----

namespace {
enum class Kind { Once, Every, Cron };

struct Job {
    std::uint64_t id = 0;
    std::string name;
    Work work;
    Retry retry;
    Kind kind = Kind::Once;
    milliseconds period{0};
    CronSpec cron;
    int attempt = 0;
    bool cancelled = false;  // under the state's mutex
};

struct Due {
    Clock::time_point at;
    std::uint64_t seq;
    std::shared_ptr<Job> job;
    bool operator>(const Due& o) const { return at != o.at ? at > o.at : seq > o.seq; }
};
}  // namespace

struct Queue::State {
    Options options;
    std::mutex mutex;
    std::condition_variable cv;
    std::priority_queue<Due, std::vector<Due>, std::greater<>> due;
    std::map<std::uint64_t, std::shared_ptr<Job>> jobs;  // alive (waiting, scheduled or running)
    std::uint64_t nextId = 1, seq = 0;
    std::size_t running = 0, pendingOnce = 0;
    std::uint64_t done = 0, failed = 0;
    bool stopping = false;
    std::vector<std::jthread> workers;

    void push(Clock::time_point at, std::shared_ptr<Job> job) {
        due.push({at, seq++, std::move(job)});
        cv.notify_all();
    }

    std::uint64_t enqueue(std::shared_ptr<Job> job, Clock::time_point at) {
        std::lock_guard lock(mutex);
        job->id = nextId++;
        if (job->kind == Kind::Once) ++pendingOnce;
        jobs[job->id] = job;
        push(at, job);
        return job->id;
    }

    void finishOnce(const std::shared_ptr<Job>& job) {  // under the lock
        jobs.erase(job->id);
        if (pendingOnce > 0) --pendingOnce;
        cv.notify_all();
    }

    void work() {
        std::unique_lock lock(mutex);
        while (true) {
            if (stopping) return;
            if (due.empty()) {
                cv.wait(lock);
                continue;
            }
            if (auto at = due.top().at; at > Clock::now()) {
                cv.wait_until(lock, at);
                continue;
            }
            std::shared_ptr<Job> job = due.top().job;
            due.pop();
            if (job->cancelled) continue;  // cancel() already let it go
            ++running;
            lock.unlock();
            std::string error;
            bool ok = true;
            try {
                job->work();
            } catch (const std::exception& e) {
                ok = false;
                error = e.what();
            } catch (...) {
                ok = false;
                error = "unknown error";
            }
            lock.lock();
            --running;
            ++job->attempt;
            std::optional<Failure> report;
            const auto now = Clock::now();
            if (ok) {
                ++done;
                job->attempt = 0;
            } else if (job->kind == Kind::Once && job->attempt < job->retry.attempts) {
                auto delay = duration<double, std::milli>(job->retry.delay) *
                             std::pow(job->retry.backoff, static_cast<double>(job->attempt - 1));
                auto wait = std::min(duration_cast<milliseconds>(delay), job->retry.maxDelay);
                if (!job->cancelled && !stopping) push(now + wait, job);
                cv.notify_all();
                continue;
            } else {
                ++failed;
                report = Failure{job->id, job->name, error, job->attempt};
                job->attempt = 0;
            }
            if (job->kind == Kind::Once) {
                finishOnce(job);
            } else if (!job->cancelled) {
                if (job->kind == Kind::Every) push(now + job->period, job);
                else if (auto at = next(job->cron, now)) push(*at, job);
                else jobs.erase(job->id);
            }
            cv.notify_all();
            if (report && options.onFailed) {
                auto onFailed = options.onFailed;
                lock.unlock();
                onFailed(*report);
                lock.lock();
            }
        }
    }
};

Queue::Queue(Options options) : state_(std::make_shared<State>()) {
    state_->options = std::move(options);
    int n = state_->options.workers > 0 ? state_->options.workers : static_cast<int>(std::thread::hardware_concurrency());
    n = std::max(n, 1);
    for (int i = 0; i < n; ++i) state_->workers.emplace_back([s = state_.get()] { s->work(); });
}

Queue::~Queue() { stop(); }

void Queue::stop() {
    {
        std::lock_guard lock(state_->mutex);
        state_->stopping = true;
        state_->pendingOnce = 0;
        state_->cv.notify_all();
    }
    for (auto& w : state_->workers)
        if (w.joinable() && w.get_id() != std::this_thread::get_id()) w.join();
}

std::uint64_t Queue::add(std::string name, Work work, Retry retry) {
    return after(milliseconds(0), std::move(name), std::move(work), retry);
}

std::uint64_t Queue::after(milliseconds delay, std::string name, Work work, Retry retry) {
    auto job = std::make_shared<Job>();
    job->name = std::move(name);
    job->work = std::move(work);
    job->retry = retry;
    job->retry.attempts = std::max(job->retry.attempts, 1);
    return state_->enqueue(std::move(job), Clock::now() + delay);
}

std::uint64_t Queue::every(milliseconds period, std::string name, Work work) {
    auto job = std::make_shared<Job>();
    job->name = std::move(name);
    job->work = std::move(work);
    job->kind = Kind::Every;
    job->period = std::max(period, milliseconds(1));
    return state_->enqueue(std::move(job), Clock::now() + job->period);
}

std::expected<std::uint64_t, std::string> Queue::cron(std::string_view spec, std::string name, Work work) {
    auto c = parseCron(spec);
    if (!c) return std::unexpected(c.error());
    auto at = next(*c, Clock::now());
    if (!at) return std::unexpected(at.error());
    auto job = std::make_shared<Job>();
    job->name = std::move(name);
    job->work = std::move(work);
    job->kind = Kind::Cron;
    job->cron = *c;
    return state_->enqueue(std::move(job), *at);
}

bool Queue::cancel(std::uint64_t id) {
    std::lock_guard lock(state_->mutex);
    auto it = state_->jobs.find(id);
    if (it == state_->jobs.end()) return false;
    auto job = it->second;
    job->cancelled = true;
    if (job->kind == Kind::Once) state_->finishOnce(job);
    else state_->jobs.erase(it);
    return true;
}

Stats Queue::stats() const {
    std::lock_guard lock(state_->mutex);
    return {.waiting = state_->jobs.size() - std::min(state_->jobs.size(), state_->running),
            .running = state_->running,
            .done = state_->done,
            .failed = state_->failed};
}

void Queue::wait() {
    std::unique_lock lock(state_->mutex);
    state_->cv.wait(lock, [&] { return state_->pendingOnce == 0 || state_->stopping; });
}

}  // namespace rx::jobs
