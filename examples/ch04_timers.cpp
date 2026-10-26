// Chapter 4: what does it cost to ask the time?
//
// Before trusting any latency number, know the resolution and overhead of
// the clock that produced it.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>

#include <chrono>
#include <cstdio>

#include "example_util.hpp"

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    llds::pin_this_thread(args.cpu_a);
    const auto n = args.n(10'000'000);

    std::uint64_t sink = 0;
    auto per_call = [&](auto&& f) {
        return llds::measure_batch([&] { sink += f(); }, n);
    };
    std::printf("%-34s %8.2f ns\n", "clock_gettime(CLOCK_MONOTONIC)", per_call([] { return llds::now_ns(); }));
    std::printf("%-34s %8.2f ns\n", "std::chrono::steady_clock::now()", per_call([] {
                    return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
                }));
    std::printf("%-34s %8.2f ns\n", "rdtsc", per_call([] { return llds::cycles(); }));
    std::printf("%-34s %8.2f ns\n", "rdtscp", per_call([] { return llds::cycles_serialized(); }));
    llds::do_not_optimize(sink);

    llds::TscClock tsc;
    std::printf("TSC: %.4f ns per cycle (%.3f GHz)\n", tsc.ns_per_cycle(), 1.0 / tsc.ns_per_cycle());

    // Smallest observable non-zero difference between two clock reads.
    std::uint64_t min_step = ~0ull;
    for (std::uint64_t i = 0; i < n / 10 + 1; ++i) {
        auto a = llds::now_ns(), b = llds::now_ns();
        if (b > a) min_step = std::min(min_step, b - a);
    }
    std::printf("clock_gettime minimum observable step: %llu ns\n",
                static_cast<unsigned long long>(min_step));

    // The harness on a trivial operation: this is your measurement floor.
    std::uint64_t x = 1;
    auto h = llds::measure_each([&] { x = x * 6364136223846793005ull + 1; }, {args.n(1'000'000), 1000});
    llds::do_not_optimize(x);
    std::printf("measure_each(empty-ish op): %s\n", h.summary().c_str());
    return 0;
}
