// Chapter 11: the allocator on the hot path.
//
// Pattern 1 (same thread): allocate a 64-byte message, touch it, free it.
// Pattern 2 (cross thread): the producer allocates, the consumer frees, the
// classic shape of a message pipeline and the worst case for malloc's
// thread caches.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/mem/object_pool.hpp>
#include <llds/queue/spsc_ring.hpp>

#include <cstdio>
#include <memory_resource>
#include <thread>
#include <vector>

#include "example_util.hpp"

struct Msg {
    std::uint64_t words[8];
};

static void same_thread(const ex::Args& args) {
    const auto n = args.n(5'000'000);
    std::printf("same-thread alloc+free of 64 B (per-operation latency):\n");
    {
        auto h = llds::measure_each([] {
            auto* m = new Msg;
            m->words[0] = 1;
            llds::do_not_optimize(m);
            delete m;
        }, {n, 10000});
        std::printf("  new/delete        %s\n", h.summary().c_str());
    }
    {
        llds::LockFreePool<Msg> pool(1024);
        auto h = llds::measure_each([&] {
            auto* m = pool.create();
            m->words[0] = 1;
            llds::do_not_optimize(m);
            pool.destroy(m);
        }, {n, 10000});
        std::printf("  LockFreePool      %s\n", h.summary().c_str());
    }
    {
        llds::Arena arena(1 << 20);
        auto h = llds::measure_each([&] {
            auto* m = arena.make<Msg>();
            if (!m) { arena.reset(); m = arena.make<Msg>(); }
            m->words[0] = 1;
            llds::do_not_optimize(m);
        }, {n, 10000});
        std::printf("  Arena (bump)      %s\n", h.summary().c_str());
    }
    {
        std::pmr::unsynchronized_pool_resource res;
        auto h = llds::measure_each([&] {
            void* p = res.allocate(sizeof(Msg), alignof(Msg));
            static_cast<Msg*>(p)->words[0] = 1;
            llds::do_not_optimize(p);
            res.deallocate(p, sizeof(Msg), alignof(Msg));
        }, {n, 10000});
        std::printf("  pmr pool          %s\n", h.summary().c_str());
    }
}

template <class Alloc, class Free>
static void cross_thread_run(const char* name, const ex::Args& args, Alloc&& alloc, Free&& free_fn) {
    const auto n = args.n(5'000'000);
    llds::SpscRing<Msg*> q(512);
    static const llds::TscClock clock;
    llds::Histogram h;
    std::thread consumer([&] {
        llds::pin_this_thread(args.cpu_b);
        Msg* m;
        for (std::uint64_t i = 0; i < n; ++i) {
            while (!q.try_pop(m)) {}
            free_fn(m);
        }
    });
    llds::pin_this_thread(args.cpu_a);
    for (std::uint64_t i = 0; i < n; ++i) {
        const auto c0 = llds::cycles();
        Msg* m = alloc();
        const auto c1 = llds::cycles_serialized();
        if (!m) { --i; continue; }  // pool momentarily empty: retry
        h.record(static_cast<std::uint64_t>(clock.to_ns(c1 - c0)));
        m->words[0] = i;
        while (!q.try_push(m)) {}
    }
    consumer.join();
    std::printf("  %-16s  alloc %s\n", name, h.summary().c_str());
}

// Pattern 3: the pipeline-friendly design. The producer owns a private,
// single-threaded free list. The consumer never touches it: it sends freed
// buffers back through a second SPSC ring, and the producer drains that
// ring only when its private list runs dry. No cache line is ever written
// by both threads except the two rings' indices.
static void return_ring(const ex::Args& args) {
    const auto n = args.n(5'000'000);
    constexpr std::size_t kBuffers = 2048;
    std::vector<Msg> storage(kBuffers);  // allocated and pre-faulted once
    std::vector<Msg*> free_list;         // producer-private
    free_list.reserve(kBuffers);
    for (auto& m : storage) free_list.push_back(&m);
    llds::SpscRing<Msg*> to_consumer(512), to_producer(kBuffers);
    static const llds::TscClock clock;
    llds::Histogram h;
    std::thread consumer([&] {
        llds::pin_this_thread(args.cpu_b);
        Msg* m;
        for (std::uint64_t i = 0; i < n; ++i) {
            while (!to_consumer.try_pop(m)) {}
            while (!to_producer.try_push(m)) {}  // "free" = hand it back
        }
    });
    llds::pin_this_thread(args.cpu_a);
    for (std::uint64_t i = 0; i < n; ++i) {
        const auto c0 = llds::cycles();
        if (free_list.empty()) {
            Msg* back;
            while (to_producer.try_pop(back)) free_list.push_back(back);
        }
        if (free_list.empty()) { --i; continue; }
        Msg* m = free_list.back();
        free_list.pop_back();
        const auto c1 = llds::cycles_serialized();
        h.record(static_cast<std::uint64_t>(clock.to_ns(c1 - c0)));
        m->words[0] = i;
        while (!to_consumer.try_push(m)) {}
    }
    consumer.join();
    std::printf("  %-16s  alloc %s\n", "private list +ring", h.summary().c_str());
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    same_thread(args);
    std::printf("\ncross-thread (producer allocates, consumer frees):\n");
    cross_thread_run("new/delete", args, [] { return new Msg; }, [](Msg* m) { delete m; });
    llds::LockFreePool<Msg> pool(2048);
    cross_thread_run("LockFreePool", args, [&] { return pool.create(); }, [&](Msg* m) { pool.destroy(m); });
    return_ring(args);
    return 0;
}
