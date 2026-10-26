// Exercise F4.1 (solution): little-endian fields and a checked calibration.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// Read an unsigned little-endian integer of type U at byte offset `at`.
// Returns nullopt, without reading, if the field does not fit in `in`.
template <class U>
std::optional<U> read_le(std::span<const std::byte> in, std::size_t at) noexcept {
    // Written as two comparisons so that at + sizeof(U) cannot overflow.
    if (at > in.size() || in.size() - at < sizeof(U)) return std::nullopt;
    U v = 0;
    for (std::size_t i = 0; i < sizeof(U); ++i)
        // Widen each byte to U *before* shifting: shifting an int by 56
        // would be undefined behaviour.
        v |= static_cast<U>(static_cast<U>(std::to_integer<unsigned>(in[at + i])) << (8 * i));
    return v;
}

// raw * gain + offset, or nullopt if any input is not finite, the gain is not
// strictly positive, or the result overflows to infinity. NaN fails every
// comparison, so the checks use isfinite rather than range tests.
inline std::optional<double> checked_calibrate(double raw, double gain, double offset) noexcept {
    if (!std::isfinite(raw) || !std::isfinite(gain) || !std::isfinite(offset) || !(gain > 0)) return std::nullopt;
    const double e = raw * gain + offset;
    if (!std::isfinite(e)) return std::nullopt;
    return e;
}
