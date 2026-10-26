#include <llds/map/concurrent_int_map.hpp>
#include <llds/map/rcu_map.hpp>
#include <llds/map/striped_hash_map.hpp>
#include <llds/ordered/lazy_skiplist.hpp>
#include <llds/ordered/order_book.hpp>

#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "check.hpp"

using namespace llds;

TEST("striped map: basic") {
    StripedHashMap<std::string, int> m;
    CHECK(m.insert("a", 1));
    CHECK(!m.insert("a", 2));
    CHECK_EQ(*m.find("a"), 1);
    CHECK(!m.insert_or_assign("a", 3));
    CHECK_EQ(*m.find("a"), 3);
    CHECK(m.erase("a"));
    CHECK(!m.find("a"));
}

TEST("striped map: concurrent updates") {
    StripedHashMap<int, long> m;
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] {
            for (int i = 0; i < 20000; ++i) m.update(i % 100, [](long& v) { ++v; });
        });
    for (auto& t : ts) t.join();
    long total = 0;
    for (int k = 0; k < 100; ++k) total += *m.find(k);
    CHECK_EQ(total, 80000);
    CHECK_EQ(m.size(), 100u);
}

TEST("int map: put/find/erase/insert") {
    ConcurrentIntMap m(64);
    CHECK(m.put(5, 50));
    CHECK_EQ(*m.find(5), 50u);
    CHECK(!m.insert(5, 51));
    CHECK(m.erase(5));
    CHECK(!m.find(5));
    CHECK(m.insert(5, 52));
    CHECK_EQ(*m.find(5), 52u);
    CHECK(!m.find(6));
}

TEST("int map: full table") {
    ConcurrentIntMap m(4);
    for (std::uint64_t k = 1; k <= 4; ++k) CHECK(m.put(k, k));
    CHECK(!m.put(99, 1));
}

TEST("int map: concurrent inserts of overlapping keys") {
    ConcurrentIntMap m(1 << 14);
    std::atomic<int> inserted{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] {
            for (std::uint64_t k = 1; k <= 4000; ++k)
                if (m.insert(k, k * 10)) inserted++;
        });
    for (auto& t : ts) t.join();
    CHECK_EQ(inserted.load(), 4000);
    bool ok = true;
    for (std::uint64_t k = 1; k <= 4000; ++k) ok = ok && m.find(k) == k * 10;
    CHECK(ok);
}

TEST("int map: concurrent fetch_add") {
    ConcurrentIntMap m(256);
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&] {
            for (int i = 0; i < 10000; ++i) m.fetch_add(1 + i % 10, 1);
        });
    for (auto& t : ts) t.join();
    std::uint64_t sum = 0;
    for (std::uint64_t k = 1; k <= 10; ++k) sum += *m.find(k);
    CHECK_EQ(sum, 40000u);
}

TEST("rcu map: readers see a consistent snapshot") {
    RcuMap<int, int> m;
    // Invariant maintained by the writer: m[0] + m[1] == 100.
    m.update([](auto& s) { s[0] = 100; s[1] = 0; });
    std::atomic<bool> stop{false}, bad{false};
    std::vector<std::thread> readers;
    for (int r = 0; r < 3; ++r)
        readers.emplace_back([&] {
            while (!stop.load())
                m.read([&](const auto& s) {
                    if (s.at(0) + s.at(1) != 100) bad = true;
                });
        });
    for (int i = 0; i < 2000; ++i) m.update([&](auto& s) { s[0] = 100 - i % 100; s[1] = i % 100; });
    stop = true;
    for (auto& t : readers) t.join();
    CHECK(!bad.load());
    CHECK(m.find(0).has_value());
}

TEST("rcu map: 1000 short-lived reader threads") {
    RcuMap<int, int> m;
    m.insert_or_assign(1, 7);
    for (int t = 0; t < 1000; ++t) {
        std::thread([&] {
            CHECK_EQ(*m.find(1), 7);
            if (t % 10 == 0) m.insert_or_assign(1, 7);  // retire from short-lived threads too
        }).join();
    }
    CHECK_EQ(*m.find(1), 7);
}

TEST("skiplist: sequential semantics") {
    LazySkipList<int, int> s;
    CHECK(s.insert(5, 50));
    CHECK(s.insert(1, 10));
    CHECK(s.insert(9, 90));
    CHECK(!s.insert(5, 51));
    CHECK(s.contains(5));
    CHECK_EQ(*s.find(9), 90);
    CHECK(s.erase(5));
    CHECK(!s.erase(5));
    CHECK(!s.contains(5));
    std::vector<int> keys;
    s.for_range(0, 100, [&](int k, int) { keys.push_back(k); });
    CHECK(keys == std::vector<int>({1, 9}));
}

TEST("skiplist: concurrent insert/erase matches a model") {
    LazySkipList<int, int> s;
    constexpr int T = 4, Range = 512;
    std::vector<std::thread> ts;
    // Each thread owns keys k with k % T == t, so the final state is
    // predictable while threads still contend on shared predecessors.
    std::vector<std::set<int>> model(T);
    for (int t = 0; t < T; ++t)
        ts.emplace_back([&, t] {
            std::mt19937 rng(t);
            for (int i = 0; i < 20000; ++i) {
                int k = static_cast<int>(rng() % (Range / T)) * T + t;
                if (rng() & 1) {
                    bool a = s.insert(k, k);
                    bool b = model[t].insert(k).second;
                    if (a != b) model[t].insert(-1);  // poison: mismatch
                } else {
                    bool a = s.erase(k);
                    bool b = model[t].erase(k) == 1;
                    if (a != b) model[t].insert(-1);
                }
            }
        });
    for (auto& t : ts) t.join();
    std::set<int> expected;
    for (auto& m : model) expected.insert(m.begin(), m.end());
    CHECK(!expected.count(-1));
    std::set<int> actual;
    s.for_range(0, Range, [&](int k, int) { actual.insert(k); });
    CHECK(actual == expected);
    CHECK_EQ(s.size_approx(), expected.size());
}

TEST("order books agree") {
    MapBook a;
    LadderBook b(10000, 2000);
    std::mt19937 rng(1);
    for (int i = 0; i < 20000; ++i) {
        Side side = (rng() & 1) ? Side::Bid : Side::Ask;
        std::int64_t px = side == Side::Bid ? 10000 + rng() % 900 : 11000 + rng() % 900;
        std::int64_t qty = static_cast<std::int64_t>(rng() % 100) - 40;
        if (qty < 0 && -qty > a.qty_at(side, px)) qty = -a.qty_at(side, px);
        a.apply(side, px, qty);
        b.apply(side, px, qty);
        auto ta = a.top(), tb = b.top();
        if (ta.bid_px != tb.bid_px || ta.ask_px != tb.ask_px || ta.bid_qty != tb.bid_qty ||
            ta.ask_qty != tb.ask_qty) {
            CHECK(false);
            break;
        }
    }
}

TEST_MAIN()
