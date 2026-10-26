// Chapter 2: measure the memory hierarchy with a pointer chase.
//
// We build a random cyclic permutation inside a buffer of a given size and
// follow it: each load's address depends on the previous load's value, so
// the CPU cannot overlap or prefetch them. The time per step is the load-to-
// use latency of whichever level of the hierarchy the buffer fits in.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

#include "example_util.hpp"

struct alignas(64) Line {
    Line* next;
    char pad[56];
};

static double chase_ns(std::size_t bytes, std::uint64_t steps) {
    const std::size_t n = std::max<std::size_t>(bytes / sizeof(Line), 2);
    std::vector<Line> lines(n);
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin() + 1, order.end(), std::mt19937_64(42));
    for (std::size_t i = 0; i < n; ++i) lines[order[i]].next = &lines[order[(i + 1) % n]];

    Line* p = &lines[0];
    for (std::size_t i = 0; i < n; ++i) p = p->next;  // warm up
    const auto t0 = llds::now_ns();
    for (std::uint64_t i = 0; i < steps; ++i) p = p->next;
    const auto t1 = llds::now_ns();
    llds::do_not_optimize(p);
    return static_cast<double>(t1 - t0) / static_cast<double>(steps);
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    llds::pin_this_thread(args.cpu_a);
    std::printf("%12s %10s\n", "working set", "ns/load");
    const std::size_t max_bytes = args.smoke ? (1u << 20) : (512u << 20);
    for (std::size_t bytes = 4096; bytes <= max_bytes; bytes *= 2) {
        const double ns = chase_ns(bytes, args.n(20'000'000));
        if (bytes < (1u << 20)) std::printf("%9zu KiB %10.2f\n", bytes >> 10, ns);
        else std::printf("%9zu MiB %10.2f\n", bytes >> 20, ns);
    }
    return 0;
}
