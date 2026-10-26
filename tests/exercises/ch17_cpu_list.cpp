#include <cpu_list.hpp>

#include "check.hpp"

static bool throws(std::string_view s) {
    try {
        parse_cpu_list(s);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

TEST("ex17.1: valid lists") {
    CHECK((parse_cpu_list("0-3,8,10-11") == std::vector<int>{0, 1, 2, 3, 8, 10, 11}));
    CHECK((parse_cpu_list("5") == std::vector<int>{5}));
    CHECK((parse_cpu_list("3,1,2,1") == std::vector<int>{1, 2, 3}));
    CHECK((parse_cpu_list("2-4\n") == std::vector<int>{2, 3, 4}));
    CHECK(parse_cpu_list("").empty());
    CHECK(parse_cpu_list("\n").empty());
}

TEST("ex17.1: malformed lists are rejected") {
    CHECK(throws("1,,2"));
    CHECK(throws("a"));
    CHECK(throws("4-2"));
    CHECK(throws("1-"));
    CHECK(throws("-1"));
    CHECK(throws("99999"));
    CHECK(throws("1, 2"));
}

TEST_MAIN()
