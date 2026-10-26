// llds/pet/hit.hpp
// A synthetic detector hit and its wire format (Chapter 19).
//
// This is a teaching format, not any real scanner's protocol. It has the
// properties that matter for the software around it: a fixed size, explicit
// little-endian fields, a per-source sequence number for loss detection, and
// fields whose valid ranges the decoder checks.
//
//   offset  size  field
//        0     8  tick     timestamp in picoseconds, already extended to a
//                          monotonic 64-bit count by the front end
//        8     4  seq      per-module sequence number, wraps at 2^32
//       12     2  module   detector module, 0 .. n_modules - 1
//       14     2  energy   raw ADC value (uncalibrated)
//
// The decoder never casts the byte buffer to Hit*: the in-memory struct may
// have a different layout, alignment and byte order from the wire.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace llds::pet {

struct Hit {
    std::uint64_t tick = 0;    // picoseconds
    std::uint32_t seq = 0;     // per-module sequence number
    std::uint16_t module = 0;  // detector module
    std::uint16_t energy = 0;  // raw ADC units

    friend bool operator==(const Hit&, const Hit&) = default;
};
static_assert(sizeof(Hit) == 16);

inline constexpr std::size_t kHitWireSize = 16;

enum class DecodeStatus : std::uint8_t { ok, bad_length, bad_module };

namespace detail {
template <class U>
U load_le(std::span<const std::byte> b, std::size_t at) noexcept {
    U v = 0;
    for (std::size_t i = 0; i < sizeof(U); ++i)
        v |= static_cast<U>(static_cast<U>(std::to_integer<unsigned>(b[at + i])) << (8 * i));
    return v;
}
template <class U>
void store_le(std::span<std::byte> b, std::size_t at, U v) noexcept {
    for (std::size_t i = 0; i < sizeof(U); ++i) b[at + i] = static_cast<std::byte>((v >> (8 * i)) & 0xff);
}
}  // namespace detail

// Decode one record. `out` is written only when the result is ok, so a
// rejected record can never be mistaken for a valid hit on module 0.
inline DecodeStatus decode_hit(std::span<const std::byte> in, std::uint16_t n_modules, Hit& out) noexcept {
    if (in.size() != kHitWireSize) return DecodeStatus::bad_length;
    Hit h;
    h.tick = detail::load_le<std::uint64_t>(in, 0);
    h.seq = detail::load_le<std::uint32_t>(in, 8);
    h.module = detail::load_le<std::uint16_t>(in, 12);
    h.energy = detail::load_le<std::uint16_t>(in, 14);
    if (h.module >= n_modules) return DecodeStatus::bad_module;
    out = h;
    return DecodeStatus::ok;
}

inline std::array<std::byte, kHitWireSize> encode_hit(const Hit& h) noexcept {
    std::array<std::byte, kHitWireSize> b{};
    detail::store_le<std::uint64_t>(b, 0, h.tick);
    detail::store_le<std::uint32_t>(b, 8, h.seq);
    detail::store_le<std::uint16_t>(b, 12, h.module);
    detail::store_le<std::uint16_t>(b, 14, h.energy);
    return b;
}

// Distance between two timestamps without unsigned wrap-around:
// subtract the smaller from the larger, never abs(a - b).
constexpr std::uint64_t tick_distance(std::uint64_t a, std::uint64_t b) noexcept {
    return a > b ? a - b : b - a;
}

}  // namespace llds::pet
