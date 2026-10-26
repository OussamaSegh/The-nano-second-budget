#include <padded_counters.hpp>

#include <thread>
#include <vector>

#include "check.hpp"

TEST("ex2.1: counters are at least a false-sharing range apart") {
    PerThreadCounters<4> c;
    for (std::size_t i = 0; i + 1 < 4; ++i) {
        auto a = reinterpret_cast<std::uintptr_t>(c.address_of(i));
        auto b = reinterpret_cast<std::uintptr_t>(c.address_of(i + 1));
        CHECK(b - a >= llds::kFalseSharingRange);
        CHECK(a % llds::kCacheLine == 0);
    }
}

TEST("ex2.1: totals are correct") {
    PerThreadCounters<4> c;
    std::vector<std::thread> ts;
    for (std::size_t t = 0; t < 4; ++t)
        ts.emplace_back([&, t] {
            for (int i = 0; i < 100000; ++i) c.add(t, 1);
        });
    for (auto& t : ts) t.join();
    CHECK_EQ(c.total(), 400000u);
}

TEST_MAIN()
