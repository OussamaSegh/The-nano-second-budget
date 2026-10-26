// llds/common/cacheline.hpp
// Cache-line constants and padding helpers used throughout the book.
#pragma once

#include <cstddef>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace llds {

// std::hardware_destructive_interference_size is the portable answer, but
// GCC warns that its value may differ between translation units built with
// different -mtune flags. We pin it to 64 bytes, which is correct for every
// mainstream x86-64 and most ARMv8 server parts. Apple M-series and some
// Neoverse cores use 128-byte lines for the purposes of false sharing
// (adjacent-line prefetch), so we expose a second constant for that case.
inline constexpr std::size_t kCacheLine = 64;
inline constexpr std::size_t kFalseSharingRange = 128;

// A value padded and aligned so that it owns its cache line(s) exclusively.
template <class T, std::size_t Align = kFalseSharingRange>
struct alignas(Align) Padded {
    T value;

    Padded() = default;
    template <class... Args>
    explicit Padded(std::in_place_t, Args&&... args)
        : value(std::forward<Args>(args)...) {}

    T& operator*() noexcept { return value; }
    const T& operator*() const noexcept { return value; }
    T* operator->() noexcept { return &value; }
    const T* operator->() const noexcept { return &value; }
    // No explicit padding member is needed: sizeof of an over-aligned type is
    // always rounded up to a multiple of its alignment, so the tail of the
    // last line cannot be shared with a neighbour.
};

static_assert(sizeof(Padded<int>) == kFalseSharingRange);
static_assert(alignof(Padded<int>) == kFalseSharingRange);

// Round n up to the next power of two (n >= 1). Throws std::length_error if
// the result does not fit in size_t: without the check, p <<= 1 wraps to 0
// for n > 2^63 and the loop never ends. Capacities often come from
// configuration files, so the check is not academic.
constexpr std::size_t next_pow2(std::size_t n) {
    constexpr std::size_t top = std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
    if (n > top) throw std::length_error("llds::next_pow2: capacity too large");
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

constexpr bool is_pow2(std::size_t n) noexcept { return n && !(n & (n - 1)); }

}  // namespace llds
