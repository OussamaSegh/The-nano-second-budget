// Chapter 1: watch your own machine's jitter.
//
// A thread spins reading the clock as fast as it can and records the gap
// between consecutive reads. On an idle core every gap should be ~20 ns. The
// long tail you actually see is the operating system and hardware stealing
// the core: timer interrupts, scheduler ticks, page faults, SMIs, migrations.
//
//   ./ch01_jitter                 # 5 s run on CPU 0
//   ./ch01_jitter --cpus=3,0      # pin to CPU 3 (try an isolated core)
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>

#include <cstdio>

#include "example_util.hpp"

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    llds::pin_this_thread(args.cpu_a);

    const std::uint64_t duration_ns = args.smoke ? 20'000'000ull : 5'000'000'000ull;
    llds::Histogram gaps;
    std::uint64_t worst = 0, worst_at = 0, over_1us = 0, over_10us = 0, stolen_ns = 0;

    const auto start = llds::now_ns();
    auto prev = start;
    for (;;) {
        const auto t = llds::now_ns();
        const auto gap = t - prev;
        gaps.record(gap);
        if (gap > 1'000) {
            ++over_1us;
            stolen_ns += gap;
        }
        if (gap > 10'000) ++over_10us;
        if (gap > worst) {
            worst = gap;
            worst_at = t - start;
        }
        prev = t;
        if (t - start > duration_ns) break;
    }

    std::printf("clock-read gaps on cpu %d:\n  %s\n", args.cpu_a, gaps.summary().c_str());
    std::printf("worst stall: %.1f us at t=%.3f s\n", worst / 1000.0, worst_at / 1e9);
    std::printf("stalls > 1 us: %llu, > 10 us: %llu\n", static_cast<unsigned long long>(over_1us),
                static_cast<unsigned long long>(over_10us));
    std::printf("time stolen by stalls > 1 us: %.3f%% of the run\n",
                100.0 * static_cast<double>(stolen_ns) / static_cast<double>(duration_ns));
    return 0;
}
