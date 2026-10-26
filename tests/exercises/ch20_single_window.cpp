#include <single_window.hpp>

#include <algorithm>
#include <random>

#include "check.hpp"

using llds::pet::Hit;

namespace {
Hit h(std::uint64_t t, std::uint16_t m, std::uint32_t id) { return {t, id, m, 0}; }
}  // namespace

TEST("ex20.1: singles, pairs, same module") {
    // groups: {0} {100,103} {200,201 same module} {300}
    std::vector<Hit> v{h(0, 0, 0), h(100, 0, 1), h(103, 4, 2), h(200, 2, 3), h(201, 2, 4), h(300, 1, 5)};
    auto r = single_window(v, 5, Multiples::kill_all);
    REQUIRE(r.pairs.size() == 1u);
    CHECK(r.pairs[0].first.seq == 1 && r.pairs[0].second.seq == 2);
    CHECK_EQ(r.singles, 2u);
    CHECK_EQ(r.same_module, 1u);
    CHECK_EQ(r.multiples, 0u);
}

TEST("ex20.1: the window does not slide") {
    // 0 opens [0,5]; 4 joins; 8 is within 5 of 4 but not of 0: new group.
    std::vector<Hit> v{h(0, 0, 0), h(4, 1, 1), h(8, 2, 2), h(9, 3, 3)};
    auto r = single_window(v, 5, Multiples::kill_all);
    REQUIRE(r.pairs.size() == 2u);
    CHECK(r.pairs[0].first.seq == 0 && r.pairs[0].second.seq == 1);
    CHECK(r.pairs[1].first.seq == 2 && r.pairs[1].second.seq == 3);
}

TEST("ex20.1: multiples, both policies") {
    std::vector<Hit> v{h(10, 0, 0), h(11, 4, 1), h(12, 4, 2), h(100, 1, 3), h(101, 5, 4)};
    auto kill = single_window(v, 5, Multiples::kill_all);
    CHECK_EQ(kill.multiples, 1u);
    CHECK_EQ(kill.pairs.size(), 1u);  // only the clean pair at 100/101
    auto take = single_window(v, 5, Multiples::take_all_pairs);
    CHECK_EQ(take.multiples, 1u);
    REQUIRE(take.pairs.size() == 3u);  // (0,1) (0,2), then the clean pair; (1,2) is same-module
    CHECK(take.pairs[0].first.seq == 0 && take.pairs[0].second.seq == 1);
    CHECK(take.pairs[1].first.seq == 0 && take.pairs[1].second.seq == 2);
}

TEST("ex20.1: isolated groups agree with the multiple-window sorter") {
    std::mt19937_64 g(5);
    for (int trial = 0; trial < 100; ++trial) {
        std::vector<Hit> v;
        std::uint64_t t = 0;
        std::uint32_t id = 0;
        for (int grp = 0; grp < 30; ++grp) {
            t += 100;  // groups far apart (> 2 windows)
            const int n = 1 + static_cast<int>(g() % 4);
            for (int k = 0; k < n; ++k) v.push_back(h(t + g() % 6, static_cast<std::uint16_t>(g() % 4), id++));
        }
        std::sort(v.begin(), v.end(), [](const Hit& a, const Hit& b) { return a.tick < b.tick; });
        auto r = single_window(v, 5, Multiples::take_all_pairs);
        CHECK(r.pairs == llds::pet::window_pairs(v, 5));
    }
}

TEST("ex20.1: unsorted input is rejected") {
    std::vector<Hit> v{h(5, 0, 0), h(4, 1, 1)};
    bool threw = false;
    try {
        (void)single_window(v, 5, Multiples::kill_all);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST_MAIN()
