// llds/common/bench.hpp
// The book's minimal benchmark harness (built in Chapter 4).
#pragma once

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/utsname.h>
#include <algorithm>
#include <thread>

#include "histogram.hpp"
#include "timer.hpp"

namespace llds {

// Print the machine description that every benchmark result must carry.
inline void print_environment(std::FILE* out = stdout) {
    std::string model = "unknown";
    std::ifstream cpuinfo("/proc/cpuinfo");
    for (std::string line; std::getline(cpuinfo, line);) {
        if (line.rfind("model name", 0) == 0) {
            model = line.substr(line.find(':') + 2);
            break;
        }
    }
    utsname u{};
    uname(&u);
    std::string governor = "n/a";
    std::ifstream gov("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (gov) std::getline(gov, governor);
    std::fprintf(out, "# cpu: %s\n# kernel: %s %s\n# governor: %s\n# threads: %u\n",
                 model.c_str(), u.sysname, u.release, governor.c_str(),
                 std::thread::hardware_concurrency());
}

struct BenchOptions {
    std::uint64_t iterations = 1'000'000;
    std::uint64_t warmup = 10'000;
};

// Time each call of `op` individually and return the latency distribution in
// nanoseconds. The cost of the timer itself is measured and subtracted.
template <class Op>
Histogram measure_each(Op&& op, const BenchOptions& opt = {}) {
    static const TscClock clock;  // calibrate once per process
    for (std::uint64_t i = 0; i < opt.warmup; ++i) op();

    // Measure the empty timed region so we can subtract it.
    std::uint64_t overhead = ~0ull;
    for (int i = 0; i < 1000; ++i) {
        const auto t0 = cycles();
        clobber_memory();
        const auto t1 = cycles_serialized();
        overhead = std::min(overhead, t1 - t0);
    }

    Histogram h;
    for (std::uint64_t i = 0; i < opt.iterations; ++i) {
        const auto t0 = cycles();
        op();
        const auto t1 = cycles_serialized();
        const auto d = (t1 - t0) > overhead ? (t1 - t0) - overhead : 0;
        h.record(static_cast<std::uint64_t>(clock.to_ns(d) + 0.5));
    }
    return h;
}

// Time a whole batch and return mean nanoseconds per operation. Use this when
// a single operation is shorter than the timer resolution.
template <class Op>
double measure_batch(Op&& op, std::uint64_t ops) {
    for (std::uint64_t i = 0; i < ops / 10 + 1; ++i) op();
    const auto t0 = now_ns();
    for (std::uint64_t i = 0; i < ops; ++i) op();
    const auto t1 = now_ns();
    return static_cast<double>(t1 - t0) / static_cast<double>(ops);
}

}  // namespace llds
