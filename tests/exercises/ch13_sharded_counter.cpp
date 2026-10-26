#include <sharded_counter.hpp>

#include <thread>
#include <vector>

#include "check.hpp"

TEST("ex13.1: exact once writers are done") {
    ShardedCounter<16> c;
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] {
            for (int i = 0; i < 250000; ++i) c.add();
        });
    for (auto& t : ts) t.join();
    CHECK_EQ(c.read(), 1'000'000u);
}

TEST("ex13.1: the counter is actually sharded") {
    // A sharded counter must be at least Shards cache lines big.
    CHECK(sizeof(ShardedCounter<16>) >= 16 * llds::kCacheLine);
}

TEST_MAIN()
