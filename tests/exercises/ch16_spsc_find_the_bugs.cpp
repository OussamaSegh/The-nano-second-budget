#include <spsc_find_the_bugs.hpp>

#include <thread>

#include "check.hpp"

TEST("ex16.1: capacity is respected") {
    SpscRingExercise<int> q(4);
    for (int i = 0; i < 4; ++i) CHECK(q.try_push(i));
    CHECK(!q.try_push(4));  // must be full now
    int v = -1;
    CHECK(q.try_pop(v));
    CHECK_EQ(v, 0);  // the oldest element must not have been overwritten
}

TEST("ex16.1: stress (run me under ThreadSanitizer too)") {
    struct P {
        std::uint64_t a, b;
    };
    SpscRingExercise<P> q(16);
    constexpr std::uint64_t N = 300000;
    std::thread prod([&] {
        for (std::uint64_t i = 1; i <= N; ++i)
            while (!q.try_push({i, ~i})) {}
    });
    bool ok = true;
    for (std::uint64_t i = 1; i <= N; ++i) {
        P p;
        while (!q.try_pop(p)) {}
        ok = ok && p.a == i && p.b == ~i;
    }
    prod.join();
    CHECK(ok);
}

TEST_MAIN()
