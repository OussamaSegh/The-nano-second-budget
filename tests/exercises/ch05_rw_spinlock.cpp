#include <rw_spinlock.hpp>

#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "check.hpp"

TEST("ex5.1: try_lock semantics") {
    RwSpinLock l;
    REQUIRE(l.try_lock_shared());
    CHECK(l.try_lock_shared());  // two readers at once
    CHECK(!l.try_lock());        // writer excluded by readers
    l.unlock_shared();
    l.unlock_shared();
    REQUIRE(l.try_lock());
    CHECK(!l.try_lock_shared());  // readers excluded by writer
    CHECK(!l.try_lock());
    l.unlock();
    CHECK(l.try_lock_shared());
    l.unlock_shared();
}

TEST("ex5.1: writers exclude everyone, readers see consistent pairs") {
    RwSpinLock l;
    REQUIRE(l.try_lock());
    l.unlock();
    long a = 0, b = 0;  // invariant under the lock: a == b
    std::atomic<bool> bad{false};
    std::vector<std::thread> ts;
    for (int w = 0; w < 2; ++w)
        ts.emplace_back([&] {
            for (int i = 0; i < 20000; ++i) {
                std::unique_lock g(l);
                ++a;
                ++b;
            }
        });
    for (int r = 0; r < 2; ++r)
        ts.emplace_back([&] {
            for (int i = 0; i < 20000; ++i) {
                std::shared_lock g(l);
                if (a != b) bad = true;
            }
        });
    for (auto& t : ts) t.join();
    CHECK(!bad.load());
    CHECK_EQ(a, 40000);
}

TEST_MAIN()
