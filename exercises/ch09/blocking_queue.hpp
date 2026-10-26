// Exercise 9.1: a blocking MPMC queue with a spin-then-wait strategy.
//
// Wrap llds::MpmcBoundedQueue so that:
//   * push() blocks while the queue is full, pop() blocks while it is empty;
//   * each side first spins for a short while (the other side is often
//     about to act), then sleeps in the kernel using C++20
//     std::atomic<T>::wait / notify_one / notify_all;
//   * the uncontended path (queue neither full nor empty) makes no system
//     call: only notify when someone may be waiting;
//   * no lost wake-ups: a sleeper must not miss the push/pop it waits for.
#pragma once

#include <atomic>
#include <cstdint>
#include <utility>

#include <llds/common/cpu.hpp>
#include <llds/queue/mpmc_bounded.hpp>

template <class T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : q_(capacity) {}

    void push(T v) {
        // TODO: correct, but burns a whole core while the queue is full.
        while (!q_.try_push(std::move(v))) llds::cpu_relax();
    }

    T pop() {
        // TODO: this is correct but burns a whole core while waiting.
        T v;
        while (!q_.try_pop(v)) llds::cpu_relax();
        return v;
    }

    bool try_pop(T& v) { return q_.try_pop(v); }

private:
    llds::MpmcBoundedQueue<T> q_;
};
