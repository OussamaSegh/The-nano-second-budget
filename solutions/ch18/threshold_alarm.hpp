// Exercise 18.1 (solution): a threshold alarm with persistence and hysteresis.
#pragma once

#include <cstdint>

class ThresholdAlarm {
public:
    enum class Direction { Above, Below };
    enum class Change { None, Raised, Cleared };

    // Raise when the value is beyond `trigger` (above or below, per `dir`)
    // continuously for at least `persist_ns`; clear only when the value
    // recovers past `clear_at`. Requires clear_at to sit on the safe side
    // of trigger (e.g. Above: clear_at < trigger).
    ThresholdAlarm(Direction dir, std::int32_t trigger, std::int32_t clear_at, std::uint64_t persist_ns)
        : dir_(dir), trigger_(trigger), clear_(clear_at), persist_(persist_ns) {}

    Change update(std::int32_t value, std::uint64_t t_ns) noexcept {
        const bool beyond = dir_ == Direction::Above ? value > trigger_ : value < trigger_;
        const bool recovered = dir_ == Direction::Above ? value < clear_ : value > clear_;
        if (!active_) {
            if (!beyond) {
                pending_ = false;  // any good sample restarts the persistence timer
                return Change::None;
            }
            if (!pending_) {
                pending_ = true;
                since_ = t_ns;
            }
            if (t_ns - since_ >= persist_) {
                active_ = true;
                pending_ = false;
                return Change::Raised;
            }
            return Change::None;
        }
        if (recovered) {
            active_ = false;
            return Change::Cleared;
        }
        return Change::None;
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
