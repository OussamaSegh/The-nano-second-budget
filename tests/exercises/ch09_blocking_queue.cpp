#include <blocking_queue.hpp>

#include <chrono>
#include <ctime>
#include <thread>
#include <vector>

#include "check.hpp"

static double thread_cpu_ms() {
    timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) * 1e3 + static_cast<double>(ts.tv_nsec) / 1e6;
}

TEST("ex9.1: pop sleeps (does not spin) while waiting for a late producer") {
    BlockingQueue<int> q(4);
    std::thread late([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        q.push(42);
    });
    const auto t0 = std::chrono::steady_clock::now();
    const double cpu0 = thread_cpu_ms();
    const int v = q.pop();
    const double cpu_used = thread_cpu_ms() - cpu0;
    const auto waited = std::chrono::steady_clock::now() - t0;
    late.join();
    CHECK_EQ(v, 42);
    CHECK(waited >= std::chrono::milliseconds(80));
    std::printf("  waited ~100 ms using %.2f ms of CPU\n", cpu_used);
    CHECK(cpu_used < 20.0);  // a spinning consumer would use ~100 ms
}

TEST("ex9.1: push waits while full") {
    BlockingQueue<int> q(2);
    q.push(1);
    q.push(2);
    std::atomic<bool> pushed{false};
    std::thread p([&] {
        q.push(3);
        pushed = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK(!pushed.load());
    CHECK_EQ(q.pop(), 1);
    p.join();
    CHECK(pushed.load());
    CHECK_EQ(q.pop(), 2);
    CHECK_EQ(q.pop(), 3);
}

TEST("ex9.1: many producers and consumers, nothing lost") {
    BlockingQueue<std::uint64_t> q(8);
    constexpr std::uint64_t PerProducer = 20000;
    std::atomic<std::uint64_t> sum{0};
    std::vector<std::thread> ts;
    for (int p = 0; p < 3; ++p)
        ts.emplace_back([&] {
            for (std::uint64_t i = 1; i <= PerProducer; ++i) q.push(i);
        });
    for (int c = 0; c < 3; ++c)
        ts.emplace_back([&] {
            for (std::uint64_t i = 0; i < PerProducer; ++i) sum += q.pop();
        });
    for (auto& t : ts) t.join();
    CHECK_EQ(sum.load(), 3 * PerProducer * (PerProducer + 1) / 2);
}

TEST_MAIN()
