#include <llds/sync/seqlock.hpp>
#include <llds/sync/spinlock.hpp>

#include <mutex>
#include <thread>
#include <vector>

#include "check.hpp"

using namespace llds;

template <class Lock>
static void hammer() {
    Lock lock;
    long counter = 0;  // deliberately non-atomic: the lock must protect it
    constexpr int kThreads = 4, kIters = 20000;
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([&] {
            for (int i = 0; i < kIters; ++i) {
                std::lock_guard g(lock);
                ++counter;
            }
        });
    for (auto& t : ts) t.join();
    CHECK_EQ(counter, static_cast<long>(kThreads) * kIters);
}

TEST("spinlock: TAS mutual exclusion") { hammer<TasLock>(); }
TEST("spinlock: TTAS mutual exclusion") { hammer<TtasLock>(); }
TEST("spinlock: backoff mutual exclusion") { hammer<BackoffLock>(); }
TEST("spinlock: ticket mutual exclusion") { hammer<TicketLock>(); }

TEST("spinlock: MCS mutual exclusion") {
    McsLock lock;
    long counter = 0;
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] {
            for (int i = 0; i < 20000; ++i) {
                McsLock::Guard g(lock);
                ++counter;
            }
        });
    for (auto& t : ts) t.join();
    CHECK_EQ(counter, 80000);
}

TEST("spinlock: try_lock") {
    TicketLock t;
    CHECK(t.try_lock());
    CHECK(!t.try_lock());
    t.unlock();
    CHECK(t.try_lock());
    t.unlock();
    TtasLock l;
    CHECK(l.try_lock());
    CHECK(!l.try_lock());
    l.unlock();
}

struct Triple {
    std::uint64_t a, b, c;
};

TEST("seqlock: readers never see torn values") {
    // The writer starts only when every reader is running, and keeps writing
    // until each reader has completed many reads, so the test cannot pass
    // vacuously when the scheduler runs the writer first.
    constexpr int kReaders = 3;
    constexpr long kMinReads = 2000;
    constexpr std::uint64_t kMinStores = 100000;
    SeqLock<Triple> s(Triple{0, 0, 0});
    std::atomic<bool> stop{false};
    std::atomic<int> ready{0};
    std::atomic<long> torn{0};
    std::atomic<long> reads[kReaders] = {};
    std::vector<std::thread> readers;
    for (int r = 0; r < kReaders; ++r)
        readers.emplace_back([&, r] {
            ready++;
            while (!stop.load(std::memory_order_relaxed)) {
                Triple t = s.load();
                if (t.a != t.b || t.b != t.c) torn++;
                reads[r].fetch_add(1, std::memory_order_relaxed);
            }
        });
    while (ready.load() < kReaders) std::this_thread::yield();
    auto all_read_enough = [&] {
        for (auto& n : reads)
            if (n.load(std::memory_order_relaxed) < kMinReads) return false;
        return true;
    };
    std::uint64_t i = 0;
    while (i < kMinStores || !all_read_enough()) {
        ++i;
        s.store(Triple{i, i, i});
        if (i % 1024 == 0) std::this_thread::yield();  // let readers run on a busy machine
    }
    stop = true;
    for (auto& t : readers) t.join();
    CHECK_EQ(torn.load(), 0);
    for (auto& n : reads) CHECK(n.load() >= kMinReads);
    CHECK_EQ(s.version(), i + 1);  // one store from the constructor
}

TEST_MAIN()
