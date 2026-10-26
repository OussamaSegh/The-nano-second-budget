// Exercise 17.1: parse a Linux CPU list.
//
// Implement parse_cpu_list for the format used by isolcpus=, nohz_full=,
// `taskset -c` and /sys/devices/system/cpu/isolated:
//     "0-3,8,10-11"  ->  {0, 1, 2, 3, 8, 10, 11}
// Rules: comma-separated items, each a number or an inclusive range lo-hi;
// result sorted and de-duplicated; an empty string (or "\n") is an empty
// list; throw std::invalid_argument for anything malformed (empty items,
// non-digits, reversed ranges, numbers > 4095).
#pragma once

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

inline std::vector<int> parse_cpu_list(std::string_view s) {
    (void)s;
    return {};  // TODO
}
