// Chapter 15: data-oriented design.
//
// Part 1: array-of-structs vs struct-of-arrays for a scan that reads one
//         field of a 64-byte record.
// Part 2: a branchy vs branchless filter.
// Part 3: the node-based MapBook vs the flat LadderBook order book on the
//         same stream of updates.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/ordered/order_book.hpp>

#include <cstdio>
#include <random>
#include <vector>

#include "example_util.hpp"

struct PatientAoS {  // one 64-byte record per bed
    std::uint32_t bed_id;
    std::int32_t heart_rate;
    std::int32_t spo2;
    std::int32_t resp_rate;
    std::int32_t systolic;
    std::int32_t diastolic;
    std::int32_t temperature_mc;
    std::int32_t flags;
    std::uint64_t last_update_ns;
    char name[24];
};
static_assert(sizeof(PatientAoS) == 64);

struct PatientsSoA {
    std::vector<std::uint32_t> bed_id;
    std::vector<std::int32_t> heart_rate, spo2, resp_rate, systolic, diastolic, temperature_mc, flags;
    std::vector<std::uint64_t> last_update_ns;
};

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    llds::pin_this_thread(args.cpu_a);
    const std::size_t n = args.smoke ? 100'000 : 4'000'000;
    std::mt19937 rng(3);

    std::vector<PatientAoS> aos(n);
    PatientsSoA soa;
    soa.heart_rate.resize(n);
    soa.spo2.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        aos[i].heart_rate = 40 + static_cast<int>(rng() % 120);
        aos[i].spo2 = 850 + static_cast<int>(rng() % 150);
        soa.heart_rate[i] = aos[i].heart_rate;
        soa.spo2[i] = aos[i].spo2;
    }

    // ---- Part 1: count beds with tachycardia (HR > 120)
    const int reps = args.smoke ? 2 : 20;
    long c1 = 0, c2 = 0;
    auto t0 = llds::now_ns();
    for (int r = 0; r < reps; ++r)
        for (std::size_t i = 0; i < n; ++i) c1 += aos[i].heart_rate > 120;
    auto t1 = llds::now_ns();
    for (int r = 0; r < reps; ++r)
        for (std::size_t i = 0; i < n; ++i) c2 += soa.heart_rate[i] > 120;
    auto t2 = llds::now_ns();
    std::printf("scan one field of %zu records: AoS %.2f ns/rec, SoA %.2f ns/rec (counts %ld %ld)\n", n,
                static_cast<double>(t1 - t0) / (static_cast<double>(n) * reps),
                static_cast<double>(t2 - t1) / (static_cast<double>(n) * reps), c1 / reps, c2 / reps);

    // ---- Part 2: sum HR of beds with SpO2 below 90%, branchy vs branchless.
    // With random data the branch is unpredictable about 1/3 of the time.
    long s1 = 0, s2 = 0;
    t0 = llds::now_ns();
    for (int r = 0; r < reps; ++r)
        for (std::size_t i = 0; i < n; ++i) {
            if (soa.spo2[i] < 900) s1 += soa.heart_rate[i];
            llds::clobber_memory();  // stop the compiler from if-converting
        }
    t1 = llds::now_ns();
    for (int r = 0; r < reps; ++r)
        for (std::size_t i = 0; i < n; ++i) {
            const long mask = -static_cast<long>(soa.spo2[i] < 900);  // all ones or zero
            s2 += soa.heart_rate[i] & mask;
        }
    t2 = llds::now_ns();
    std::printf("filtered sum: branchy %.2f ns/rec, branchless %.2f ns/rec (%s)\n",
                static_cast<double>(t1 - t0) / (static_cast<double>(n) * reps),
                static_cast<double>(t2 - t1) / (static_cast<double>(n) * reps), s1 == s2 ? "same result" : "MISMATCH");

    // ---- Part 3: order books
    struct Update {
        llds::Side side;
        std::int64_t px, qty;
    };
    std::vector<Update> stream;
    const std::size_t updates = args.smoke ? 100'000 : 5'000'000;
    stream.reserve(updates);
    {
        llds::MapBook shadow;  // to generate only valid removals
        std::normal_distribution<double> off(0, 30);
        for (std::size_t i = 0; i < updates; ++i) {
            const bool bid = rng() & 1;
            const auto side = bid ? llds::Side::Bid : llds::Side::Ask;
            const auto d = static_cast<std::int64_t>(std::abs(off(rng)));
            const std::int64_t px = bid ? 100'000 - 1 - d : 100'000 + d;
            std::int64_t qty = static_cast<std::int64_t>(rng() % 200) - 90;
            if (qty < 0) qty = -std::min(-qty, shadow.qty_at(side, px));
            shadow.apply(side, px, qty);
            stream.push_back({side, px, qty});
        }
    }
    llds::MapBook mb;
    llds::LadderBook lb(100'000 - 4096, 8192);
    t0 = llds::now_ns();
    for (auto& u : stream) mb.apply(u.side, u.px, u.qty);
    t1 = llds::now_ns();
    for (auto& u : stream) lb.apply(u.side, u.px, u.qty);
    t2 = llds::now_ns();
    const auto a = mb.top(), b = lb.top();
    std::printf("order book updates: MapBook %.1f ns/update, LadderBook %.1f ns/update (tops %s)\n",
                static_cast<double>(t1 - t0) / static_cast<double>(updates),
                static_cast<double>(t2 - t1) / static_cast<double>(updates),
                (a.bid_px == b.bid_px && a.ask_px == b.ask_px && a.bid_qty == b.bid_qty) ? "agree" : "DISAGREE");
    return 0;
}
