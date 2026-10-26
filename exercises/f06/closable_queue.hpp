// Exercise F6.1: a bounded blocking queue that can be closed.
//
// Contract:
//   * push(v) blocks while the queue holds `capacity` items. It returns
//     false, without enqueuing, if the queue is closed (including when the
//     close happens while it is blocked).
//   * pop() blocks while the queue is empty and open. It returns the oldest
//     item, or nullopt only when the queue is closed *and* empty, so items
//     pushed before close() are still delivered.
//   * close() is idempotent and wakes every blocked producer and consumer.
// Use one std::mutex and two std::condition_variables, and always wait with
// a predicate.
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>

template <class T>
class ClosableQueue {
public:
    explicit ClosableQueue(std::size_t capacity) : cap_(capacity) {}

    bool push(T v) {
        // TODO
        (void)v;
        return false;
    }

    std::optional<T> pop() {
        // TODO
        return std::nullopt;
    }

    void close() {
        // TODO
    }

private:
    std::mutex m_;
    std::condition_variable not_empty_, not_full_;
    std::deque<T> q_;
    const std::size_t cap_;
    bool closed_ = false;
};
