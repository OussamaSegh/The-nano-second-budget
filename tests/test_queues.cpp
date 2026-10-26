#include <llds/queue/disruptor.hpp>
#include <llds/queue/mpmc_bounded.hpp>
#include <llds/queue/ms_queue.hpp>
#include <llds/queue/spsc_ring.hpp>

#include <algorithm>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "check.hpp"

using namespace llds;

TEST("spsc: single-threaded FIFO and capacity") {
    SpscRing<int> q(4);
    CHECK_EQ(q.capacity(), 4u);
    for (int i = 0; i < 4; ++i) CHECK(q.try_push(i));
    CHECK(!q.try_push(99));
    int v = -1;
    for (int i = 0; i < 4; ++i) {
        CHECK(q.try_pop(v));
        CHECK_EQ(v, i);
    }
    CHECK(!q.try_pop(v));
}

TEST("spsc: non-trivial types are destroyed") {
    auto p = std::make_shared<int>(7);
    {
        SpscRing<std::shared_ptr<int>> q(8);
        q.try_push(p);
        q.try_push(p);
        CHECK_EQ(p.use_count(), 3);
    }
    CHECK_EQ(p.use_count(), 1);
}

TEST("spsc: two threads, order preserved") {
    SpscRing<std::uint64_t> q(1024);
    constexpr std::uint64_t N = 500000;
    std::thread prod([&] {
        for (std::uint64_t i = 0; i < N; ++i)
            while (!q.try_push(i)) {}
    });
    std::uint64_t expect = 0;
    bool ordered = true;
    while (expect < N) {
        std::uint64_t v;
        if (q.try_pop(v)) {
            if (v != expect) ordered = false;
            ++expect;
        }
    }
    prod.join();
    CHECK(ordered);
}

TEST("mpmc: every item delivered exactly once") {
    MpmcBoundedQueue<std::uint64_t> q(256);
    constexpr int P = 2, C = 2;
    constexpr std::uint64_t PerProducer = 100000;
    std::vector<std::thread> ts;
    std::vector<std::vector<std::uint64_t>> got(C);
    std::atomic<std::uint64_t> consumed{0};
    for (int p = 0; p < P; ++p)
        ts.emplace_back([&, p] {
            for (std::uint64_t i = 0; i < PerProducer; ++i) q.push(p * PerProducer + i);
        });
    for (int c = 0; c < C; ++c)
        ts.emplace_back([&, c] {
            std::uint64_t v;
            while (consumed.load() < P * PerProducer) {
                if (q.try_pop(v)) {
                    got[c].push_back(v);
                    consumed++;
                }
            }
        });
    for (auto& t : ts) t.join();
    std::vector<std::uint64_t> all;
    for (auto& g : got) all.insert(all.end(), g.begin(), g.end());
    std::sort(all.begin(), all.end());
    bool exact = all.size() == P * PerProducer;
    for (std::size_t i = 0; exact && i < all.size(); ++i) exact = all[i] == i;
    CHECK(exact);
}

TEST("mpmc: per-producer FIFO") {
    MpmcBoundedQueue<std::uint64_t> q(64);
    constexpr std::uint64_t N = 100000;
    std::thread p0([&] { for (std::uint64_t i = 0; i < N; ++i) q.push(i); });
    std::thread p1([&] { for (std::uint64_t i = 0; i < N; ++i) q.push((1ull << 32) | i); });
    std::uint64_t last0 = 0, last1 = 0, n = 0;
    bool fifo = true;
    bool seen0 = false, seen1 = false;
    while (n < 2 * N) {
        std::uint64_t v;
        if (!q.try_pop(v)) continue;
        ++n;
        if (v >> 32) {
            auto x = v & 0xffffffff;
            if (seen1 && x <= last1) fifo = false;
            last1 = x; seen1 = true;
        } else {
            if (seen0 && v <= last0) fifo = false;
            last0 = v; seen0 = true;
        }
    }
    p0.join();
    p1.join();
    CHECK(fifo);
}

TEST("msqueue: single-threaded FIFO") {
    MsQueue<std::string> q;
    CHECK(q.empty());
    q.push(std::string("a"));
    q.push(std::string("b"));
    CHECK_EQ(*q.try_pop(), "a");
    CHECK_EQ(*q.try_pop(), "b");
    CHECK(!q.try_pop());
}

TEST("msqueue: MPMC exactly once") {
    MsQueue<std::uint64_t> q;
    constexpr int P = 2, C = 2;
    constexpr std::uint64_t PerProducer = 50000;
    std::atomic<std::uint64_t> sum{0}, count{0};
    std::vector<std::thread> ts;
    for (int p = 0; p < P; ++p)
        ts.emplace_back([&, p] {
            for (std::uint64_t i = 1; i <= PerProducer; ++i) q.push(i + p * PerProducer);
        });
    for (int c = 0; c < C; ++c)
        ts.emplace_back([&] {
            while (count.load() < P * PerProducer) {
                if (auto v = q.try_pop()) {
                    sum += *v;
                    count++;
                }
            }
        });
    for (auto& t : ts) t.join();
    const std::uint64_t n = P * PerProducer;
    CHECK_EQ(sum.load(), n * (n + 1) / 2);
    CHECK(q.empty());
}

TEST("disruptor: two-stage pipeline sees every entry in order") {
    Disruptor<std::uint64_t> ring(64);
    Sequence& stage1 = ring.add_consumer();
    Sequence& stage2 = ring.add_consumer({&stage1});
    ring.set_gating({&stage2});
    constexpr std::int64_t N = 100000;
    std::vector<std::uint64_t> doubled(N);
    std::atomic<bool> ok1{true}, ok2{true};

    std::thread t1([&] {
        std::int64_t next = 0;
        std::vector<const Sequence*> deps;
        while (next < N) {
            auto avail = ring.wait_for(next, deps);
            for (; next <= avail; ++next) {
                if (ring[next] != static_cast<std::uint64_t>(next)) ok1 = false;
                doubled[static_cast<std::size_t>(next)] = ring[next] * 2;
            }
            stage1.set(avail);
        }
    });
    std::thread t2([&] {
        std::int64_t next = 0;
        std::vector<const Sequence*> deps{&stage1};
        while (next < N) {
            auto avail = ring.wait_for(next, deps);
            for (; next <= avail; ++next)
                if (doubled[static_cast<std::size_t>(next)] != 2 * static_cast<std::uint64_t>(next)) ok2 = false;
            stage2.set(avail);
        }
    });
    for (std::int64_t i = 0; i < N; ++i) {
        auto s = ring.claim();
        ring[s] = static_cast<std::uint64_t>(s);
        ring.publish(s);
    }
    t1.join();
    t2.join();
    CHECK(ok1.load());
    CHECK(ok2.load());
}

TEST_MAIN()
