// Exercise 17.1 (solution): parse a Linux CPU list ("0-3,8,10-11").
//
// This is the format of isolcpus=, nohz_full=, taskset -c, and
// /sys/devices/system/cpu/isolated. A latency-sensitive service should read
// its core assignment from configuration in this format and refuse to start
// if it is malformed, rather than silently running unpinned.
#pragma once

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

inline std::vector<int> parse_cpu_list(std::string_view s) {
    std::vector<int> cpus;
    auto fail = [&](const char* why) {
        throw std::invalid_argument(std::string("bad cpu list '") + std::string(s) + "': " + why);
    };
    auto parse_int = [&](std::string_view t) {
        if (t.empty()) fail("empty number");
        int v = 0;
        for (char c : t) {
            if (!std::isdigit(static_cast<unsigned char>(c))) fail("not a number");
            v = v * 10 + (c - '0');
            if (v > 4095) fail("cpu number too large");
        }
        return v;
    };
    // Trim one trailing newline, as read from sysfs.
    if (!s.empty() && s.back() == '\n') s.remove_suffix(1);
    if (s.empty()) return cpus;
    std::size_t pos = 0;
    while (pos <= s.size()) {
        const auto comma = s.find(',', pos);
        const auto item = s.substr(pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
        const auto dash = item.find('-');
        if (dash == std::string_view::npos) {
            cpus.push_back(parse_int(item));
        } else {
            const int lo = parse_int(item.substr(0, dash));
            const int hi = parse_int(item.substr(dash + 1));
            if (lo > hi) fail("range is reversed");
            for (int c = lo; c <= hi; ++c) cpus.push_back(c);
        }
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    std::sort(cpus.begin(), cpus.end());
    cpus.erase(std::unique(cpus.begin(), cpus.end()), cpus.end());
    return cpus;
}
