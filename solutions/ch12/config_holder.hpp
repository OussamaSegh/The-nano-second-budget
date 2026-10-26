// Exercise 12.1 (solution): a hot-swappable configuration object protected
// by hazard pointers.
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

    // Call f(const T&) on the current configuration. The object is
    // guaranteed to stay alive for the duration of the call, even if a
    // writer replaces it concurrently. Lock-free, no reference counting.
    template <class F>
    decltype(auto) read(F&& f) const {
        llds::HazardDomain::Guard hp;
        const T* p = hp.protect(current_);
        return std::forward<F>(f)(*p);
    }

    // Replace the configuration. Writers are serialized by a mutex (writes
    // are rare); readers are never blocked.
    void update(T next) {
        std::lock_guard g(writer_);
        swap_in(std::move(next));
    }

    // Read-modify-write: build the next version from the current one.
    template <class F>
    void modify(F&& f) {
        std::lock_guard g(writer_);
        // No hazard pointer needed: only writer_ holders retire versions,
        // and we hold writer_.
        T next = *current_.load(std::memory_order_acquire);
        f(next);
        swap_in(std::move(next));
    }

private:
    void swap_in(T next) {
        T* old = current_.exchange(new T(std::move(next)), std::memory_order_acq_rel);
        llds::HazardDomain::global().retire(old);
    }

    mutable std::atomic<T*> current_;
    std::mutex writer_;
};
