// Exercise F6.1 (solution): a bounded blocking queue that can be closed.
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
    explicit ClosableQueue(std::size_t capacity) : cap_(capacity) {
        if (capacity == 0) throw std::invalid_argument("ClosableQueue: capacity must be >= 1");
    }

    // Blocks while full. Returns false if the queue is (or becomes) closed.
    bool push(T v) {
        std::unique_lock lock(m_);
        // The predicate is re-checked under the lock after every wake-up,
        // which handles spurious wake-ups and lost races alike.
        not_full_.wait(lock, [&] { return closed_ || q_.size() < cap_; });
        if (closed_) return false;
        q_.push_back(std::move(v));
        lock.unlock();  // notify without the lock: the woken thread need not wait for it
        not_empty_.notify_one();
        return true;
    }

    // Blocks while empty and open. Returns nullopt only when closed *and*
    // empty: items accepted before close() are still delivered (drain).
    std::optional<T> pop() {
        std::unique_lock lock(m_);
        not_empty_.wait(lock, [&] { return closed_ || !q_.empty(); });
        if (q_.empty()) return std::nullopt;
        std::optional<T> v(std::move(q_.front()));
        q_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return v;
    }

    // Idempotent. Wakes *every* waiter on *both* sides: a producer blocked
    // on a full queue must learn about the close too.
    void close() {
        {
            std::lock_guard lock(m_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable not_empty_, not_full_;
    std::deque<T> q_;  // bounded in count, but may allocate: a correctness baseline
    const std::size_t cap_;
    bool closed_ = false;
};
