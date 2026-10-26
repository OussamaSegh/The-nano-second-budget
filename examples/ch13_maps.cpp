// Chapter 13: concurrent hash maps under a read-heavy workload.
//
// T threads perform lookups on 10,000 keys; a fraction of operations are
// updates. We compare:
//   mutex+unordered_map, shared_mutex+unordered_map, StripedHashMap,
//   ConcurrentIntMap (lock-free open addressing) and RcuMap (copy-on-write)
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/map/concurrent_int_map.hpp>
#include <llds/map/rcu_map.hpp>
#include <llds/map/striped_hash_map.hpp>

#include <cstdio>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "example_util.hpp"

constexpr std::uint64_t kKeys = 10'000;

struct MutexMap {
    std::mutex m;
    std::unordered_map<std::uint64_t, std::uint64_t> map;
    bool get(std::uint64_t k, std::uint64_t& v) {
        std::lock_guard g(m);
        auto it = map.find(k);
        if (it == map.end()) return false;
        v = it->second;
        return true;
    }
    void put(std::uint64_t k, std::uint64_t v) { std::lock_guard g(m); map[k] = v; }
};

struct SharedMutexMap {
    std::shared_mutex m;
    std::unordered_map<std::uint64_t, std::uint64_t> map;
    bool get(std::uint64_t k, std::uint64_t& v) {
        std::shared_lock g(m);
        auto it = map.find(k);
        if (it == map.end()) return false;
        v = it->second;
        return true;
    }
    void put(std::uint64_t k, std::uint64_t v) { std::unique_lock g(m); map[k] = v; }
};

struct Striped {
    llds::StripedHashMap<std::uint64_t, std::uint64_t> map;
    bool get(std::uint64_t k, std::uint64_t& v) {
        auto r = map.find(k);
        if (!r) return false;
        v = *r;
        return true;
    }
    void put(std::uint64_t k, std::uint64_t v) { map.insert_or_assign(k, v); }
};

struct LockFree {
    llds::ConcurrentIntMap map{4 * kKeys};
    bool get(std::uint64_t k, std::uint64_t& v) {
        auto r = map.find(k + 1);  // key 0 is reserved
        if (!r) return false;
        v = *r;
        return true;
    }
    void put(std::uint64_t k, std::uint64_t v) { map.put(k + 1, v); }
};

struct Rcu {
    llds::RcuMap<std::uint64_t, std::uint64_t> map;
    bool get(std::uint64_t k, std::uint64_t& v) {
        auto r = map.find(k);
        if (!r) return false;
        v = *r;
        return true;
    }
    void put(std::uint64_t k, std::uint64_t v) { map.insert_or_assign(k, v); }
};

template <class Map>
static void run(const char* name, unsigned threads, double write_fraction, std::uint64_t ops) {
    Map m;
    for (std::uint64_t k = 0; k < kKeys; ++k) m.put(k, k);
    std::vector<std::thread> ts;
    const auto t0 = llds::now_ns();
    for (unsigned t = 0; t < threads; ++t)
        ts.emplace_back([&, t] {
            llds::pin_this_thread(static_cast<int>(t % llds::hardware_threads()));
            std::mt19937_64 rng(t);
            const auto write_threshold = static_cast<std::uint64_t>(write_fraction * 1'000'000);
            std::uint64_t sink = 0, v;
            for (std::uint64_t i = 0; i < ops; ++i) {
                const auto r = rng();
                const auto k = r % kKeys;
                if ((r >> 40) % 1'000'000 < write_threshold) m.put(k, i);
                else if (m.get(k, v)) sink += v;
            }
            llds::do_not_optimize(sink);
        });
    for (auto& t : ts) t.join();
    const double secs = static_cast<double>(llds::now_ns() - t0) / 1e9;
    std::printf("%-16s T=%u writes=%5.2f%%  %8.2f M ops/s\n", name, threads, write_fraction * 100,
                static_cast<double>(ops * threads) / secs / 1e6);
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const auto ops = args.n(2'000'000);
    const unsigned hw = std::min(8u, llds::hardware_threads());
    for (double wf : {0.0, 0.0001, 0.05}) {
        for (unsigned t : {1u, hw}) {
            run<MutexMap>("mutex", t, wf, ops);
            run<SharedMutexMap>("shared_mutex", t, wf, ops);
            run<Striped>("striped", t, wf, ops);
            run<LockFree>("lock-free int", t, wf, ops);
            // RCU copies 10k entries per write: only sensible when writes are rare.
            if (wf < 0.001) run<Rcu>("rcu", t, wf, ops);
            std::printf("\n");
        }
    }
    return 0;
}
