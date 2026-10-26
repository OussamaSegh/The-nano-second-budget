// Exercise 20.1 (solution): single-window coincidence grouping.
#pragma once

#include <llds/pet/coincidence.hpp>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

enum class Multiples { kill_all, take_all_pairs };

struct SingleWindowResult {
    std::vector<llds::pet::Coincidence> pairs;
    std::uint64_t singles = 0;      // groups of one hit
    std::uint64_t same_module = 0;  // groups of two on one module
    std::uint64_t multiples = 0;    // groups of three or more
};

inline SingleWindowResult single_window(std::span<const llds::pet::Hit> hits, std::uint64_t window, Multiples policy) {
    SingleWindowResult r;
    for (std::size_t j = 1; j < hits.size(); ++j)
        if (hits[j].tick < hits[j - 1].tick) throw std::invalid_argument("single_window: input not sorted");
    std::size_t i = 0;
    while (i < hits.size()) {
        // The opening hit fixes the window; it does not slide. Every hit within
        // `window` of the *opening* hit joins the group, however close it is
        // to the group's last member.
        std::size_t end = i + 1;
        while (end < hits.size() && hits[end].tick - hits[i].tick <= window) ++end;
        const std::size_t size = end - i;
        if (size == 1) {
            ++r.singles;
        } else if (size == 2) {
            if (hits[i].module != hits[i + 1].module) r.pairs.push_back({hits[i], hits[i + 1]});
            else ++r.same_module;
        } else {
            ++r.multiples;
            if (policy == Multiples::take_all_pairs)
                for (std::size_t b = i + 1; b < end; ++b)
                    for (std::size_t a = i; a < b; ++a)
                        if (hits[a].module != hits[b].module) r.pairs.push_back({hits[a], hits[b]});
        }
        i = end;  // the next window opens at the first hit after this group
    }
    return r;
}
