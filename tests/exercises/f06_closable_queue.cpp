#include <closable_queue.hpp>

#include <atomic>
#include <future>
#include <thread>
#include <vector>

#include "check.hpp"

TEST("exF6.1: FIFO, close, drain") {
    ClosableQueue<int> q(2);
    REQUIRE(q.push(1));
    REQUIRE(q.push(2));
    q.close();
    q.close();  // idempotent
    CHECK(!q.push(3));
    CHECK(q.pop() == std::optional<int>(1));
    CHECK(q.pop() == std::optional<int>(2));
    CHECK(!q.pop().has_value());
}

TEST("exF6.1: zero capacity is rejected") {
    bool threw = false;
    try {
        ClosableQueue<int> q(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST("exF6.1: close wakes a blocked consumer and a blocked producer") {
    {
        ClosableQueue<int> q(1);
        REQUIRE(q.push(0));
        REQUIRE(q.pop().has_value());  // starter code stops here
        auto reader = std::async(std::launch::async, [&] { return q.pop(); });
        q.close();
        CHECK(!reader.get().has_value());
    }
    {
        ClosableQueue<int> q(1);
        REQUIRE(q.push(1));
        auto writer = std::async(std::launch::async, [&] { return q.push(2); });
        q.close();
        const bool pushed = writer.get();  // may win the race before close, or see it
        CHECK(q.pop() == std::optional<int>(1));
        if (pushed) CHECK(q.pop() == std::optional<int>(2));
        CHECK(!q.pop().has_value());
    }
}

TEST("exF6.1: four producers, four consumers, nothing lost or duplicated") {
    ClosableQueue<std::uint64_t> q(8);
    REQUIRE(q.push(0));
    REQUIRE(q.pop().has_value());
    constexpr std::uint64_t per = 20000;
    std::vector<std::thread> prod, cons;
    std::atomic<std::uint64_t> sum{0}, count{0};
    std::atomic<bool> fifo{true};
    for (std::uint64_t p = 0; p < 4; ++p)
        prod.emplace_back([&, p] {
            for (std::uint64_t i = 0; i < per; ++i) q.push(p << 32 | i);
        });
    for (int c = 0; c < 4; ++c)
        cons.emplace_back([&] {
            std::uint64_t last[4] = {0, 0, 0, 0};
            bool first[4] = {true, true, true, true};
            while (auto v = q.pop()) {
                const auto p = *v >> 32, i = *v & 0xffffffff;
                if (!first[p] && i <= last[p]) fifo = false;  // per-producer order, per consumer
                first[p] = false;
                last[p] = i;
                sum += i;
                ++count;
            }
        });
    for (auto& t : prod) t.join();
    q.close();
    for (auto& t : cons) t.join();
    CHECK_EQ(count.load(), 4 * per);
    CHECK_EQ(sum.load(), 4 * (per * (per - 1) / 2));
    CHECK(fifo.load());
}

TEST_MAIN()
