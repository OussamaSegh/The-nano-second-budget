// llds/common/cpu.hpp
// CPU pinning and spin-wait hints.
#pragma once

#include <thread>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace llds {

// Tell the core we are in a spin-wait loop. On x86 `pause` stops the
// pipeline from speculatively piling up loads of the contended line and saves
// power; on Skylake and later it costs ~140 cycles, on older parts ~10.
// On ARMv8 `yield` is a hint with similar intent.
inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

// Pin the calling thread to one logical CPU. Returns false on failure (for
// example when the CPU does not exist or is outside our cpuset).
inline bool pin_this_thread(int cpu) noexcept {
#if defined(__linux__)
    if (cpu < 0) return false;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
    (void)cpu;
    return false;
#endif
}

inline unsigned hardware_threads() noexcept {
    unsigned n = std::thread::hardware_concurrency();
    return n ? n : 1;
}

// Exponential backoff for contended CAS loops and spinlocks.
class Backoff {
public:
    explicit Backoff(unsigned max_spins = 1024) noexcept : max_(max_spins) {}

    void pause() noexcept {
        for (unsigned i = 0; i < spins_; ++i) cpu_relax();
        if (spins_ < max_) spins_ <<= 1;
        else std::this_thread::yield();
    }
    void reset() noexcept { spins_ = 1; }

private:
    unsigned spins_ = 1;
    unsigned max_;
};

}  // namespace llds
