// llds/sync/spinlock.hpp
// The spinlock family from Chapter 5. All satisfy BasicLockable, so they work
// with std::lock_guard and std::scoped_lock.
#pragma once

#include <atomic>
#include <cstdint>

#include "../common/cacheline.hpp"
#include "../common/cpu.hpp"

namespace llds {

// 1. Test-and-set: every spin iteration is an atomic RMW, so every waiter
//    keeps stealing the line in exclusive state. Simple and terrible under
//    contention.
class TasLock {
public:
    void lock() noexcept {
        while (flag_.exchange(true, std::memory_order_acquire)) cpu_relax();
    }
    bool try_lock() noexcept { return !flag_.exchange(true, std::memory_order_acquire); }
    void unlock() noexcept { flag_.store(false, std::memory_order_release); }

private:
    std::atomic<bool> flag_{false};
};

// 2. Test-and-test-and-set: spin on a plain load (the line stays Shared in
//    every waiter's cache) and only attempt the RMW when the lock looks free.
class TtasLock {
public:
    void lock() noexcept {
        for (;;) {
            if (!flag_.exchange(true, std::memory_order_acquire)) return;
            while (flag_.load(std::memory_order_relaxed)) cpu_relax();
        }
    }
    bool try_lock() noexcept {
        return !flag_.load(std::memory_order_relaxed) &&
               !flag_.exchange(true, std::memory_order_acquire);
    }
    void unlock() noexcept { flag_.store(false, std::memory_order_release); }

private:
    std::atomic<bool> flag_{false};
};

// 3. TTAS with exponential backoff: after a failed attempt, wait longer before
//    retrying, which spreads out the thundering herd at unlock time.
class BackoffLock {
public:
    void lock() noexcept {
        Backoff backoff;
        for (;;) {
            if (!flag_.load(std::memory_order_relaxed) &&
                !flag_.exchange(true, std::memory_order_acquire))
                return;
            backoff.pause();
        }
    }
    bool try_lock() noexcept {
        return !flag_.load(std::memory_order_relaxed) &&
               !flag_.exchange(true, std::memory_order_acquire);
    }
    void unlock() noexcept { flag_.store(false, std::memory_order_release); }

private:
    std::atomic<bool> flag_{false};
};

// 4. Ticket lock: FIFO fair. A waiter takes a ticket with fetch_add and spins
//    until `serving_` reaches it. All waiters still spin on one line.
class TicketLock {
public:
    void lock() noexcept {
        const auto my = next_.fetch_add(1, std::memory_order_relaxed);
        for (;;) {
            const auto cur = serving_.load(std::memory_order_acquire);
            if (cur == my) return;
            // Proportional backoff: the further back in line, the longer we wait.
            for (std::uint32_t i = 0; i < (my - cur) * 32; ++i) cpu_relax();
        }
    }
    bool try_lock() noexcept {
        auto cur = serving_.load(std::memory_order_relaxed);
        auto expected = cur;
        return next_.compare_exchange_strong(expected, cur + 1, std::memory_order_acquire,
                                             std::memory_order_relaxed);
    }
    void unlock() noexcept {
        // Only the holder writes serving_, so a load+store is enough.
        serving_.store(serving_.load(std::memory_order_relaxed) + 1,
                       std::memory_order_release);
    }

private:
    alignas(kCacheLine) std::atomic<std::uint32_t> next_{0};
    alignas(kCacheLine) std::atomic<std::uint32_t> serving_{0};
};

// 5. MCS queue lock: each waiter spins on its *own* node, so a hand-off
//    touches exactly one remote cache line. Scales on many-socket machines.
//    The node must outlive the critical section; McsLock::Guard handles that.
class McsLock {
public:
    struct alignas(kCacheLine) Node {
        std::atomic<Node*> next{nullptr};
        std::atomic<bool> locked{false};
    };

    void lock(Node& me) noexcept {
        me.next.store(nullptr, std::memory_order_relaxed);
        me.locked.store(true, std::memory_order_relaxed);
        Node* prev = tail_.exchange(&me, std::memory_order_acq_rel);
        if (prev) {
            prev->next.store(&me, std::memory_order_release);
            while (me.locked.load(std::memory_order_acquire)) cpu_relax();
        }
    }

    void unlock(Node& me) noexcept {
        Node* succ = me.next.load(std::memory_order_acquire);
        if (!succ) {
            Node* expected = &me;
            if (tail_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel,
                                              std::memory_order_relaxed))
                return;  // no one was waiting
            // A successor swapped itself into tail_ but has not linked yet.
            while (!(succ = me.next.load(std::memory_order_acquire))) cpu_relax();
        }
        succ->locked.store(false, std::memory_order_release);
    }

    class Guard {
    public:
        explicit Guard(McsLock& l) noexcept : lock_(l) { lock_.lock(node_); }
        ~Guard() { lock_.unlock(node_); }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

    private:
        McsLock& lock_;
        Node node_;
    };

private:
    alignas(kCacheLine) std::atomic<Node*> tail_{nullptr};
};

}  // namespace llds
