// Chapter 17: tuning the whole system, one knob at a time.
//
// A three-thread pipeline (feed -> work -> sink) moves timestamped events
// at a fixed rate of one every 2 us. We measure end-to-end latency at the
// sink under four configurations:
//
//   A  unpinned threads, consumers sleep 50 us when idle (a "polite" design)
//   B  unpinned threads, consumers yield when idle
//   C  unpinned threads, consumers busy-spin
//   D  pinned threads, busy-spin, warm caches, pre-faulted memory
//
// On a quiet, isolated machine D should show a p99.9 within a few hundred
// nanoseconds of its p50. On a shared cloud VM the gap tells you how much
// of your tail is the platform, not your code.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/queue/spsc_ring.hpp>

#include <chrono>
#include <cstdio>
#include <thread>

#include "example_util.hpp"

enum class Wait { Sleep, Yield, Spin };

struct Ev {
    std::uint64_t t0;
    std::uint64_t v;
};

static void idle(Wait w) {
    switch (w) {
        case Wait::Sleep: std::this_thread::sleep_for(std::chrono::microseconds(50)); break;
        case Wait::Yield: std::this_thread::yield(); break;
        case Wait::Spin: llds::cpu_relax(); break;
    }
}

static void run(const char* name, Wait w, bool pin, std::uint64_t n) {
    llds::SpscRing<Ev> q1(4096), q2(4096);
    static const llds::TscClock clock;
    llds::Histogram h;
    std::thread work([&] {
        if (pin) llds::pin_this_thread(1);
        Ev e;
        for (std::uint64_t i = 0; i < n; ++i) {
            while (!q1.try_pop(e)) idle(w);
            e.v = e.v * 2654435761u;
            while (!q2.try_push(e)) {}
        }
    });
    std::thread sink([&] {
        if (pin) llds::pin_this_thread(2);
        Ev e;
        for (std::uint64_t i = 0; i < n; ++i) {
            while (!q2.try_pop(e)) idle(w);
            h.record(static_cast<std::uint64_t>(clock.to_ns(llds::cycles() - e.t0)));
        }
    });
    if (pin) llds::pin_this_thread(0);
    const auto start = llds::now_ns();
    for (std::uint64_t i = 0; i < n; ++i) {
        const auto due = start + i * 2'000;
        while (llds::now_ns() < due) llds::cpu_relax();
        while (!q1.try_push(Ev{llds::cycles(), i})) {}
    }
    work.join();
    sink.join();
    std::printf("%-30s p50=%7llu p99=%8llu p99.9=%9llu max=%9llu ns\n", name,
                static_cast<unsigned long long>(h.percentile(50)),
                static_cast<unsigned long long>(h.percentile(99)),
                static_cast<unsigned long long>(h.percentile(99.9)),
                static_cast<unsigned long long>(h.max()));
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    const auto n = args.n(1'000'000);
    run("A unpinned, sleep when idle", Wait::Sleep, false, n);
    run("B unpinned, yield when idle", Wait::Yield, false, n);
    run("C unpinned, busy-spin", Wait::Spin, false, n);
    run("D pinned, busy-spin", Wait::Spin, true, n);
    return 0;
}
