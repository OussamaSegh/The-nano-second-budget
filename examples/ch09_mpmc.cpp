// Chapter 9: multi-producer multi-consumer queues compared.
//
//   mutex+deque   a std::deque behind one std::mutex
//   MsQueue       Michael-Scott unbounded lock-free queue (allocates per item)
//   Vyukov        bounded array queue with per-slot sequence numbers
//
// P producers and C consumers move N items in total; we report throughput
// and the end-to-end latency of items (enqueue timestamp to dequeue).
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/queue/mpmc_bounded.hpp>
#include <llds/queue/ms_queue.hpp>

#include <atomic>
#include <cstdio>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "example_util.hpp"

struct Msg {
    std::uint64_t sent_cycles = 0;
    std::uint64_t value = 0;
};

class MutexQueue {
public:
    bool try_push(const Msg& m) {
        std::lock_guard g(mu_);
        if (q_.size() >= 4096) return false;
        q_.push_back(m);
        return true;
    }
    bool try_pop(Msg& m) {
        std::lock_guard g(mu_);
        if (q_.empty()) return false;
        m = q_.front();
        q_.pop_front();
        return true;
    }

private:
    std::mutex mu_;
    std::deque<Msg> q_;
};

class MsAdapter {
public:
    bool try_push(const Msg& m) {
        q_.push(m);
        return true;
    }
    bool try_pop(Msg& m) {
        if (auto v = q_.try_pop()) {
            m = *v;
            return true;
        }
        return false;
    }

private:
    llds::MsQueue<Msg> q_;
};

class VyukovAdapter {
public:
    bool try_push(const Msg& m) { return q_.try_push(m); }
    bool try_pop(Msg& m) { return q_.try_pop(m); }

private:
    llds::MpmcBoundedQueue<Msg> q_{4096};
};

// gap_ns == 0: flood (measures capacity; latency is mostly queueing).
// gap_ns > 0: each producer sends one item every gap_ns (measures hand-off).
template <class Q>
static void run(const char* name, unsigned P, unsigned C, std::uint64_t per_producer, std::uint64_t gap_ns) {
    Q q;
    static const llds::TscClock clock;
    std::atomic<std::uint64_t> consumed{0};
    const std::uint64_t total = per_producer * P;
    std::vector<llds::Histogram> lat(C);
    std::vector<std::thread> ts;
    std::atomic<unsigned> ready{0};
    const auto t0 = llds::now_ns();
    for (unsigned p = 0; p < P; ++p)
        ts.emplace_back([&, p] {
            llds::pin_this_thread(static_cast<int>(p % llds::hardware_threads()));
            ready++;
            while (ready.load() < P + C) llds::cpu_relax();
            const auto start = llds::now_ns();
            for (std::uint64_t i = 0; i < per_producer; ++i) {
                if (gap_ns) {
                    const auto due = start + i * gap_ns;
                    while (llds::now_ns() < due) llds::cpu_relax();
                }
                Msg m{llds::cycles(), i};
                while (!q.try_push(m)) llds::cpu_relax();
            }
        });
    for (unsigned c = 0; c < C; ++c)
        ts.emplace_back([&, c] {
            llds::pin_this_thread(static_cast<int>((P + c) % llds::hardware_threads()));
            ready++;
            Msg m;
            while (consumed.load(std::memory_order_relaxed) < total) {
                if (q.try_pop(m)) {
                    lat[c].record(static_cast<std::uint64_t>(clock.to_ns(llds::cycles() - m.sent_cycles)));
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    for (auto& t : ts) t.join();
    const double secs = static_cast<double>(llds::now_ns() - t0) / 1e9;
    llds::Histogram all;
    for (auto& h : lat) all.merge(h);
    if (gap_ns == 0)
        std::printf("%-12s P=%u C=%u  flood  %7.2f M items/s\n", name, P, C, static_cast<double>(total) / secs / 1e6);
    else
        std::printf("%-12s P=%u C=%u  paced  latency p50=%6llu p99=%7llu p99.9=%8llu ns\n", name, P, C,
                    static_cast<unsigned long long>(all.percentile(50)),
                    static_cast<unsigned long long>(all.percentile(99)),
                    static_cast<unsigned long long>(all.percentile(99.9)));
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const auto n = args.n(2'000'000);
    const unsigned hw = llds::hardware_threads();
    std::vector<std::pair<unsigned, unsigned>> configs{{1, 1}, {2, 2}, {3, 1}, {1, 3}};
    if (hw >= 8) configs.push_back({hw / 2, hw / 2});
    for (auto [p, c] : configs) {
        if (p + c > hw) continue;
        run<MutexQueue>("mutex+deque", p, c, n, 0);
        run<MsAdapter>("MsQueue", p, c, n, 0);
        run<VyukovAdapter>("Vyukov", p, c, n, 0);
        // Paced: each producer sends one item every 2 us.
        const auto np = args.n(500'000);
        run<MutexQueue>("mutex+deque", p, c, np, 2000);
        run<MsAdapter>("MsQueue", p, c, np, 2000);
        run<VyukovAdapter>("Vyukov", p, c, np, 2000);
        std::printf("\n");
    }
    return 0;
}
