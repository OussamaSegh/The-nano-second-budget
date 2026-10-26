#include <config_holder.hpp>

#include <thread>
#include <vector>

#include "check.hpp"

struct Limits {
    static inline std::atomic<int> alive{0};
    std::uint64_t version = 0;
    std::uint64_t max_rate = 0;   // invariant: max_rate == version * 10
    std::uint64_t poisoned = 0;   // set by the destructor
    Limits() { alive++; }
    Limits(std::uint64_t v) : version(v), max_rate(v * 10) { alive++; }
    Limits(const Limits& o) : version(o.version), max_rate(o.max_rate) { alive++; }
    Limits& operator=(const Limits&) = default;
    ~Limits() {
        poisoned = 0xdead;
        version = 1;
        max_rate = 0;
        alive--;
    }
};

TEST("ex12.1: readers never observe a destroyed object") {
    {
        ConfigHolder<Limits> h(Limits(1));
        std::atomic<bool> stop{false}, bad{false};
        std::vector<std::thread> readers;
        for (int r = 0; r < 3; ++r)
            readers.emplace_back([&] {
                while (!stop.load()) {
                    h.read([&](const Limits& l) {
                        // Linger inside the read to widen the race window.
                        for (int i = 0; i < 50; ++i)
                            if (l.poisoned || l.max_rate != l.version * 10) bad = true;
                    });
                }
            });
        for (std::uint64_t v = 2; v < 20000; ++v) h.update(Limits(v));
        stop = true;
        for (auto& t : readers) t.join();
        CHECK(!bad.load());
        CHECK_EQ(h.read([](const Limits& l) { return l.version; }), 19999u);
    }
    llds::HazardDomain::global().flush();
    CHECK_EQ(Limits::alive.load(), 0);
}

TEST("ex12.1: concurrent modify() loses no updates") {
    ConfigHolder<Limits> h(Limits(0));
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] {
            for (int i = 0; i < 2000; ++i)
                h.modify([](Limits& l) {
                    l.version += 1;
                    l.max_rate = l.version * 10;
                });
        });
    for (auto& t : ts) t.join();
    CHECK_EQ(h.read([](const Limits& l) { return l.version; }), 8000u);
}

TEST_MAIN()
