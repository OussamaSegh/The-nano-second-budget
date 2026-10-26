#include <delayed_window.hpp>

#include <limits>
#include <random>
#include <vector>

#include "check.hpp"

using llds::pet::Hit;

namespace {
std::uint64_t brute(const std::vector<Hit>& v, std::uint64_t w, std::uint64_t d) {
    std::uint64_t n = 0;
    for (std::size_t i = 0; i < v.size(); ++i)
        for (std::size_t j = 0; j < v.size(); ++j) {
            if (v[j].tick < v[i].tick || v[i].module == v[j].module) continue;
            const auto dt = v[j].tick - v[i].tick;
            if (dt >= d && dt <= d + w) ++n;
        }
    return n;
}
}  // namespace

TEST("ex20.2: small example") {
    // delay 100, window 5: pairs with dt in [100, 105]
    std::vector<Hit> v{{0, 0, 0, 0}, {3, 1, 1, 0}, {98, 2, 0, 0}, {104, 3, 1, 0}, {200, 4, 2, 0}, {205, 5, 3, 0}};
    // {tick, seq, module, energy}
    // (0 -> 104): dt 104, modules 0,1: yes.  (3 -> 98): dt 95: no (before the delayed window).
    // (98 -> 200): dt 102, modules 0,2: yes. (104 -> 205): dt 101, modules 1,3: yes.
    // (104 -> 200): dt 96: no. (98 -> 205): dt 107: no.
    CHECK_EQ(delayed_window_count(v, 5, 100), 3u);
}

TEST("ex20.2: equals brute force") {
    std::mt19937_64 g(3);
    for (int trial = 0; trial < 300; ++trial) {
        std::vector<Hit> v;
        std::uint64_t t = g() % 50;
        for (int i = 0; i < trial % 90; ++i) {
            t += g() % 30;
            v.push_back({t, static_cast<std::uint32_t>(i), static_cast<std::uint16_t>(g() % 4), 0});
        }
        const std::uint64_t w = g() % 10, d = w + 1 + g() % 200;
        CHECK_EQ(delayed_window_count(v, w, d), brute(v, w, d));
    }
}

TEST("ex20.2: no overflow near the top of the tick range") {
    const auto top = std::numeric_limits<std::uint64_t>::max();
    std::vector<Hit> v{{top - 200, 0, 0, 0}, {top - 100, 1, 1, 0}, {top, 2, 2, 0}};
    CHECK_EQ(delayed_window_count(v, 5, 100), 2u);
}

TEST("ex20.2: preconditions") {
    std::vector<Hit> v{{5, 0, 0, 0}, {4, 1, 1, 0}};
    int threw = 0;
    try {
        (void)delayed_window_count(v, 5, 100);
    } catch (const std::invalid_argument&) {
        ++threw;
    }
    try {
        (void)delayed_window_count({}, 5, 5);
    } catch (const std::invalid_argument&) {
        ++threw;
    }
    CHECK_EQ(threw, 2);
}

TEST_MAIN()
