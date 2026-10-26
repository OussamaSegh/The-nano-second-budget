// Exercise 21.1 (solution): a parallel histogram with private bins.
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
    if (threads == 0 || modules == 0) throw std::invalid_argument("pair_histogram: threads and modules must be >= 1");
    for (const auto& c : in)
        if (c.first.module >= modules || c.second.module >= modules)
            throw std::invalid_argument("pair_histogram: module out of range");  // checked before any thread starts
    const std::size_t bins = modules * modules;
    // One private histogram per worker. Each starts on its own cache line
    // (Padded), so workers never write to a line another worker writes.
    std::vector<llds::Padded<std::vector<std::uint64_t>>> priv(threads);
    for (auto& p : priv) p.value.assign(bins, 0);
    std::vector<std::thread> ws;
    const std::size_t chunk = (in.size() + threads - 1) / threads;
    for (unsigned t = 0; t < threads; ++t) {
        const std::size_t begin = std::min(in.size(), t * chunk), end = std::min(in.size(), begin + chunk);
        ws.emplace_back([&, t, begin, end] {
            auto& h = priv[t].value;  // only this thread touches it until join
            for (std::size_t i = begin; i < end; ++i) ++h[pair_bin(in[i], modules)];
        });
    }
    for (auto& w : ws) w.join();
    // Merge in a fixed order. For integers any order gives the same result;
    // for floating-point weights a fixed order is what makes it reproducible.
    std::vector<std::uint64_t> out(bins, 0);
    for (const auto& p : priv)
        for (std::size_t b = 0; b < bins; ++b) out[b] += p.value[b];
    return out;
}
