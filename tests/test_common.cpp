#include <llds/common/cacheline.hpp>
#include <llds/common/histogram.hpp>
#include <llds/common/timer.hpp>

#include "check.hpp"

using namespace llds;

TEST("histogram: small values are exact") {
    Histogram h;
    for (std::uint64_t v = 0; v < 64; ++v) h.record(v);
    CHECK_EQ(h.count(), 64u);
    CHECK_EQ(h.min(), 0u);
    CHECK_EQ(h.max(), 63u);
    CHECK_EQ(h.percentile(50), 31u);
    CHECK_EQ(h.percentile(100), 63u);
}

TEST("histogram: relative error under 2 percent") {
    Histogram h;
    for (std::uint64_t v = 1; v <= 1'000'000; ++v) h.record(v);
    const double p99 = static_cast<double>(h.percentile(99));
    CHECK(p99 >= 990'000.0 && p99 <= 990'000.0 * 1.02);
    const double p50 = static_cast<double>(h.percentile(50));
    CHECK(p50 >= 500'000.0 && p50 <= 500'000.0 * 1.02);
}

TEST("histogram: bucket index is monotonic and in range") {
    int prev = -1;
    for (std::uint64_t v = 0; v < (1u << 20); v += 7) {
        const int i = Histogram::index_of(v);
        CHECK(i >= prev);
        CHECK(i < Histogram::kBuckets);
        CHECK(Histogram::upper_edge(i) >= v);
        prev = i;
    }
    CHECK(Histogram::index_of(~0ull) < Histogram::kBuckets);
}

TEST("histogram: merge") {
    Histogram a, b;
    a.record(10);
    b.record(1000);
    a.merge(b);
    CHECK_EQ(a.count(), 2u);
    CHECK_EQ(a.max(), 1000u);
    CHECK_EQ(a.min(), 10u);
}

TEST("padded: alignment") {
    Padded<int> p[2];
    auto d = reinterpret_cast<char*>(&p[1]) - reinterpret_cast<char*>(&p[0]);
    CHECK_EQ(static_cast<std::size_t>(d), kFalseSharingRange);
    CHECK(next_pow2(1000) == 1024);
    CHECK(is_pow2(64) && !is_pow2(65));
}

TEST("next_pow2: boundaries and overflow") {
    static_assert(next_pow2(1) == 1 && next_pow2(3) == 4 && next_pow2(4) == 4);
    constexpr std::size_t top = std::size_t{1} << 63;
    CHECK(next_pow2(top) == top);
    bool threw = false;
    try {
        (void)next_pow2(top + 1);  // used to loop forever
    } catch (const std::length_error&) {
        threw = true;
    }
    CHECK(threw);
}

TEST("timer: monotonic and calibrated") {
    auto a = now_ns();
    auto b = now_ns();
    CHECK(b >= a);
    TscClock c;
    CHECK(c.ns_per_cycle() > 0.0);
}

TEST_MAIN()
