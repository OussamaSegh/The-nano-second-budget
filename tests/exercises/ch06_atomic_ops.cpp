#include <atomic_ops.hpp>

#include <algorithm>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

#include "check.hpp"

TEST("ex6.1: fetch_max under contention") {
    std::atomic<std::uint64_t> m{0};
    CHECK_EQ(fetch_max(m, std::uint64_t{5}), 0u);
    CHECK_EQ(fetch_max(m, std::uint64_t{3}), 5u);
    CHECK_EQ(m.load(), 5u);
    m = 0;
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&, t] {
            for (std::uint64_t i = 0; i < 200000; ++i) fetch_max(m, i * 4 + static_cast<std::uint64_t>(t));
        });
    for (auto& t : ts) t.join();
    CHECK_EQ(m.load(), 199999u * 4 + 3);
}

// A value whose comparisons are slow, so the test can force the interleaving
// that breaks a load-compare-store: thread A reads 0 and is still comparing
// when thread B stores a larger value; a non-atomic fetch_max then lets A
// overwrite B's value with its smaller one. A CAS loop notices and retries.
struct alignas(8) SlowValue {  // 8-byte aligned so the atomic needs no libatomic call
    std::uint32_t v;  // 8 bytes in all, so std::atomic<SlowValue> stays lock-free
    std::uint32_t delay_us;
    static void wait(SlowValue a, SlowValue b) {
        std::this_thread::sleep_for(std::chrono::microseconds(std::max(a.delay_us, b.delay_us)));
    }
    friend bool operator<(SlowValue a, SlowValue b) { wait(a, b); return a.v < b.v; }
    friend bool operator>(SlowValue a, SlowValue b) { wait(a, b); return a.v > b.v; }
    friend bool operator<=(SlowValue a, SlowValue b) { wait(a, b); return a.v <= b.v; }
    friend bool operator>=(SlowValue a, SlowValue b) { wait(a, b); return a.v >= b.v; }
};

TEST("ex6.1: fetch_max with a forced interleaving") {
    std::atomic<SlowValue> m{SlowValue{0, 0}};
    std::latch start(2);
    std::thread slow([&] {
        start.arrive_and_wait();
        fetch_max(m, SlowValue{10, 20000});  // compares for 20 ms
    });
    std::thread fast([&] {
        start.arrive_and_wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));  // inside slow's window
        fetch_max(m, SlowValue{20, 0});
    });
    slow.join();
    fast.join();
    CHECK_EQ(m.load().v, 20u);  // the larger value must win whatever the interleaving
}

TEST("ex6.2: bounded increment never overshoots") {
    for (int round = 0; round < 200; ++round) {
        std::atomic<std::uint32_t> c{0};
        std::atomic<int> granted{0};
        std::atomic<std::uint32_t> peak{0};
        std::latch start(4);
        std::vector<std::thread> ts;
        for (int t = 0; t < 4; ++t)
            ts.emplace_back([&] {
                start.arrive_and_wait();  // all four contend from the first iteration
                for (int i = 0; i < 2000; ++i) {
                    if (try_increment_below(c, 3)) {
                        granted++;
                        auto now = c.load();
                        auto p = peak.load();
                        while (now > p && !peak.compare_exchange_weak(p, now)) {}
                        c.fetch_sub(1);
                    }
                }
            });
        for (auto& t : ts) t.join();
        CHECK(peak.load() <= 3);
        CHECK(granted.load() > 0);
        CHECK_EQ(c.load(), 0u);
    }
}

TEST_MAIN()
