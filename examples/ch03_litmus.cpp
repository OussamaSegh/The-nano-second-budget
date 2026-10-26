// Chapter 3: memory-model litmus tests, run for real.
//
// Store buffering (SB):          Message passing (MP):
//   T1: x = 1; r1 = y;              T1: data = 42; flag = 1;
//   T2: y = 1; r2 = x;              T2: if (flag) r = data;
//   forbidden by seq_cst:           forbidden by release/acquire:
//     r1 == 0 && r2 == 0              flag seen but r != 42
//
// x86 (TSO) has a store buffer, so SB with anything weaker than seq_cst
// shows the "impossible" outcome within a few thousand trials. MP never
// fails on x86 even with relaxed, because x86 never reorders two stores or
// two loads, but it does fail on ARM and POWER. Remember that "works on my
// x86" is not "correct": the compiler may also reorder relaxed operations.
#include <llds/common/cpu.hpp>

#include <atomic>
#include <cstdio>
#include <thread>

#include "example_util.hpp"

// A reusable two-thread start line so both sides of each trial run at once.
class StartLine {
public:
    void arrive_and_wait(std::uint64_t round) {
        // Round r is complete when both threads have arrived r + 1 times.
        arrived_.fetch_add(1, std::memory_order_acq_rel);
        while (arrived_.load(std::memory_order_acquire) < 2 * (round + 1)) llds::cpu_relax();
    }

private:
    std::atomic<std::uint64_t> arrived_{0};
};

template <std::memory_order Store, std::memory_order Load>
static std::uint64_t store_buffering(const ex::Args& args, std::uint64_t trials) {
    std::atomic<int> x{0}, y{0};
    int r1 = 0, r2 = 0;
    std::uint64_t both_zero = 0;
    StartLine start, done;
    std::thread t2([&] {
        llds::pin_this_thread(args.cpu_b);
        for (std::uint64_t i = 0; i < trials; ++i) {
            start.arrive_and_wait(i);
            y.store(1, Store);
            r2 = x.load(Load);
            done.arrive_and_wait(i);
        }
    });
    llds::pin_this_thread(args.cpu_a);
    for (std::uint64_t i = 0; i < trials; ++i) {
        x.store(0, std::memory_order_relaxed);
        y.store(0, std::memory_order_relaxed);
        start.arrive_and_wait(i);
        x.store(1, Store);
        r1 = y.load(Load);
        done.arrive_and_wait(i);
        if (r1 == 0 && r2 == 0) ++both_zero;
    }
    t2.join();
    return both_zero;
}

template <std::memory_order Store, std::memory_order Load>
static std::uint64_t message_passing(const ex::Args& args, std::uint64_t trials) {
    std::atomic<int> data{0}, flag{0};
    int seen_flag = 0, seen_data = 0;
    std::uint64_t violations = 0;
    StartLine start, done;
    std::thread t2([&] {
        llds::pin_this_thread(args.cpu_b);
        for (std::uint64_t i = 0; i < trials; ++i) {
            start.arrive_and_wait(i);
            seen_flag = flag.load(Load);
            seen_data = data.load(std::memory_order_relaxed);
            done.arrive_and_wait(i);
        }
    });
    llds::pin_this_thread(args.cpu_a);
    for (std::uint64_t i = 0; i < trials; ++i) {
        data.store(0, std::memory_order_relaxed);
        flag.store(0, std::memory_order_relaxed);
        start.arrive_and_wait(i);
        data.store(42, std::memory_order_relaxed);
        flag.store(1, Store);
        done.arrive_and_wait(i);
        if (seen_flag == 1 && seen_data != 42) ++violations;
    }
    t2.join();
    return violations;
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    const auto trials = args.n(2'000'000);
    using std::memory_order_acquire, std::memory_order_relaxed, std::memory_order_release,
        std::memory_order_seq_cst;

    std::printf("store buffering, %llu trials (count of r1 == r2 == 0)\n",
                static_cast<unsigned long long>(trials));
    std::printf("  relaxed          : %llu\n", static_cast<unsigned long long>(
                    store_buffering<memory_order_relaxed, memory_order_relaxed>(args, trials)));
    std::printf("  release/acquire  : %llu\n", static_cast<unsigned long long>(
                    store_buffering<memory_order_release, memory_order_acquire>(args, trials)));
    std::printf("  seq_cst          : %llu   (must be 0)\n", static_cast<unsigned long long>(
                    store_buffering<memory_order_seq_cst, memory_order_seq_cst>(args, trials)));

    std::printf("message passing, %llu trials (count of flag seen but data stale)\n",
                static_cast<unsigned long long>(trials));
    std::printf("  relaxed          : %llu   (0 on x86, nonzero possible on ARM)\n",
                static_cast<unsigned long long>(
                    message_passing<memory_order_relaxed, memory_order_relaxed>(args, trials)));
    std::printf("  release/acquire  : %llu   (must be 0)\n", static_cast<unsigned long long>(
                    message_passing<memory_order_release, memory_order_acquire>(args, trials)));
    return 0;
}
