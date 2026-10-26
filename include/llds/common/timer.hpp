// llds/common/timer.hpp
// Low-overhead clocks for latency measurement.
#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#define LLDS_HAS_RDTSC 1
#else
#define LLDS_HAS_RDTSC 0
#endif

namespace llds {

// Monotonic nanoseconds from CLOCK_MONOTONIC. On Linux this goes through the
// vDSO and costs roughly 15-20 ns on bare metal and about 30 ns in a VM; it is the safe default.
inline std::uint64_t now_ns() noexcept {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull +
           static_cast<std::uint64_t>(ts.tv_nsec);
}

// Raw cycle counter. On x86 this is the invariant TSC (constant rate,
// independent of frequency scaling on any CPU from the last decade). On other
// architectures we fall back to now_ns() so the code still runs.
inline std::uint64_t cycles() noexcept {
#if LLDS_HAS_RDTSC
    return __rdtsc();
#else
    return now_ns();
#endif
}

// rdtscp waits for all earlier instructions to retire before reading the
// counter, which makes it the right choice for the *end* of a timed region.
inline std::uint64_t cycles_serialized() noexcept {
#if LLDS_HAS_RDTSC
    unsigned aux;
    return __rdtscp(&aux);
#else
    return now_ns();
#endif
}

// Calibrates cycles() against the monotonic clock. Call once at startup.
class TscClock {
public:
    TscClock() { calibrate(); }

    void calibrate(std::chrono::milliseconds window = std::chrono::milliseconds(50)) {
        const auto c0 = cycles();
        const auto t0 = now_ns();
        std::this_thread::sleep_for(window);
        const auto c1 = cycles();
        const auto t1 = now_ns();
        ns_per_cycle_ = static_cast<double>(t1 - t0) / static_cast<double>(c1 - c0);
    }

    double to_ns(std::uint64_t cycle_delta) const noexcept {
        return static_cast<double>(cycle_delta) * ns_per_cycle_;
    }
    double ns_per_cycle() const noexcept { return ns_per_cycle_; }

private:
    double ns_per_cycle_ = 1.0;
};

// Prevent the optimizer from deleting a computation whose result is unused.
template <class T>
inline void do_not_optimize(T const& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

inline void clobber_memory() { asm volatile("" : : : "memory"); }

}  // namespace llds
