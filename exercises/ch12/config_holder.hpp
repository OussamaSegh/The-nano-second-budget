// Exercise 12.1: a hot-swappable configuration object.
//
// Readers call read(f) millions of times per second; a control thread calls
// update() a few times per minute. Requirements:
//   * read() is lock-free and never blocks on a writer;
//   * a reader that is inside f(const T&) must never see the object freed;
//   * old versions are freed eventually (no leak), using
//     llds::HazardDomain (protect + retire);
//   * modify(f) builds the next version from the current one; concurrent
//     modify() calls must not lose updates (they may serialize).
//
// The starter "works" in single-threaded tests but frees the old object
// while readers may still use it. Fix it.
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#include <llds/reclaim/hazard_pointers.hpp>

template <class T>
class ConfigHolder {
public:
    explicit ConfigHolder(T initial) : current_(new T(std::move(initial))) {}
    ~ConfigHolder() { delete current_.load(std::memory_order_relaxed); }

    ConfigHolder(const ConfigHolder&) = delete;
    ConfigHolder& operator=(const ConfigHolder&) = delete;

    template <class F>
    decltype(auto) read(F&& f) const {
        // TODO: nothing stops update() from freeing *p while f runs.
        const T* p = current_.load(std::memory_order_acquire);
        return std::forward<F>(f)(*p);
    }

    void update(T next) {
        T* old = current_.exchange(new T(std::move(next)), std::memory_order_acq_rel);
        delete old;  // TODO: use-after-free for concurrent readers
    }

    template <class F>
    void modify(F&& f) {
        // TODO: two concurrent modify() calls can lose an update.
        T next = *current_.load(std::memory_order_acquire);
        f(next);
        update(std::move(next));
    }

private:
    mutable std::atomic<T*> current_;
    std::mutex writer_;
};
