// Chapter 10: a multi-stage event pipeline, two ways.
//
//  (a) chained SPSC queues:  producer -> [q1] -> parse -> [q2] -> process
//  (b) one Disruptor ring:   producer -> ring; parse and journal read the
//      ring directly; process depends on parse. No copies between stages,
//      and the journal runs in parallel with parse for free.
//
// Events carry their publish timestamp; we report end-to-end latency at the
// final stage and overall throughput.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/queue/disruptor.hpp>
#include <llds/queue/spsc_ring.hpp>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "example_util.hpp"

struct Event {
    std::uint64_t t_publish = 0;
    std::uint64_t raw = 0;      // "wire" payload
    std::uint64_t parsed = 0;   // filled by the parse stage
    std::uint64_t result = 0;   // filled by the process stage
};

static int cpu(int i) { return i % static_cast<int>(llds::hardware_threads()); }

static void chained(const ex::Args& args, std::uint64_t n, std::uint64_t gap_ns) {
    llds::SpscRing<Event> q1(1024), q2(1024);
    static const llds::TscClock clock;
    llds::Histogram lat;
    std::uint64_t checksum = 0;
    std::atomic<int> ready{0};

    std::thread parse([&] {
        llds::pin_this_thread(cpu(1));
        ready++;
        Event e;
        for (std::uint64_t i = 0; i < n; ++i) {
            while (!q1.try_pop(e)) {}
            e.parsed = e.raw * 3 + 1;
            while (!q2.try_push(e)) {}
        }
    });
    std::thread process([&] {
        llds::pin_this_thread(cpu(2));
        ready++;
        Event e;
        for (std::uint64_t i = 0; i < n; ++i) {
            while (!q2.try_pop(e)) {}
            e.result = e.parsed ^ (e.parsed >> 7);
            checksum += e.result;
            lat.record(static_cast<std::uint64_t>(clock.to_ns(llds::cycles() - e.t_publish)));
        }
    });
    llds::pin_this_thread(cpu(0));
    while (ready.load() < 2) llds::cpu_relax();  // don't start the clock before the stages run
    const auto t0 = llds::now_ns();
    for (std::uint64_t i = 0; i < n; ++i) {
        if (gap_ns) {
            const auto due = t0 + i * gap_ns;
            while (llds::now_ns() < due) llds::cpu_relax();
        }
        Event e;
        e.raw = i;
        e.t_publish = llds::cycles();
        while (!q1.try_push(e)) {}
    }
    parse.join();
    process.join();
    const double secs = static_cast<double>(llds::now_ns() - t0) / 1e9;
    std::printf("chained SPSC  %s %7.2f M ev/s  e2e p50=%6llu p99=%8llu ns  (checksum %llx)\n",
                gap_ns ? "paced " : "flood ", static_cast<double>(n) / secs / 1e6,
                static_cast<unsigned long long>(lat.percentile(50)),
                static_cast<unsigned long long>(lat.percentile(99)), static_cast<unsigned long long>(checksum));
    (void)args;
}

static void disruptor(const ex::Args& args, std::uint64_t n, std::uint64_t gap_ns) {
    llds::Disruptor<Event> ring(1024);
    llds::Sequence& parse_seq = ring.add_consumer();
    llds::Sequence& journal_seq = ring.add_consumer();
    llds::Sequence& process_seq = ring.add_consumer({&parse_seq});
    ring.set_gating({&process_seq, &journal_seq});
    static const llds::TscClock clock;
    llds::Histogram lat;
    std::uint64_t checksum = 0, journaled = 0;
    const auto last = static_cast<std::int64_t>(n) - 1;
    std::atomic<int> ready{0};

    auto stage = [&](int core, llds::Sequence& me, std::vector<const llds::Sequence*> deps, auto&& on_event) {
        return std::thread([&, core, deps, on_event]() mutable {
            llds::pin_this_thread(cpu(core));
            ready++;
            std::int64_t next = 0;
            while (next <= last) {
                const auto avail = ring.wait_for(next, deps);
                for (; next <= avail; ++next) on_event(ring[next]);
                me.set(avail);  // one store per batch, not per event
            }
        });
    };
    auto parse = stage(1, parse_seq, {}, [](Event& e) { e.parsed = e.raw * 3 + 1; });
    auto journal = stage(3, journal_seq, {}, [&](Event& e) { journaled += e.raw & 1; });
    auto process = stage(2, process_seq, {&parse_seq}, [&](Event& e) {
        e.result = e.parsed ^ (e.parsed >> 7);
        checksum += e.result;
        lat.record(static_cast<std::uint64_t>(clock.to_ns(llds::cycles() - e.t_publish)));
    });

    llds::pin_this_thread(cpu(0));
    while (ready.load() < 3) llds::cpu_relax();
    const auto t0 = llds::now_ns();
    for (std::uint64_t i = 0; i < n; ++i) {
        if (gap_ns) {
            const auto due = t0 + i * gap_ns;
            while (llds::now_ns() < due) llds::cpu_relax();
        }
        const auto s = ring.claim();
        Event& e = ring[s];
        e.raw = i;
        e.t_publish = llds::cycles();
        ring.publish(s);
    }
    parse.join();
    journal.join();
    process.join();
    const double secs = static_cast<double>(llds::now_ns() - t0) / 1e9;
    std::printf("disruptor     %s %7.2f M ev/s  e2e p50=%6llu p99=%8llu ns  (checksum %llx, +journal)\n",
                gap_ns ? "paced " : "flood ", static_cast<double>(n) / secs / 1e6,
                static_cast<unsigned long long>(lat.percentile(50)),
                static_cast<unsigned long long>(lat.percentile(99)), static_cast<unsigned long long>(checksum));
    (void)args;
    (void)journaled;
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    // Flood: the producer never waits, so queues fill and the latency you see
    // is mostly queueing delay. Paced: one event every 1 us, well below
    // capacity, so latency is the pipeline's own hand-off cost.
    const auto n = args.n(20'000'000);
    chained(args, n, 0);
    disruptor(args, n, 0);
    const auto np = args.n(2'000'000);
    chained(args, np, 1000);
    disruptor(args, np, 1000);
    return 0;
}
