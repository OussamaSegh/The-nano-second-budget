// Chapter 2: false sharing, measured.
//
// Two threads each increment their *own* counter. In the first run the two
// counters sit in the same cache line; in the second each has a line to
// itself. The work is identical; only the memory layout differs.
#include <llds/common/bench.hpp>
#include <llds/common/cacheline.hpp>
#include <llds/common/cpu.hpp>

#include <atomic>
#include <cstdio>
#include <thread>

#include "example_util.hpp"

struct Together {
    std::atomic<std::uint64_t> a{0};
    std::atomic<std::uint64_t> b{0};
};

struct Apart {
    alignas(llds::kFalseSharingRange) std::atomic<std::uint64_t> a{0};
    alignas(llds::kFalseSharingRange) std::atomic<std::uint64_t> b{0};
};

template <class Counters>
static double run(const ex::Args& args, std::uint64_t iters) {
    Counters c;
    const auto t0 = llds::now_ns();
    std::thread t1([&] {
        llds::pin_this_thread(args.cpu_a);
        for (std::uint64_t i = 0; i < iters; ++i) c.a.fetch_add(1, std::memory_order_relaxed);
    });
    std::thread t2([&] {
        llds::pin_this_thread(args.cpu_b);
        for (std::uint64_t i = 0; i < iters; ++i) c.b.fetch_add(1, std::memory_order_relaxed);
    });
    t1.join();
    t2.join();
    const auto t1ns = llds::now_ns();
    return static_cast<double>(t1ns - t0) / static_cast<double>(iters);
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const auto iters = args.n(50'000'000);
    std::printf("sizeof(Together)=%zu  sizeof(Apart)=%zu\n", sizeof(Together), sizeof(Apart));
    const double shared = run<Together>(args, iters);
    const double apart = run<Apart>(args, iters);
    std::printf("same line:      %6.2f ns per increment\n", shared);
    std::printf("separate lines: %6.2f ns per increment\n", apart);
    std::printf("slowdown from false sharing: %.1fx\n", shared / apart);
    return 0;
}
