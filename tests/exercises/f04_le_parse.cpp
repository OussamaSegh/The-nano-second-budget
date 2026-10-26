#include <le_parse.hpp>

#include <array>
#include <limits>

#include "check.hpp"

TEST("exF4.1: little-endian values") {
    std::array<std::byte, 10> b{};
    for (std::size_t i = 0; i < b.size(); ++i) b[i] = static_cast<std::byte>(i + 1);  // 01 02 03 ...
    auto v16 = read_le<std::uint16_t>(b, 0);
    REQUIRE(v16.has_value());
    CHECK_EQ(*v16, 0x0201u);  // 1 + 2*256 = 513
    CHECK_EQ(read_le<std::uint32_t>(b, 1).value_or(0), 0x05040302u);
    CHECK_EQ(read_le<std::uint64_t>(b, 2).value_or(0), 0x0a09080706050403ull);
    std::array<std::byte, 8> ff;
    ff.fill(std::byte{0xff});
    CHECK_EQ(read_le<std::uint64_t>(ff, 0).value_or(0), std::numeric_limits<std::uint64_t>::max());
}

TEST("exF4.1: short buffers are rejected, not overrun") {
    std::array<std::byte, 4> b{};
    CHECK(read_le<std::uint32_t>(b, 0).has_value());
    CHECK(!read_le<std::uint32_t>(b, 1).has_value());
    CHECK(!read_le<std::uint16_t>(b, 4).has_value());
    CHECK(!read_le<std::uint16_t>(b, 5).has_value());
    CHECK(!read_le<std::uint16_t>(b, std::numeric_limits<std::size_t>::max()).has_value());  // at + 2 wraps
    CHECK(!read_le<std::uint64_t>(std::span<const std::byte>{}, 0).has_value());
}

TEST("exF4.1: checked calibration") {
    CHECK(checked_calibrate(100, 2, -1).value_or(0) == 199.0);
    CHECK(checked_calibrate(0, 1, -5).value_or(0) == -5.0);  // negative is allowed by this contract
    CHECK(!checked_calibrate(1, 0, 0));
    CHECK(!checked_calibrate(1, -1, 0));
    CHECK(!checked_calibrate(std::numeric_limits<double>::quiet_NaN(), 1, 0));
    CHECK(!checked_calibrate(1, std::numeric_limits<double>::quiet_NaN(), 0));
    CHECK(!checked_calibrate(1, 1, std::numeric_limits<double>::infinity()));
    CHECK(!checked_calibrate(std::numeric_limits<double>::max(), 2, 0));  // overflows to inf
}

TEST_MAIN()
