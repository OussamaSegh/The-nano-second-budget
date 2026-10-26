// Exercise 3.1: a one-slot SPSC mailbox.
//
// One producer thread calls try_put, one consumer calls try_take. The slot
// holds at most one message. value_ is a plain (non-atomic) T: the atomic
// flag must order every access to it, so that the program is free of data
// races. Choose the weakest memory orders that are correct and justify
// each one in a comment. Build with -DLLDS_SANITIZE=thread to check.
#pragma once

#include <atomic>
#include <type_traits>

template <class T>
class Mailbox {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    bool try_put(const T& v) noexcept {
        // TODO
        (void)v;
        return false;
    }

    bool try_take(T& out) noexcept {
        // TODO
        (void)out;
        return false;
    }

private:
    T value_{};
    std::atomic<bool> full_{false};
};
