// Chapter 8: building the fastest SPSC queue, one step at a time.
//
// Each version changes exactly one thing relative to the previous one:
//   v1  std::mutex + std::deque                      the baseline
//   v2  atomic indices, modulo wrap, seq_cst          "lock-free", naive
//   v3  power-of-two mask, acquire/release            cheaper ordering
//   v4  head and tail on separate cache lines         no false sharing
//   v5  cached remote index (llds::SpscRing)          minimal coherence traffic
//
// For each version we measure throughput (one producer streaming to one
// consumer) and round-trip latency (ping-pong through two queues).
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/queue/spsc_ring.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "example_util.hpp"

using Item = std::uint64_t;

// --- v1 ------------------------------------------------------------------
class V1MutexDeque {
public:
    explicit V1MutexDeque(std::size_t cap) : cap_(cap) {}
    bool try_push(Item v) {
        std::lock_guard g(m_);
        if (q_.size() == cap_) return false;
        q_.push_back(v);
        return true;
    }
    bool try_pop(Item& v) {
        std::lock_guard g(m_);
        if (q_.empty()) return false;
        v = q_.front();
        q_.pop_front();
        return true;
    }

private:
    std::mutex m_;
    std::deque<Item> q_;
    std::size_t cap_;
};

// --- v2 ------------------------------------------------------------------
class V2NaiveAtomic {
public:
    explicit V2NaiveAtomic(std::size_t cap) : cap_(cap + 1), buf_(cap_) {}
    bool try_push(Item v) {
        const auto t = tail_.load();
        const auto next = (t + 1) % cap_;
        if (next == head_.load()) return false;
        buf_[t] = v;
        tail_.store(next);
        return true;
    }
    bool try_pop(Item& v) {
        const auto h = head_.load();
        if (h == tail_.load()) return false;
        v = buf_[h];
        head_.store((h + 1) % cap_);
        return true;
    }

private:
    std::size_t cap_;
    std::vector<Item> buf_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

// --- v3 ------------------------------------------------------------------
class V3MaskAcqRel {
public:
    explicit V3MaskAcqRel(std::size_t cap) : mask_(llds::next_pow2(cap) - 1), buf_(mask_ + 1) {}
    bool try_push(Item v) {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t - head_.load(std::memory_order_acquire) == mask_ + 1) return false;
        buf_[t & mask_] = v;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
    bool try_pop(Item& v) {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) return false;
        v = buf_[h & mask_];
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

private:
    std::size_t mask_;
    std::vector<Item> buf_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

// --- v4 ------------------------------------------------------------------
class V4Padded {
public:
    explicit V4Padded(std::size_t cap) : mask_(llds::next_pow2(cap) - 1), buf_(mask_ + 1) {}
    bool try_push(Item v) {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t - head_.load(std::memory_order_acquire) == mask_ + 1) return false;
        buf_[t & mask_] = v;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
    bool try_pop(Item& v) {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) return false;
        v = buf_[h & mask_];
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

private:
    std::size_t mask_;
    std::vector<Item> buf_;
    alignas(llds::kFalseSharingRange) std::atomic<std::size_t> head_{0};
    alignas(llds::kFalseSharingRange) std::atomic<std::size_t> tail_{0};
};

// --- v5 is llds::SpscRing<Item> --------------------------------------------
using V5Cached = llds::SpscRing<Item>;

template <class Q>
static double throughput_mops(const ex::Args& args, std::uint64_t n) {
    Q q(1024);
    std::thread consumer([&] {
        llds::pin_this_thread(args.cpu_b);
        Item v, expect = 0;
        while (expect < n) {
            if (q.try_pop(v)) {
                if (v != expect) std::abort();
                ++expect;
            }
        }
    });
    llds::pin_this_thread(args.cpu_a);
    const auto t0 = llds::now_ns();
    for (Item i = 0; i < n; ++i)
        while (!q.try_push(i)) {}
    consumer.join();
    return static_cast<double>(n) / (static_cast<double>(llds::now_ns() - t0) / 1e3);
}

template <class Q>
static llds::Histogram round_trip(const ex::Args& args, std::uint64_t n) {
    Q ping(64), pong(64);
    std::thread echo([&] {
        llds::pin_this_thread(args.cpu_b);
        Item v;
        for (std::uint64_t i = 0; i < n; ++i) {
            while (!ping.try_pop(v)) {}
            while (!pong.try_push(v)) {}
        }
    });
    llds::pin_this_thread(args.cpu_a);
    static const llds::TscClock clock;
    llds::Histogram h;
    for (std::uint64_t i = 0; i < n; ++i) {
        const auto c0 = llds::cycles();
        while (!ping.try_push(i)) {}
        Item v;
        while (!pong.try_pop(v)) {}
        h.record(static_cast<std::uint64_t>(clock.to_ns(llds::cycles_serialized() - c0)));
    }
    echo.join();
    return h;
}

template <class Q>
static void report(const char* name, const ex::Args& args) {
    // SPSC throughput is bimodal: depending on how the first few thousand
    // items race, the ring settles either near-empty (every item is a
    // cache-line hand-off) or near-full (hand-offs are amortised). So we run
    // several times and report the median and the range.
    constexpr int kRuns = 7;
    std::vector<double> runs;
    for (int r = 0; r < kRuns; ++r) runs.push_back(throughput_mops<Q>(args, args.n(20'000'000)));
    std::sort(runs.begin(), runs.end());
    const auto rt = round_trip<Q>(args, args.n(2'000'000));
    std::printf("%-26s %6.1f M/s (range %6.1f-%6.1f)  RTT p50=%5llu p99=%6llu p99.9=%7llu ns\n", name,
                runs[kRuns / 2], runs.front(), runs.back(),
                static_cast<unsigned long long>(rt.percentile(50)),
                static_cast<unsigned long long>(rt.percentile(99)),
                static_cast<unsigned long long>(rt.percentile(99.9)));
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    std::printf("producer cpu %d, consumer cpu %d\n", args.cpu_a, args.cpu_b);
    report<V1MutexDeque>("v1 mutex + deque", args);
    report<V2NaiveAtomic>("v2 atomic, modulo, seq_cst", args);
    report<V3MaskAcqRel>("v3 mask, acquire/release", args);
    report<V4Padded>("v4 + padding", args);
    report<V5Cached>("v5 + cached indices", args);
    return 0;
}
