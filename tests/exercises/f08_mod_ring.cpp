#include <mod_ring.hpp>

#include <cstdint>
#include <thread>

#include "check.hpp"

template <std::size_t N>
void laps() {
    ModRing<int, N> r;
    int out = -1;
    CHECK(!r.try_pop(out));
    CHECK_EQ(out, -1);  // unchanged on failure
    for (int lap = 0; lap < 50; ++lap) {
        for (std::size_t i = 0; i < N - 1; ++i) REQUIRE(r.try_push(lap * 100 + static_cast<int>(i)));
        CHECK(!r.try_push(-2));  // full at N - 1
        for (std::size_t i = 0; i < N - 1; ++i) {
            REQUIRE(r.try_pop(out));
            CHECK_EQ(out, lap * 100 + static_cast<int>(i));
        }
        CHECK(!r.try_pop(out));
    }
}

TEST("exF8.1: capacity N-1 and wrap-around, N = 4") { laps<4>(); }
TEST("exF8.1: N need not be a power of two, N = 7") { laps<7>(); }
TEST("exF8.1: N = 2 holds one item") { laps<2>(); }

TEST("exF8.1: two threads, every value in order") {
    ModRing<std::uint64_t, 7> r;
    std::uint64_t probe;
    REQUIRE(r.try_push(0));
    REQUIRE(r.try_pop(probe));
    constexpr std::uint64_t n = 200000;
    std::thread prod([&] {
        for (std::uint64_t i = 1; i <= n; ++i)
            while (!r.try_push(i)) std::this_thread::yield();
    });
    bool ok = true;
    for (std::uint64_t i = 1; i <= n;) {
        std::uint64_t v;
        if (r.try_pop(v)) {
            ok = ok && v == i;  // a sum would hide two compensating errors
            ++i;
        }
    }
    prod.join();
    CHECK(ok);
}

TEST_MAIN()
