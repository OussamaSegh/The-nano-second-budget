// Exercise 5.1: a reader-writer spinlock.
//
// Requirements:
//   * lock()/unlock(): exclusive (writer) access.
//   * lock_shared()/unlock_shared(): any number of concurrent readers.
//   * try_lock(), try_lock_shared(): never spin.
//   * Writer preference: once a writer is waiting, new readers must wait,
//     so a steady stream of readers cannot starve writers.
//   * A single std::atomic<std::uint32_t> of state is enough.
//
// The class satisfies the SharedLockable requirements, so it works with
// std::shared_lock and std::unique_lock.
#pragma once

#include <atomic>
#include <cstdint>

#include <llds/common/cpu.hpp>

class RwSpinLock {
public:
    void lock() noexcept { /* TODO */ }
    bool try_lock() noexcept { return false; /* TODO */ }
    void unlock() noexcept { /* TODO */ }

    void lock_shared() noexcept { /* TODO */ }
    bool try_lock_shared() noexcept { return false; /* TODO */ }
    void unlock_shared() noexcept { /* TODO */ }

private:
    std::atomic<std::uint32_t> state_{0};
};
