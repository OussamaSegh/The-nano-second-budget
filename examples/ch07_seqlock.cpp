// Chapter 7: publishing a snapshot to many readers.
//
// One writer updates a 48-byte "vital signs" record 100,000 times a second
// (think: a bedside monitor's fused sensor state, or a market-data top of
// book). R readers read it as fast as they can. We compare three ways to
// share it: std::mutex, std::shared_mutex, and a SeqLock.
//
// Watch two numbers: reader throughput (does it scale with R?) and the
// writer's own latency (do readers slow the writer down?).
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/sync/seqlock.hpp>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "example_util.hpp"

struct Vitals {
    std::uint64_t timestamp_ns;
    std::int32_t heart_rate_bpm;
    std::int32_t spo2_permille;
    std::int32_t resp_rate_bpm;
    std::int32_t systolic_mmhg;
    std::int32_t diastolic_mmhg;
    std::int32_t temperature_mc;  // milli-degrees Celsius
    std::uint64_t sequence;       // == timestamp_ns in this test: lets us detect tearing
    std::uint64_t checksum;
};

static Vitals make(std::uint64_t i) {
    Vitals v{i, 60 + static_cast<int>(i % 40), 950 + static_cast<int>(i % 50), 12, 120, 80, 37000, i, 0};
    v.checksum = v.timestamp_ns ^ static_cast<std::uint64_t>(v.heart_rate_bpm) ^ v.sequence;
    return v;
}
static bool consistent(const Vitals& v) {
    return v.sequence == v.timestamp_ns &&
           v.checksum == (v.timestamp_ns ^ static_cast<std::uint64_t>(v.heart_rate_bpm) ^ v.sequence);
}

struct MutexBox {
    std::mutex m;
    Vitals v{};
    void store(const Vitals& x) { std::lock_guard g(m); v = x; }
    Vitals load() { std::lock_guard g(m); return v; }
};
struct SharedMutexBox {
    std::shared_mutex m;
    Vitals v{};
    void store(const Vitals& x) { std::unique_lock g(m); v = x; }
    Vitals load() { std::shared_lock g(m); return v; }
};
struct SeqBox {
    llds::SeqLock<Vitals> s;
    void store(const Vitals& x) { s.store(x); }
    Vitals load() { return s.load(); }
};

template <class Box>
static void run(const char* name, unsigned readers, const ex::Args& args) {
    Box box;
    box.store(make(0));
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> reads{0}, torn{0};
    std::vector<std::thread> ts;
    for (unsigned r = 0; r < readers; ++r)
        ts.emplace_back([&, r] {
            llds::pin_this_thread(static_cast<int>((r + 1) % llds::hardware_threads()));
            std::uint64_t local = 0, bad = 0;
            while (!stop.load(std::memory_order_relaxed)) {
                Vitals v = box.load();
                if (!consistent(v)) ++bad;
                ++local;
            }
            reads += local;
            torn += bad;
        });

    llds::pin_this_thread(0);
    static const llds::TscClock clock;
    llds::Histogram write_lat;
    const std::uint64_t writes = args.smoke ? 2'000 : 200'000;
    const auto t0 = llds::now_ns();
    for (std::uint64_t i = 1; i <= writes; ++i) {
        const auto c0 = llds::cycles();
        box.store(make(i));
        write_lat.record(static_cast<std::uint64_t>(clock.to_ns(llds::cycles_serialized() - c0)));
        // pace at ~100 kHz
        const auto due = t0 + i * 10'000;
        while (llds::now_ns() < due) llds::cpu_relax();
    }
    stop = true;
    for (auto& t : ts) t.join();
    const double secs = static_cast<double>(llds::now_ns() - t0) / 1e9;
    std::printf("%-14s R=%u  reads %8.2f M/s  torn=%llu  write p50=%4llu p99=%6llu max=%8llu ns\n", name,
                readers, static_cast<double>(reads.load()) / secs / 1e6,
                static_cast<unsigned long long>(torn.load()),
                static_cast<unsigned long long>(write_lat.percentile(50)),
                static_cast<unsigned long long>(write_lat.percentile(99)),
                static_cast<unsigned long long>(write_lat.max()));
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const unsigned max_r = std::max(1u, std::min(7u, llds::hardware_threads() - 1));
    for (unsigned r = 1; r <= max_r; r = r * 2 + 1) {
        run<MutexBox>("std::mutex", r, args);
        run<SharedMutexBox>("shared_mutex", r, args);
        run<SeqBox>("SeqLock", r, args);
        std::printf("\n");
    }
    return 0;
}
