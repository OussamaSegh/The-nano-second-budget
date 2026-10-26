#include <percentiles.hpp>

#include "check.hpp"

TEST("ex4.1: nearest rank") {
    std::vector<std::uint64_t> v{10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    CHECK_EQ(percentile_sorted(v, 50), 50u);
    CHECK_EQ(percentile_sorted(v, 90), 90u);
    CHECK_EQ(percentile_sorted(v, 91), 100u);
    CHECK_EQ(percentile_sorted(v, 100), 100u);
    CHECK_EQ(percentile_sorted(v, 1), 10u);
    std::vector<std::uint64_t> one{7};
    CHECK_EQ(percentile_sorted(one, 99.9), 7u);
    bool threw = false;
    try {
        percentile_sorted({}, 50);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST("ex4.2: coordinated omission correction") {
    llds::Histogram h;
    record_corrected(h, 100, 1000);  // fast: one sample
    CHECK_EQ(h.count(), 1u);
    record_corrected(h, 10'000, 1000);  // stuck for 10 intervals
    // 10000 plus 9000, 8000, ..., 1000 => 10 samples
    CHECK_EQ(h.count(), 11u);
    CHECK_EQ(h.max(), 10'000u);
    CHECK(h.min() == 100u);
}

TEST_MAIN()
