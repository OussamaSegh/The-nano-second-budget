// Exercise F4.1: little-endian fields and a checked calibration.
//
// read_le<U>(in, at): read an unsigned little-endian integer of type U
// (uint16_t, uint32_t or uint64_t) starting at byte `at`. Return nullopt,
// without reading out of bounds, if the field does not fit. Do not cast the
// byte pointer to U*; build the value from the bytes. Watch for at + size
// overflowing, and for shifting a narrow type by too much.
//
// checked_calibrate(raw, gain, offset): return raw * gain + offset, or
// nullopt if any input is not finite, the gain is not strictly positive, or
// the result is not finite.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

template <class U>
std::optional<U> read_le(std::span<const std::byte> in, std::size_t at) noexcept {
    // TODO
    (void)in;
    (void)at;
    return std::nullopt;
}

inline std::optional<double> checked_calibrate(double raw, double gain, double offset) noexcept {
    // TODO
    (void)raw;
    (void)gain;
    (void)offset;
    return std::nullopt;
}
