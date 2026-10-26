// Chapter 5: five spinlocks and std::mutex under contention.
//
// T threads each acquire the lock, do a tiny critical section (increment a
// shared counter and touch one cache line), and release it. We report total
// throughput and the per-acquisition latency distribution of one thread.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/sync/spinlock.hpp>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#include "example_util.hpp"


template <class Lock>
struct Runner {
    static void run(const char* name, unsigned threads, std::uint64_t iters) {
        Lock lock;
        std::uint64_t shared = 0;
        std::atomic<bool> go{false};
        llds::Histogram h0;
        static const llds::TscClock clock;
        std::vector<std::thread> ts;
        const auto t0 = llds::now_ns();
        for (unsigned t = 0; t < threads; ++t)
            ts.emplace_back([&, t] {
                llds::pin_this_thread(static_cast<int>(t % llds::hardware_threads()));
                while (!go.load(std::memory_order_acquire)) llds::cpu_relax();
                for (std::uint64_t i = 0; i < iters; ++i) {
                    const auto c0 = llds::cycles();
                    acquire(lock);
                    const auto c1 = llds::cycles();
                    ++shared;
                    release(lock);
                    if (t == 0) h0.record(static_cast<std::uint64_t>(clock.to_ns(c1 - c0)));
                }
            });
        go.store(true, std::memory_order_release);
        for (auto& t : ts) t.join();
        const auto elapsed = llds::now_ns() - t0;
        const double mops = static_cast<double>(threads * iters) / (static_cast<double>(elapsed) / 1e3);
        std::printf("%-12s T=%-2u %8.2f Mops/s  acquire p50=%5llu p99=%7llu p99.9=%8llu max=%9llu ns  %s\n",
                    name, threads, mops, static_cast<unsigned long long>(h0.percentile(50)),
                    static_cast<unsigned long long>(h0.percentile(99)),
                    static_cast<unsigned long long>(h0.percentile(99.9)),
                    static_cast<unsigned long long>(h0.max()),
                    shared == threads * iters ? "" : "COUNT MISMATCH");
    }

    // McsLock needs a queue node per acquisition; everything else is BasicLockable.
    static thread_local inline llds::McsLock::Node node;
    static void acquire(Lock& l) {
        if constexpr (std::is_same_v<Lock, llds::McsLock>) l.lock(node);
        else l.lock();
    }
    static void release(Lock& l) {
        if constexpr (std::is_same_v<Lock, llds::McsLock>) l.unlock(node);
        else l.unlock();
    }
};

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const auto iters = args.n(1'000'000);
    const unsigned max_t = std::min(8u, llds::hardware_threads());
    for (unsigned t = 1; t <= max_t; t *= 2) {
        Runner<std::mutex>::run("std::mutex", t, iters);
        Runner<llds::TasLock>::run("TAS", t, iters);
        Runner<llds::TtasLock>::run("TTAS", t, iters);
        Runner<llds::BackoffLock>::run("TTAS+backoff", t, iters);
        Runner<llds::TicketLock>::run("ticket", t, iters);
        Runner<llds::McsLock>::run("MCS", t, iters);
        std::printf("\n");
    }
    return 0;
}
