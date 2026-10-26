#include <llds/pet/calibration.hpp>
#include <llds/pet/coincidence.hpp>
#include <llds/pet/hit.hpp>
#include <llds/pet/merge.hpp>

#include <algorithm>
#include <atomic>
#include <limits>
#include <random>
#include <thread>
#include <vector>

#include "check.hpp"

using namespace llds::pet;

namespace {

// Sorted synthetic hits: small gaps, so windows are often dense, and few
// modules, so same-module rejections happen too.
std::vector<Hit> random_hits(std::mt19937_64& g, std::size_t n, unsigned max_gap, unsigned modules) {
    std::vector<Hit> v;
    std::uint64_t t = 1000;
    for (std::size_t i = 0; i < n; ++i) {
        t += g() % (max_gap + 1);
        v.push_back({t, static_cast<std::uint32_t>(i), static_cast<std::uint16_t>(g() % modules), 500});
    }
    return v;
}

}  // namespace

TEST("pet: decode round trip and boundaries") {
    Hit h{std::numeric_limits<std::uint64_t>::max(), 0xfffffffeu, 63, 0xffff};
    auto b = encode_hit(h);
    Hit out;
    CHECK(decode_hit(b, 64, out) == DecodeStatus::ok);
    CHECK(out == h);
    // little-endian: the first byte is the least significant
    Hit small{0x0102, 0, 0, 0};
    auto sb = encode_hit(small);
    CHECK(std::to_integer<int>(sb[0]) == 0x02 && std::to_integer<int>(sb[1]) == 0x01);

    Hit untouched{7, 7, 7, 7};
    CHECK(decode_hit(std::span(b).first(15), 64, untouched) == DecodeStatus::bad_length);
    CHECK(decode_hit(b, 63, untouched) == DecodeStatus::bad_module);  // module 63 is not < 63
    CHECK(untouched == (Hit{7, 7, 7, 7}));                              // not written on failure
}

TEST("pet: tick distance never wraps") {
    CHECK_EQ(tick_distance(0, std::numeric_limits<std::uint64_t>::max()), std::numeric_limits<std::uint64_t>::max());
    CHECK_EQ(tick_distance(10, 3), 7u);
    CHECK_EQ(tick_distance(3, 10), 7u);
    CHECK(!eligible({0, 0, 1, 0}, {std::numeric_limits<std::uint64_t>::max(), 0, 2, 0}, 10));
}

TEST("pet: worked example from Chapter 20") {
    // (tick, module) = (0,A) (2,B) (5,A) (12,C), window 5
    std::vector<Hit> v{{0, 0, 0, 0}, {2, 1, 1, 0}, {5, 2, 0, 0}, {12, 3, 2, 0}};
    auto p = window_pairs(v, 5);
    REQUIRE(p.size() == 2u);
    CHECK(p[0].first.seq == 0 && p[0].second.seq == 1);
    CHECK(p[1].first.seq == 1 && p[1].second.seq == 2);
    CHECK(p == pairs_oracle(v, 5));
}

