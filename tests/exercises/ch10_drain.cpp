#include <drain.hpp>

#include <thread>

#include "check.hpp"

TEST("ex10.1: drains exactly what is available, end_of_batch on the last") {
    llds::Disruptor<int> ring(16);
    llds::Sequence& c = ring.add_consumer();
    for (int i = 0; i < 5; ++i) {
        auto s = ring.claim();
        ring[s] = i * 10;
        ring.publish(s);
    }
    std::vector<int> seen;
    int ends = 0;
    auto n = drain(ring, c, {}, [&](int& v, std::int64_t, bool end) {
        seen.push_back(v);
        ends += end;
    });
    REQUIRE(n == 5);
    CHECK((seen == std::vector<int>{0, 10, 20, 30, 40}));
    CHECK_EQ(ends, 1);
    CHECK_EQ(c.get(), 4);
    CHECK_EQ(drain(ring, c, {}, [](int&, std::int64_t, bool) {}), 0u);
}

TEST("ex10.1: a dependent stage never overtakes its dependency") {
    llds::Disruptor<std::uint64_t> ring(64);
    llds::Sequence& a = ring.add_consumer();
    llds::Sequence& b = ring.add_consumer({&a});
    ring.set_gating({&b});
    {
        auto s = ring.claim();
        ring[s] = 0;
        ring.publish(s);
        REQUIRE(drain(ring, a, {}, [](std::uint64_t&, std::int64_t, bool) {}) == 1);
        std::vector<const llds::Sequence*> deps{&a};
        REQUIRE(drain(ring, b, deps, [](std::uint64_t&, std::int64_t, bool) {}) == 1);
    }
    constexpr std::int64_t N = 200000;
    std::vector<std::uint64_t> stage_a(N);
    std::atomic<bool> bad{false};
    std::thread ta([&] {
        while (a.get() < N - 1)
            drain(ring, a, {}, [&](std::uint64_t& v, std::int64_t s, bool) {
                stage_a[static_cast<std::size_t>(s)] = v + 1;  // written by A, read by B
            });
    });
    std::thread tb([&] {
        std::vector<const llds::Sequence*> deps{&a};
        while (b.get() < N - 1)
            drain(ring, b, deps, [&](std::uint64_t& v, std::int64_t s, bool) {
                if (stage_a[static_cast<std::size_t>(s)] != v + 1) bad = true;
            });
    });
    for (std::int64_t i = 1; i < N; ++i) {
        auto s = ring.claim();
        ring[s] = static_cast<std::uint64_t>(s) * 3;
        ring.publish(s);
    }
    ta.join();
    tb.join();
    CHECK(!bad.load());
}

TEST_MAIN()
