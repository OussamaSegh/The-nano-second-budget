// Chapter 16: finding concurrency bugs with stress tests.
//
// BuggyRing is SpscRing's simplest ancestor with ONE deliberate bug: the
// producer publishes the new tail with a relaxed store instead of a release
// store. Nothing orders the write of the element before the index update,
// so the consumer may read a slot before its contents arrive.
//
// On x86 the hardware never reorders two stores, so this bug is invisible
// at runtime; only the compiler could expose it, and today it usually
// doesn't. Built with -DLLDS_SANITIZE=thread, ThreadSanitizer reports it on
// the first run. On ARM the stress test below catches it directly. That
// asymmetry is the whole lesson of the chapter: tests on x86 are not
// evidence of correctness; tools that check the memory model are.
//
//   ./ch16_stress          run the stress test against both rings
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/queue/spsc_ring.hpp>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "example_util.hpp"

struct Payload {
    std::uint64_t a, b, c, d;  // invariant: all four equal
};

template <bool Buggy>
class Ring {
public:
    bool try_push(const Payload& p) {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t - head_.load(std::memory_order_acquire) == kCap) return false;
        slots_[t % kCap] = p;  // plain write, ordered only by the tail store
        tail_.store(t + 1, Buggy ? std::memory_order_relaxed : std::memory_order_release);
        return true;
    }
    bool try_pop(Payload& p) {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(Buggy ? std::memory_order_relaxed : std::memory_order_acquire)) return false;
        p = slots_[h % kCap];  // plain read
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

private:
    static constexpr std::uint64_t kCap = 8;  // tiny: maximise wrap-around races
    Payload slots_[kCap]{};
    alignas(64) std::atomic<std::uint64_t> head_{0};
    alignas(64) std::atomic<std::uint64_t> tail_{0};
};

template <class Q>
static std::uint64_t stress(const ex::Args& args, std::uint64_t n) {
    Q q;
    std::atomic<std::uint64_t> bad{0};
    std::thread consumer([&] {
        llds::pin_this_thread(args.cpu_b);
        Payload p;
        for (std::uint64_t i = 1; i <= n; ++i) {
            while (!q.try_pop(p)) {}
            if (p.a != i || p.b != i || p.c != i || p.d != i) bad++;
        }
    });
    llds::pin_this_thread(args.cpu_a);
    for (std::uint64_t i = 1; i <= n; ++i)
        while (!q.try_push(Payload{i, i, i, i})) {}
    consumer.join();
    return bad.load();
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const auto n = args.n(20'000'000);
    std::printf("correct ring: %llu bad reads out of %llu\n",
                static_cast<unsigned long long>(stress<Ring<false>>(args, n)),
                static_cast<unsigned long long>(n));
    std::printf("buggy ring:   %llu bad reads out of %llu  (0 on x86 is expected: see the comment)\n",
                static_cast<unsigned long long>(stress<Ring<true>>(args, n)),
                static_cast<unsigned long long>(n));
    return 0;
}
