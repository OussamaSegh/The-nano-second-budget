#include <vitals_board.hpp>

#include <thread>
#include <vector>

#include "check.hpp"

TEST("ex7.1: no torn reads") {
    // The writer starts only when every reader is running, and keeps
    // publishing until each reader has completed many reads, so readers
    // always overlap writes (an unsynchronised board cannot pass by luck).
    constexpr int kReaders = 3;
    constexpr long kMinReads = 20000;
    VitalsBoard b;
    std::atomic<bool> stop{false};
    std::atomic<int> ready{0};
    std::atomic<long> torn{0};
    std::atomic<long> reads[kReaders] = {};
    std::vector<std::thread> readers;
    for (int r = 0; r < kReaders; ++r)
        readers.emplace_back([&, r] {
            ready++;
            while (!stop.load(std::memory_order_relaxed)) {
                auto v = b.read();
                // The writer always publishes four equal fields.
                if (v.heart_rate != v.spo2 || v.spo2 != v.resp_rate || v.resp_rate != v.timestamp) torn++;
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
    while (i < 300000 || !all_read_enough()) {
        ++i;
        b.publish({i, i, i, i});
        if (i % 1024 == 0) std::this_thread::yield();
    }
    stop = true;
    for (auto& t : readers) t.join();
    CHECK_EQ(torn.load(), 0);
    auto last = b.read();
    CHECK_EQ(last.timestamp, i);
}

TEST_MAIN()
