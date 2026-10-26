#include <parallel_sum.hpp>

#include <limits>
#include <numeric>
#include <vector>

#include "check.hpp"

TEST("exF5.1: parallel sum, all splits") {
    std::vector<std::uint32_t> v(1001);
    std::iota(v.begin(), v.end(), 0u);
    const std::uint64_t expect = 1000ull * 1001 / 2;
    for (unsigned t : {1u, 2u, 3u, 7u, 16u, 1500u}) CHECK_EQ(parallel_sum(v, t), expect);
    CHECK_EQ(parallel_sum({}, 4), 0u);
}

TEST("exF5.1: the accumulator is 64 bits wide") {
    std::vector<std::uint32_t> big(10, std::numeric_limits<std::uint32_t>::max());
    CHECK_EQ(parallel_sum(big, 3), 10ull * std::numeric_limits<std::uint32_t>::max());
}

TEST("exF5.1: zero threads is rejected") {
    bool threw = false;
    try {
        (void)parallel_sum({}, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST("exF5.1: exact counter") {
    CHECK_EQ(exact_count(0, 100), 0u);
    CHECK_EQ(exact_count(4, 100000), 400000u);
}

TEST_MAIN()
