#include <threshold_alarm.hpp>

#include "check.hpp"

using A = ThresholdAlarm;
constexpr std::uint64_t ms = 1'000'000;

TEST("ex18.1: persistence suppresses short excursions") {
    A hr(A::Direction::Above, 130, 120, 100 * ms);
    CHECK(hr.update(140, 0) == A::Change::None);
    CHECK(hr.update(140, 50 * ms) == A::Change::None);
    CHECK(hr.update(110, 60 * ms) == A::Change::None);   // back in range: timer resets
    CHECK(hr.update(140, 70 * ms) == A::Change::None);
    CHECK(hr.update(140, 160 * ms) == A::Change::None);  // only 90 ms since 70
    CHECK(hr.update(140, 170 * ms) == A::Change::Raised);
    CHECK(hr.active());
}

TEST("ex18.1: hysteresis on clearing") {
    A spo2(A::Direction::Below, 880, 900, 0);
    CHECK(spo2.update(870, 0) == A::Change::Raised);
    CHECK(spo2.update(885, 1 * ms) == A::Change::None);  // above trigger but below clear level
    CHECK(spo2.update(895, 2 * ms) == A::Change::None);
    CHECK(spo2.active());
    CHECK(spo2.update(905, 3 * ms) == A::Change::Cleared);
    CHECK(!spo2.active());
    CHECK(spo2.update(890, 4 * ms) == A::Change::None);  // between levels, not active
    CHECK(spo2.update(870, 5 * ms) == A::Change::Raised);
}

TEST_MAIN()
