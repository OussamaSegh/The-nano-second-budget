// Exercise 21.1: a parallel histogram with private bins.
//
// Count coincidences per unordered module pair: bin pair_bin(c) is
// min*modules + max of the two modules (provided). Split the input among
// `threads` workers. Each worker must count into its *own* histogram; merge
// them after join(), in a fixed order. No atomics, no mutex, and no two
// workers writing the same cache line.
//
// Throw std::invalid_argument if threads or modules is 0, or if any
// coincidence has a module >= modules. Check this before starting threads:
// an exception escaping a std::thread terminates the program.
#pragma once

#include <llds/common/cacheline.hpp>
#include <llds/pet/coincidence.hpp>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

inline std::size_t pair_bin(const llds::pet::Coincidence& c, std::size_t modules) noexcept {
    const std::size_t a = c.first.module, b = c.second.module;
    return a < b ? a * modules + b : b * modules + a;
}

inline std::vector<std::uint64_t> pair_histogram(std::span<const llds::pet::Coincidence> in, std::size_t modules,
                                                 unsigned threads) {
    // TODO
    (void)in;
    (void)threads;
    return std::vector<std::uint64_t>(modules * modules, 0);
}
