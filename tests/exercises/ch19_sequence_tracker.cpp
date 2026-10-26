#include <sequence_tracker.hpp>

#include "check.hpp"

TEST("ex19.1: in order, gaps, and the first record") {
    SequenceTracker t(2);
    CHECK_EQ(t.observe(0, 0), 0u);
    CHECK_EQ(t.observe(0, 1), 0u);
    CHECK_EQ(t.observe(0, 5), 3u);  // 2, 3, 4 lost
    CHECK_EQ(t.observe(0, 6), 0u);
    CHECK_EQ(t.missing(0), 3u);
    CHECK_EQ(t.observe(1, 2), 0u);  // first record of source 1 synchronises
    CHECK_EQ(t.observe(1, 4), 1u);
    CHECK_EQ(t.missing(1), 1u);
}

TEST("ex19.1: late and duplicate records") {
    SequenceTracker t(1);
    t.observe(0, 0);
    t.observe(0, 4);
    CHECK_EQ(t.observe(0, 2), 0u);  // late
    CHECK_EQ(t.observe(0, 4), 0u);  // duplicate
    CHECK_EQ(t.late(0), 2u);
    CHECK_EQ(t.observe(0, 5), 0u);  // still expecting 5
    CHECK_EQ(t.missing(0), 3u);
}

TEST("ex19.1: across the 2^32 wrap") {
    SequenceTracker t(1);
    CHECK_EQ(t.observe(0, 0xffff'fff0u), 0u);  // synchronises near the top of the range
    const auto before = t.missing(0);
    CHECK_EQ(before, 0u);
    CHECK_EQ(t.observe(0, 0xffff'ffffu), 14u);
    CHECK_EQ(t.observe(0, 0), 0u);  // wrapped in order
    CHECK_EQ(t.observe(0, 3), 2u);
    CHECK_EQ(t.observe(0, 0xffff'fffeu), 0u);  // far behind: late, not 4 billion lost
    CHECK_EQ(t.late(0), 1u);
    CHECK_EQ(t.missing(0), before + 16);
}

TEST("ex19.1: loss at the very end") {
    SequenceTracker t(1);
    for (std::uint32_t s = 0; s < 10; ++s) t.observe(0, s);
    CHECK_EQ(t.finish(0, 13), 3u);
    CHECK_EQ(t.missing(0), 3u);
    SequenceTracker u(1);
    u.observe(0, 0);
    CHECK_EQ(u.finish(0, 1), 0u);
    SequenceTracker silent(1);
    CHECK_EQ(silent.finish(0, 7), 7u);  // never heard from: all lost
}

TEST_MAIN()
