#include <llds/mem/object_pool.hpp>
#include <llds/reclaim/epoch.hpp>
#include <llds/reclaim/hazard_pointers.hpp>
#include <llds/stack/treiber_stack.hpp>

#include <set>
#include <thread>
#include <vector>

#include "check.hpp"

using namespace llds;

struct Item : TaggedStackHook {
    int id = 0;
};

TEST("tagged stack: LIFO") {
    TaggedStack s;
    Item a, b;
    a.id = 1;
    b.id = 2;
    s.push(&a);
    s.push(&b);
    CHECK_EQ(static_cast<Item*>(s.pop())->id, 2);
    CHECK_EQ(static_cast<Item*>(s.pop())->id, 1);
    CHECK(s.pop() == nullptr);
}

TEST("tagged stack: concurrent push/pop keeps every node") {
    constexpr int N = 1000;
    std::vector<Item> items(N);
    TaggedStack s;
    for (int i = 0; i < N; ++i) {
        items[i].id = i;
        s.push(&items[i]);
    }
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] {
            for (int i = 0; i < 50000; ++i) {
                if (auto* n = s.pop()) s.push(n);
            }
        });
    for (auto& t : ts) t.join();
    std::set<int> ids;
    while (auto* n = s.pop()) ids.insert(static_cast<Item*>(n)->id);
    CHECK_EQ(ids.size(), static_cast<std::size_t>(N));
}

struct Obj {
    explicit Obj(int v) : v(v) { live++; }
    ~Obj() { live--; }
    int v;
    static inline std::atomic<int> live{0};
};

TEST("pool: create/destroy and exhaustion") {
    LockFreePool<Obj> pool(3);
    Obj* a = pool.create(1);
    Obj* b = pool.create(2);
    Obj* c = pool.create(3);
    CHECK(a && b && c);
    CHECK(pool.create(4) == nullptr);
    CHECK(pool.owns(a));
    CHECK_EQ(Obj::live.load(), 3);
    pool.destroy(b);
    Obj* d = pool.create(5);
    CHECK(d == b);  // LIFO reuse: the slot just freed is hot in cache
    pool.destroy(a);
    pool.destroy(c);
    pool.destroy(d);
    CHECK_EQ(Obj::live.load(), 0);
}

TEST("pool: concurrent allocate/free") {
    LockFreePool<std::uint64_t> pool(64);
    std::atomic<bool> bad{false};
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&, t] {
            for (int i = 0; i < 20000; ++i) {
                auto* p = pool.create(static_cast<std::uint64_t>(t));
                if (!p) continue;
                if (*p != static_cast<std::uint64_t>(t)) bad = true;
                pool.destroy(p);
            }
        });
    for (auto& t : ts) t.join();
    CHECK(!bad.load());
    int n = 0;
    std::vector<std::uint64_t*> all;
    while (auto* p = pool.create(0)) { all.push_back(p); ++n; }
    CHECK_EQ(n, 64);
    for (auto* p : all) pool.destroy(p);
}

TEST("arena: bump and reset") {
    Arena a(1024);
    auto* x = a.make<std::uint64_t>(42);
    REQUIRE(x);
    CHECK_EQ(*x, 42u);
    CHECK(reinterpret_cast<std::uintptr_t>(x) % alignof(std::uint64_t) == 0);
    CHECK(a.allocate(2000) == nullptr);
    a.reset();
    CHECK_EQ(a.used(), 0u);
}

struct Tracked {
    static inline std::atomic<int> alive{0};
    int payload = 123;
    Tracked() { alive++; }
    ~Tracked() { payload = -1; alive--; }
};

TEST("hazard pointers: protected node survives a scan") {
    auto& d = HazardDomain::global();
    std::atomic<Tracked*> src{new Tracked};
    {
        HazardDomain::Guard g;
        Tracked* p = g.protect(src);
        src.store(nullptr);
        d.retire(p);
        d.flush();
        CHECK_EQ(p->payload, 123);  // still alive: we hold a hazard on it
        CHECK_EQ(Tracked::alive.load(), 1);
    }
    d.flush();
    CHECK_EQ(Tracked::alive.load(), 0);
}

TEST("hazard pointers: concurrent readers and a writer") {
    std::atomic<Tracked*> src{new Tracked};
    std::atomic<bool> stop{false}, bad{false};
    std::vector<std::thread> readers;
    for (int r = 0; r < 3; ++r)
        readers.emplace_back([&] {
            while (!stop.load()) {
                HazardDomain::Guard g;
                Tracked* p = g.protect(src);
                if (p && p->payload != 123) bad = true;
            }
        });
    for (int i = 0; i < 20000; ++i) {
        Tracked* old = src.exchange(new Tracked);
        HazardDomain::global().retire(old);
    }
    stop = true;
    for (auto& t : readers) t.join();
    HazardDomain::global().retire(src.exchange(nullptr));
    HazardDomain::global().flush();
    CHECK(!bad.load());
    CHECK_EQ(Tracked::alive.load(), 0);
}

TEST("epoch: retired node freed only after readers leave") {
    auto& d = EpochDomain::global();
    std::atomic<Tracked*> src{new Tracked};
    std::atomic<bool> stop{false}, bad{false};
    std::vector<std::thread> readers;
    for (int r = 0; r < 3; ++r)
        readers.emplace_back([&] {
            while (!stop.load()) {
                EpochDomain::Guard g;
                Tracked* p = src.load(std::memory_order_acquire);
                if (p && p->payload != 123) bad = true;
            }
        });
    for (int i = 0; i < 20000; ++i) d.retire(src.exchange(new Tracked));
    stop = true;
    for (auto& t : readers) t.join();
    d.retire(src.exchange(nullptr));
    for (int i = 0; i < 4; ++i) d.collect();
    CHECK(!bad.load());
    CHECK_EQ(Tracked::alive.load(), 0);
    CHECK(d.epoch() > 0);
}

TEST("epoch: 1000 short-lived threads reuse records and free their nodes") {
    // kMaxThreads limits threads registered at once, not threads over the
    // process lifetime: each exiting thread releases its record.
    auto& d = EpochDomain::global();
    for (int t = 0; t < 1000; ++t) {
        std::thread([&] {
            { EpochDomain::Guard g; }
            d.retire(new Tracked);  // left behind when the thread exits
        }).join();
    }
    for (int i = 0; i < 4; ++i) d.collect();
    CHECK_EQ(Tracked::alive.load(), 0);
}

TEST("hazard pointers: 1000 short-lived threads reuse records") {
    auto& d = HazardDomain::global();
    std::atomic<Tracked*> src{new Tracked};
    for (int t = 0; t < 1000; ++t) {
        std::thread([&] {
            HazardDomain::Guard g;
            Tracked* p = g.protect(src);
            CHECK(p != nullptr);
            g.reset();
            d.retire(src.exchange(new Tracked));
        }).join();
    }
    d.retire(src.exchange(nullptr));
    d.flush();
    CHECK_EQ(Tracked::alive.load(), 0);
}

TEST_MAIN()
