// Exercise 3.1 (solution): a one-slot SPSC mailbox.
#pragma once

#include <atomic>
#include <type_traits>

template <class T>
class Mailbox {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    // Producer only.
    bool try_put(const T& v) noexcept {
        // acquire: pairs with the consumer's release in try_take, so our
        // write to value_ cannot race with the consumer's read of the
        // previous message.
        if (full_.load(std::memory_order_acquire)) return false;
        value_ = v;
        // release: makes the write to value_ visible before `full`.
        full_.store(true, std::memory_order_release);
        return true;
    }

    // Consumer only.
    bool try_take(T& out) noexcept {
        if (!full_.load(std::memory_order_acquire)) return false;  // pairs with put's release
        out = value_;
        full_.store(false, std::memory_order_release);  // hand the slot back
        return true;
    }

private:
    T value_{};
    std::atomic<bool> full_{false};
};
