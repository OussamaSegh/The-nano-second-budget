#include <batch_ring.hpp>

#include <thread>
#include <vector>

#include "check.hpp"

TEST("ex8.1: partial batches at the edges") {
    BatchRing<int> r(8);
    int in[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int out[10] = {};
    REQUIRE(r.push_batch(in, 10) == 8);  // only 8 fit
    CHECK_EQ(r.push_batch(in, 1), 0u);
    REQUIRE(r.pop_batch(out, 3) == 3);
    CHECK(out[0] == 0 && out[2] == 2);
    CHECK_EQ(r.push_batch(in, 10), 3u);  // wraps around
    CHECK_EQ(r.pop_batch(out, 10), 8u);
    CHECK(out[0] == 3 && out[4] == 7 && out[5] == 0 && out[7] == 2);
    CHECK_EQ(r.pop_batch(out, 10), 0u);
}

TEST("ex8.1: two threads, random batch sizes, order preserved") {
    BatchRing<std::uint64_t> r(256);
    std::uint64_t probe = 1;
    REQUIRE(r.push_batch(&probe, 1) == 1);
    REQUIRE(r.pop_batch(&probe, 1) == 1);
    constexpr std::uint64_t N = 1'000'000;
    std::thread prod([&] {
        std::uint64_t buf[37];
        std::uint64_t next = 0;
        while (next < N) {
            const std::size_t want = std::min<std::uint64_t>(1 + next % 37, N - next);
            for (std::size_t i = 0; i < want; ++i) buf[i] = next + i;
            next += r.push_batch(buf, want);
        }
    });
    std::uint64_t expect = 0;
    bool ordered = true;
    std::uint64_t buf[53];
    while (expect < N) {
        const auto k = r.pop_batch(buf, 1 + expect % 53);
        for (std::size_t i = 0; i < k; ++i) ordered = ordered && buf[i] == expect++;
    }
    prod.join();
    CHECK(ordered);
}

TEST_MAIN()