TEST("pet: window_pairs equals the oracle") {
    std::mt19937_64 g(42);
    for (int trial = 0; trial < 500; ++trial) {
        auto v = random_hits(g, trial % 120, 6, 1 + trial % 5);
        const std::uint64_t w = g() % 20;
        CHECK(window_pairs(v, w) == pairs_oracle(v, w));
    }
    std::vector<Hit> unsorted{{5, 0, 0, 0}, {4, 1, 1, 0}};
    bool threw = false;
    try {
        (void)window_pairs(unsorted, 3);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST("pet: streaming output equals batch for every split") {
    std::mt19937_64 g(7);
    for (int trial = 0; trial < 60; ++trial) {
        auto v = random_hits(g, 40, 4, 3);
        const std::uint64_t w = 1 + g() % 10;
        const auto expect = window_pairs(v, w);
        // Every two-chunk split, including empty chunks at either end.
        for (std::size_t split = 0; split <= v.size(); ++split) {
            StreamingSorter s(w, 64);
            std::vector<Coincidence> got;
            auto sink = [&](const Coincidence& c) { got.push_back(c); };
            for (std::size_t i = 0; i < split; ++i) s.push(v[i], sink);
            for (std::size_t i = split; i < v.size(); ++i) s.push(v[i], sink);
            CHECK(got == expect);
            CHECK_EQ(s.counters().forced_evictions, 0u);
        }
    }
}

TEST("pet: streaming dense equal-tick run and overflow accounting") {
    std::vector<Hit> v;
    for (std::uint32_t i = 0; i < 10; ++i) v.push_back({100, i, static_cast<std::uint16_t>(i % 2), 0});
    // 10 hits at one tick, alternating modules: 5*5 = 25 cross-module pairs.
    StreamingSorter big(0, 16);
    std::size_t n = 0;
    for (auto& h : v) big.push(h, [&](const Coincidence&) { ++n; });
    CHECK_EQ(n, 25u);
    CHECK_EQ(big.counters().retained_peak, 10u);

    StreamingSorter small(0, 4);  // too small for the run: must say so
    std::size_t m = 0;
    for (auto& h : v) small.push(h, [&](const Coincidence&) { ++m; });
    CHECK(m < 25u);
    CHECK_EQ(small.counters().forced_evictions, 6u);
}

TEST("pet: streaming rejects out-of-order hits") {
    StreamingSorter s(10, 8);
    auto none = [](const Coincidence&) {};
    CHECK(s.push({100, 0, 0, 0}, none) == StreamingSorter::Status::ok);
    CHECK(s.push({99, 1, 1, 0}, none) == StreamingSorter::Status::out_of_order);
    CHECK_EQ(s.counters().out_of_order, 1u);
    CHECK_EQ(s.counters().hits_in, 1u);
}

TEST("pet: watermark merge equals a stable sort") {
    std::mt19937_64 g(11);
    for (int trial = 0; trial < 200; ++trial) {
        const std::size_t k = 1 + trial % 4;
        std::vector<std::vector<Hit>> per(k);
        std::vector<Hit> all;
        for (std::size_t s = 0; s < k; ++s) {
            per[s] = random_hits(g, g() % 30, 5, 1);
            for (auto& h : per[s]) {
                h.module = static_cast<std::uint16_t>(s);
                all.push_back(h);
            }
        }
        std::stable_sort(all.begin(), all.end(), [](const Hit& a, const Hit& b) {
            return a.tick != b.tick ? a.tick < b.tick : a.module < b.module;
        });

        WatermarkMerger m(k, 64);
        std::vector<Hit> out;
        auto sink = [&](const Hit& h) { out.push_back(h); };
        std::vector<std::size_t> next(k, 0);
        // Interleave sources randomly; after each hit the source promises
        // its next hit's tick (or closes when it has none left).
        for (std::size_t left = all.size(); left;) {
            const std::size_t s = g() % k;
            if (next[s] == per[s].size()) continue;
            CHECK(m.add(s, per[s][next[s]]));
            ++next[s];
            --left;
            if (next[s] < per[s].size()) m.promise(s, per[s][next[s]].tick);
            else m.close(s);
            m.drain(sink);
        }
        for (std::size_t s = 0; s < k; ++s) m.close(s);
        m.drain(sink);
        CHECK(out == all);
        CHECK(m.all_closed_and_empty());
    }
}

TEST("pet: a silent source holds the watermark") {
    WatermarkMerger m(2, 8);
    CHECK(m.add(0, {10, 0, 0, 0}));
    m.promise(0, 20);
    std::size_t n = m.drain([](const Hit&) {});
    CHECK_EQ(n, 0u);  // source 1 has promised nothing yet
    m.promise(1, 15);
    n = m.drain([](const Hit&) {});
    CHECK_EQ(n, 1u);
    CHECK(!m.add(0, {12, 1, 0, 0}));  // breaks source 0's promise of 20
    m.close(1);                       // e.g. excluded after a timeout
    CHECK_EQ(m.watermark(), 20u);
}

TEST("pet: calibration validates and publishes snapshots") {
    bool threw = false;
    try {
        CalibrationTable bad(1, {{0.0, 0.0}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    CalibrationTable t(3, {{2.0, -1.0}, {1.0, 0.0}});
    CHECK(t.energy_kev({0, 0, 0, 100}) == 199.0);
    // volatile: with a constant index GCC 13 warns (-Warray-bounds) about the
    // branch the bounds check never takes.
    volatile std::uint16_t far = 5;
    CHECK(std::isnan(t.energy_kev({0, 0, far, 100})));
    EnergyWindow win{425, 650};
    CHECK(win.contains(511) && win.contains(425) && !win.contains(651) && !win.contains(std::nan("")));

    CalibrationHolder holder(std::make_shared<const CalibrationTable>(1, std::vector<ModuleCalibration>{{1, 0}}));
    std::atomic<bool> stop{false};
    std::thread writer([&] {
        for (std::uint32_t v = 2; v <= 2000; ++v)
            holder.publish(std::make_shared<const CalibrationTable>(v, std::vector<ModuleCalibration>{{1.0 * v, 0}}));
        stop.store(true);
    });
    std::uint32_t last = 0;
    bool monotonic = true, consistent = true;
    while (!stop.load()) {
        auto snap = holder.snapshot();
        if (snap->version() < last) monotonic = false;
        if (snap->energy_kev({0, 0, 0, 1}) != 1.0 * snap->version() && snap->version() > 1) consistent = false;
        last = snap->version();
    }
    writer.join();
    CHECK(monotonic);
    CHECK(consistent);
    CHECK_EQ(holder.snapshot()->version(), 2000u);
}

TEST_MAIN()
