#include <simple_skiplist.hpp>

#include <random>
#include <set>

#include "check.hpp"

TEST("ex14.1: basic operations") {
    SimpleSkipList<int> s;
    CHECK(s.insert(10));
    CHECK(s.insert(30));
    CHECK(s.insert(20));
    CHECK(!s.insert(20));
    CHECK(s.contains(20));
    CHECK(!s.contains(25));
    CHECK_EQ(*s.lower_bound(25), 30);
    CHECK_EQ(*s.lower_bound(10), 10);
    CHECK(!s.lower_bound(31));
    CHECK(s.erase(20));
    CHECK(!s.erase(20));
    CHECK_EQ(s.size(), 2u);
}

TEST("ex14.1: agrees with std::set on a random workload") {
    SimpleSkipList<int> s;
    std::set<int> ref;
    std::mt19937 rng(9);
    bool ok = true;
    for (int i = 0; i < 100000 && ok; ++i) {
        int k = static_cast<int>(rng() % 5000);
        switch (rng() % 4) {
            case 0: ok = s.insert(k) == ref.insert(k).second; break;
            case 1: ok = s.erase(k) == (ref.erase(k) == 1); break;
            case 2: ok = s.contains(k) == (ref.count(k) == 1); break;
            default: {
                auto it = ref.lower_bound(k);
                auto lb = s.lower_bound(k);
                ok = (it == ref.end()) ? !lb.has_value() : (lb && *lb == *it);
            }
        }
    }
    CHECK(ok);
    CHECK_EQ(s.size(), ref.size());
}

TEST_MAIN()
