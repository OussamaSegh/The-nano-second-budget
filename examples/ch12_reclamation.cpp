// Chapter 12: the read-side cost of safe memory reclamation.
//
// R readers repeatedly read a shared configuration object through a pointer
// while one writer replaces it every 10 us. We compare:
//   mutex                std::mutex around a raw pointer
//   atomic<shared_ptr>   C++20 std::atomic<std::shared_ptr<T>>
//   hazard pointers      llds::HazardDomain
//   epoch                llds::EpochDomain
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/reclaim/epoch.hpp>
#include <llds/reclaim/hazard_pointers.hpp>

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "example_util.hpp"

struct Config {
    std::uint64_t version;
    std::uint64_t limits[7];
};

struct MutexScheme {
    std::mutex m;
    Config* p = new Config{};
    ~MutexScheme() { delete p; }
    std::uint64_t read() { std::lock_guard g(m); return p->version + p->limits[3]; }
    void replace(Config* c) { Config* old; { std::lock_guard g(m); old = p; p = c; } delete old; }
};

struct SharedPtrScheme {
    std::atomic<std::shared_ptr<Config>> p{std::make_shared<Config>()};
    std::uint64_t read() { auto s = p.load(); return s->version + s->limits[3]; }
    void replace(Config* c) { p.store(std::shared_ptr<Config>(c)); }
};

struct HazardScheme {
    std::atomic<Config*> p{new Config{}};
    ~HazardScheme() { delete p.load(); }
    std::uint64_t read() {
        llds::HazardDomain::Guard g;
        Config* c = g.protect(p);
        return c->version + c->limits[3];
    }
    void replace(Config* c) { llds::HazardDomain::global().retire(p.exchange(c)); }
};

struct EpochScheme {
    std::atomic<Config*> p{new Config{}};
    ~EpochScheme() { delete p.load(); }
    std::uint64_t read() {
        llds::EpochDomain::Guard g;
        Config* c = p.load(std::memory_order_acquire);
        return c->version + c->limits[3];
    }
    void replace(Config* c) { llds::EpochDomain::global().retire(p.exchange(c)); }
};

template <class Scheme>
static void run(const char* name, unsigned readers, const ex::Args& args) {
    Scheme s;
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> total{0};
    std::vector<std::thread> ts;
    for (unsigned r = 0; r < readers; ++r)
        ts.emplace_back([&, r] {
            llds::pin_this_thread(static_cast<int>((r + 1) % llds::hardware_threads()));
            std::uint64_t n = 0, sink = 0;
            while (!stop.load(std::memory_order_relaxed)) {
                sink += s.read();
                ++n;
            }
            llds::do_not_optimize(sink);
            total += n;
        });
    llds::pin_this_thread(0);
    const std::uint64_t writes = args.smoke ? 1000 : 100'000;
    const auto t0 = llds::now_ns();
    for (std::uint64_t i = 1; i <= writes; ++i) {
        s.replace(new Config{i, {}});
        const auto due = t0 + i * 10'000;
        while (llds::now_ns() < due) llds::cpu_relax();
    }
    stop = true;
    for (auto& t : ts) t.join();
    const double secs = static_cast<double>(llds::now_ns() - t0) / 1e9;
    const double per_reader = static_cast<double>(total.load()) / secs / readers;
    std::printf("%-20s R=%u  %8.2f M reads/s per reader  (%6.1f ns per read)\n", name, readers,
                per_reader / 1e6, 1e9 / per_reader);
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const unsigned max_r = std::max(1u, llds::hardware_threads() - 1);
    for (unsigned r = 1; r <= max_r; r = r * 2 + 1) {
        run<MutexScheme>("mutex", r, args);
        run<SharedPtrScheme>("atomic<shared_ptr>", r, args);
        run<HazardScheme>("hazard pointers", r, args);
        run<EpochScheme>("epoch", r, args);
        std::printf("\n");
    }
    return 0;
}
