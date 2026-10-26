#include <mailbox.hpp>

#include <thread>

#include "check.hpp"

struct Msg {
    std::uint64_t a, b;
};

TEST("ex3.1: single-threaded semantics") {
    Mailbox<Msg> m;
    Msg out{};
    CHECK(!m.try_take(out));
    REQUIRE(m.try_put({1, 2}));
    CHECK(!m.try_put({3, 4}));
    REQUIRE(m.try_take(out));
    CHECK(out.a == 1 && out.b == 2);
    CHECK(!m.try_take(out));
}

TEST("ex3.1: two threads, every message intact and in order") {
    Mailbox<Msg> m;
    REQUIRE(m.try_put({0, 0}));
    Msg tmp;
    REQUIRE(m.try_take(tmp));
    constexpr std::uint64_t N = 200000;
    std::thread prod([&] {
        for (std::uint64_t i = 1; i <= N; ++i)
            while (!m.try_put({i, ~i})) {}
    });
    bool ok = true;
    for (std::uint64_t i = 1; i <= N; ++i) {
        Msg out;
        while (!m.try_take(out)) {}
        ok = ok && out.a == i && out.b == ~i;
    }
    prod.join();
    CHECK(ok);
}

TEST_MAIN()
