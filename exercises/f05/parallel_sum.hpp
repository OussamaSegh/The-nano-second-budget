// Exercise F5.1: a parallel sum and an exact shared counter.
//
// parallel_sum(in, threads): split `in` into `threads` contiguous parts of
// nearly equal size, sum each part on its own std::thread into a 64-bit
// result owned by that thread, join, then add the partial sums. Throw
// std::invalid_argument if threads == 0. More threads than elements is
// allowed (some parts are empty). No atomics and no mutex are needed: say
// why in a comment.
//
// exact_count(workers, per_worker): start `workers` threads; each adds 1 to
// one shared counter `per_worker` times. Return the exact total. Choose the
// weakest memory order that is correct and justify it.
#pragma once

#include <atomic>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

inline std::uint64_t parallel_sum(std::span<const std::uint32_t> in, unsigned threads) {
    // TODO
    (void)in;
    (void)threads;
    return 0;
}

inline std::uint64_t exact_count(unsigned workers, std::uint64_t per_worker) {
    // TODO: the obvious `++plain_counter` is a data race. Run under TSan.
    (void)workers;
    (void)per_worker;
    return 0;
}
