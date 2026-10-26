// Chapter 14: ordered concurrent structures.
//
// A mixed workload (80% lookups, 10% inserts, 10% erases) over 100,000 keys,
// comparing a mutex-protected std::map, a shared_mutex-protected std::map
// and the lazy skip list. Then a range scan: "all price levels in a band",
// the operation that makes an ordered structure worth having.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/ordered/lazy_skiplist.hpp>

#include <cstdio>
#include <map>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "example_util.hpp"

constexpr int kRange = 100'000;

struct MutexMap {
    std::mutex m;
    std::map<int, int> map;
    bool contains(int k) { std::lock_guard g(m); return map.count(k) != 0; }
    bool insert(int k, int v) { std::lock_guard g(m); return map.emplace(k, v).second; }
    bool erase(int k) { std::lock_guard g(m); return map.erase(k) != 0; }
};

struct RwMap {
    std::shared_mutex m;
    std::map<int, int> map;
    bool contains(int k) { std::shared_lock g(m); return map.count(k) != 0; }
    bool insert(int k, int v) { std::unique_lock g(m); return map.emplace(k, v).second; }
    bool erase(int k) { std::unique_lock g(m); return map.erase(k) != 0; }
};

struct Skip {
    llds::LazySkipList<int, int> s;
    bool contains(int k) { return s.contains(k); }
    bool insert(int k, int v) { return s.insert(k, v); }
    bool erase(int k) { return s.erase(k); }
};

template <class M>
static void run(const char* name, unsigned threads, std::uint64_t ops) {
    M m;
    for (int k = 0; k < kRange; k += 2) m.insert(k, k);
    std::vector<std::thread> ts;
    const auto t0 = llds::now_ns();
    for (unsigned t = 0; t < threads; ++t)
        ts.emplace_back([&, t] {
            llds::pin_this_thread(static_cast<int>(t % llds::hardware_threads()));
            std::mt19937 rng(t + 1);
            std::uint64_t hits = 0;
            for (std::uint64_t i = 0; i < ops; ++i) {
                const int k = static_cast<int>(rng() % kRange);
                const unsigned op = rng() % 10;
                if (op < 8) hits += m.contains(k);
                else if (op == 8) m.insert(k, k);
                else m.erase(k);
            }
            llds::do_not_optimize(hits);
        });
    for (auto& t : ts) t.join();
    const double secs = static_cast<double>(llds::now_ns() - t0) / 1e9;
    std::printf("%-14s T=%u  %7.2f M ops/s\n", name, threads, static_cast<double>(ops * threads) / secs / 1e6);
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const auto ops = args.n(1'000'000);
    const unsigned hw = std::min(8u, llds::hardware_threads());
    for (unsigned t : {1u, hw}) {
        run<MutexMap>("mutex map", t, ops);
        run<RwMap>("rw map", t, ops);
        run<Skip>("lazy skiplist", t, ops);
        std::printf("\n");
    }

    // Range scan while writers churn: readers never block writers.
    llds::LazySkipList<int, int> levels;
    for (int px = 10'000; px < 10'000 + 2'000; ++px) levels.insert(px, 100);
    std::atomic<bool> stop{false};
    std::thread churn([&] {
        std::mt19937 rng(7);
        while (!stop.load()) {
            int px = 10'000 + static_cast<int>(rng() % 2'000);
            if (!levels.erase(px)) levels.insert(px, 100);
        }
    });
    auto h = llds::measure_each([&] {
        long qty = 0;
        levels.for_range(10'500, 10'550, [&](int, int q) { qty += q; });
        llds::do_not_optimize(qty);
    }, {args.n(200'000), 1000});
    stop = true;
    churn.join();
    std::printf("50-level range scan under churn: %s\n", h.summary().c_str());
    return 0;
}
