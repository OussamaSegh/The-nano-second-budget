// tests/check.hpp -- a dependency-free test helper used by all tests.
//
// Each test is a function registered with TEST(name). CHECK(cond) records a
// failure and continues; REQUIRE(cond) stops the current test.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace check {

struct Test {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Test>& registry() {
    static std::vector<Test> r;
    return r;
}

inline int& failures() {
    static int f = 0;
    return f;
}

struct Register {
    Register(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

struct Abort {};

inline int run_all(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0;
    for (auto& t : registry()) {
        if (filter && std::string(t.name).find(filter) == std::string::npos) continue;
        const int before = failures();
        std::printf("[ RUN  ] %s\n", t.name);
        try {
            t.fn();
        } catch (Abort&) {
        }
        std::printf("[ %s ] %s\n", failures() == before ? " OK " : "FAIL", t.name);
        ++ran;
    }
    std::printf("%d test(s), %d failure(s)\n", ran, failures());
    return failures() ? EXIT_FAILURE : EXIT_SUCCESS;
}

}  // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
#define TEST(name)                                                              \
    static void CHECK_CAT(test_fn_, __LINE__)();                                \
    static ::check::Register CHECK_CAT(test_reg_, __LINE__)(name, CHECK_CAT(test_fn_, __LINE__)); \
    static void CHECK_CAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
            ++::check::failures();                                              \
        }                                                                       \
    } while (0)

#define REQUIRE(cond)                                                           \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("  %s:%d: REQUIRE(%s) failed\n", __FILE__, __LINE__, #cond); \
            ++::check::failures();                                              \
            throw ::check::Abort{};                                             \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

#define TEST_MAIN() \
    int main(int argc, char** argv) { return ::check::run_all(argc, argv); }
