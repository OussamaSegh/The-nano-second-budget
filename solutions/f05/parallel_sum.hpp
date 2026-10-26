// Exercise F5.1 (solution): a parallel sum and an exact shared counter.
#pragma once

#include <atomic>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

// Each worker owns one partial sum; nothing is shared until after join().
inline std::uint64_t parallel_sum(std::span<const std::uint32_t> in, unsigned threads) {
    if (threads == 0) throw std::invalid_argument("parallel_sum: threads must be >= 1");
    std::vector<std::uint64_t> partial(threads, 0);  // written by one worker each, read after join
    std::vector<std::thread> workers;
    const std::size_t chunk = in.size() / threads, extra = in.size() % threads;
    std::size_t begin = 0;
    for (unsigned t = 0; t < threads; ++t) {
        const std::size_t len = chunk + (t < extra ? 1 : 0);  // spread the remainder
        const auto part = in.subspan(begin, len);
        begin += len;
        // The initial value's type is the accumulator's type: uint64_t{0},
        // not 0, or the sum would be accumulated in int.
        workers.emplace_back([&partial, part, t] { partial[t] = std::accumulate(part.begin(), part.end(), std::uint64_t{0}); });
    }
    for (auto& w : workers) w.join();  // join: every partial write happens before our reads
    return std::accumulate(partial.begin(), partial.end(), std::uint64_t{0});
}

// `workers` threads each add 1, `per_worker` times, to one counter.
// fetch_add makes each increment indivisible; relaxed is enough because the
// count publishes nothing else, and join() orders the final load.
inline std::uint64_t exact_count(unsigned workers, std::uint64_t per_worker) {
    std::atomic<std::uint64_t> count{0};
    std::vector<std::thread> ts;
    for (unsigned w = 0; w < workers; ++w)
        ts.emplace_back([&] {
            for (std::uint64_t i = 0; i < per_worker; ++i) count.fetch_add(1, std::memory_order_relaxed);
        });
    for (auto& t : ts) t.join();
    return count.load(std::memory_order_relaxed);
}
