// Exercise 18.1: a threshold alarm with persistence and hysteresis.
//
// Implement update() for the alarm evaluator of the Chapter 18 case study:
//   * Raise when the value is beyond `trigger` (above or below, per `dir`)
//     continuously for at least `persist_ns`. A single in-range sample
//     restarts the timer. (This suppresses alarms on momentary artefacts.)
//   * Once raised, clear only when the value recovers past `clear_at`
//     (hysteresis). Values between clear_at and trigger keep it active.
//   * Return Raised / Cleared on the sample that causes the transition,
//     None otherwise.
//   * update() runs on the real-time path: no allocation, no locks.
#pragma once

#include <cstdint>

class ThresholdAlarm {
public:
    enum class Direction { Above, Below };
    enum class Change { None, Raised, Cleared };

    ThresholdAlarm(Direction dir, std::int32_t trigger, std::int32_t clear_at, std::uint64_t persist_ns)
        : dir_(dir), trigger_(trigger), clear_(clear_at), persist_(persist_ns) {}

    Change update(std::int32_t value, std::uint64_t t_ns) noexcept {
        (void)value;
        (void)t_ns;
        return Change::None;  // TODO
    }

    bool active() const noexcept { return active_; }

private:
    Direction dir_;
    std::int32_t trigger_, clear_;
    std::uint64_t persist_;
    bool active_ = false;
    bool pending_ = false;
    std::uint64_t since_ = 0;
};
