#include <pair_histogram.hpp>

#include <random>
#include <vector>

#include "check.hpp"

using llds::pet::Coincidence;

namespace {
std::vector<Coincidence> random_coincidences(std::size_t n, std::uint16_t modules, unsigned seed) {
    std::mt19937 g(seed);
    std::vector<Coincidence> v(n);
    for (auto& c : v) {
        c.first.module = static_cast<std::uint16_t>(g() % modules);
        c.second.module = static_cast<std::uint16_t>(g() % modules);
    }
    return v;
}
std::vector<std::uint64_t> serial(const std::vector<Coincidence>& v, std::size_t modules) {
    std::vector<std::uint64_t> h(modules * modules, 0);
    for (const auto& c : v) ++h[pair_bin(c, modules)];
    return h;
}
}  // namespace

TEST("ex21.1: equals the serial histogram for any thread count") {
    const auto v = random_coincidences(100000, 8, 1);
    const auto expect = serial(v, 8);
    for (unsigned t : {1u, 2u, 3u, 4u, 7u, 16u}) CHECK(pair_histogram(v, 8, t) == expect);
}

TEST("ex21.1: small and empty inputs") {
    CHECK(pair_histogram({}, 4, 3) == std::vector<std::uint64_t>(16, 0));
    std::vector<Coincidence> one(1);
    one[0].first.module = 3;
    one[0].second.module = 1;
    auto h = pair_histogram(one, 4, 8);  // more threads than items
    CHECK_EQ(h[1 * 4 + 3], 1u);
    std::uint64_t total = 0;
    for (auto x : h) total += x;
    CHECK_EQ(total, 1u);
}

TEST("ex21.1: invalid arguments are rejected before threads start") {
    std::vector<Coincidence> bad(1);
    bad[0].first.module = 9;
    int threw = 0;
    for (auto f : {+[](std::span<const Coincidence> s) { (void)pair_histogram(s, 4, 2); },
                   +[](std::span<const Coincidence> s) { (void)pair_histogram(s, 4, 0); }}) {
        try {
            f(bad);
        } catch (const std::invalid_argument&) {
            ++threw;
        }
    }
    CHECK_EQ(threw, 2);
}

TEST_MAIN()
