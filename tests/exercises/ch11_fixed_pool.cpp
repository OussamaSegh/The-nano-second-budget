#include <fixed_pool.hpp>

#include <set>
#include <stdexcept>
#include <string>

#include "check.hpp"

struct Thrower {
    explicit Thrower(bool t) {
        if (t) throw std::runtime_error("boom");
    }
};

TEST("ex11.1: allocate until exhausted, then reuse") {
    FixedPool<std::string> p(3);
    auto* a = p.create("alpha");
    auto* b = p.create("beta");
    auto* c = p.create("gamma");
    REQUIRE(a && b && c);
    CHECK(*b == "beta");
    CHECK_EQ(p.in_use(), 3u);
    CHECK(p.create("delta") == nullptr);
    std::set<void*> distinct{a, b, c};
    CHECK_EQ(distinct.size(), 3u);
    p.destroy(b);
    CHECK_EQ(p.in_use(), 2u);
    auto* d = p.create("delta");
    CHECK(static_cast<void*>(d) == static_cast<void*>(b));
    p.destroy(a);
    p.destroy(c);
    p.destroy(d);
    CHECK_EQ(p.in_use(), 0u);
}

TEST("ex11.1: a throwing constructor does not leak the slot") {
    FixedPool<Thrower> p(1);
    bool threw = false;
    try {
        p.create(true);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
    CHECK_EQ(p.in_use(), 0u);
    auto* t = p.create(false);
    CHECK(t != nullptr);
    p.destroy(t);
}

TEST_MAIN()
