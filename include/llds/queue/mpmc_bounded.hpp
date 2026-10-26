// llds/queue/mpmc_bounded.hpp
// Dmitry Vyukov's bounded MPMC queue (Chapter 9).
//
// Every slot carries a sequence number. For slot i (position pos, i = pos &
// mask):
//   seq == pos       -> slot is free for the producer that claims `pos`
//   seq == pos + 1   -> slot holds data for the consumer that claims `pos`
// Producers and consumers claim positions with a CAS on a shared counter and
// then work on their slot without touching anyone else's. One CAS per
// operation, no allocation, FIFO per producer.
//
// Progress: not lock-free. A producer preempted between the claim and the
// sequence store makes the queue look empty at that position, so no consumer
// gets past it until the producer resumes, even if later slots are full.
// (Symmetrically, a stalled consumer makes the queue look full to producers
// one lap later.) This is the price of avoiding per-node allocation, and in
// practice is a far better trade than the unbounded queues in ms_queue.hpp.
//
// Exception safety: once a position is claimed it must be resolved, because
// nobody else can release it. A constructor that threw after the claim would
// leave the slot "claimed but never published" and stall the queue for good,
// and so would a move assignment that threw in try_pop. Both operations are
// therefore required to be noexcept (checked at compile time).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "../common/cacheline.hpp"
#include "../common/cpu.hpp"

namespace llds {

template <class T>
class MpmcBoundedQueue {
public:
    explicit MpmcBoundedQueue(std::size_t capacity)
        : cap_(next_pow2(capacity < 2 ? 2 : capacity)), mask_(cap_ - 1),
          cells_(new Cell[cap_]) {
        for (std::size_t i = 0; i < cap_; ++i) cells_[i].seq.store(i, std::memory_order_relaxed);
    }

    ~MpmcBoundedQueue() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            // Single-threaded by now: destroy whatever was pushed but not popped.
            const auto end = enqueue_pos_.load(std::memory_order_relaxed);
            for (auto pos = dequeue_pos_.load(std::memory_order_relaxed); pos != end; ++pos)
                std::launder(reinterpret_cast<T*>(&cells_[pos & mask_].storage))->~T();
        }
    }

    MpmcBoundedQueue(const MpmcBoundedQueue&) = delete;
    MpmcBoundedQueue& operator=(const MpmcBoundedQueue&) = delete;

    std::size_t capacity() const noexcept { return cap_; }

    template <class... Args>
    bool try_emplace(Args&&... args) noexcept {
        static_assert(std::is_nothrow_constructible_v<T, Args...>,
                      "MpmcBoundedQueue: constructing T after claiming a slot must not throw");
        Cell* cell;
        auto pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &cells_[pos & mask_];
            const auto seq = cell->seq.load(std::memory_order_acquire);
            const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;  // slot claimed
            } else if (diff < 0) {
                return false;  // slot still holds an element from one lap ago: full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);  // someone beat us
            }
        }
        new (&cell->storage) T(std::forward<Args>(args)...);
        cell->seq.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool try_push(const T& v) { return try_emplace(v); }
    bool try_push(T&& v) { return try_emplace(std::move(v)); }

    bool try_pop(T& out) noexcept {
        static_assert(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>,
                      "MpmcBoundedQueue: moving T out of a claimed slot must not throw");
        Cell* cell;
        auto pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &cells_[pos & mask_];
            const auto seq = cell->seq.load(std::memory_order_acquire);
            const auto diff =
                static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);
            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return false;  // empty
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
        T* p = std::launder(reinterpret_cast<T*>(&cell->storage));
        out = std::move(*p);
        p->~T();
        cell->seq.store(pos + mask_ + 1, std::memory_order_release);  // free for next lap
        return true;
    }

    // Blocking helpers for tests and examples. They spin; production code
    // would add a wait strategy (Chapter 9, "Waiting well").
    void push(T v) {
        while (!try_push(std::move(v))) cpu_relax();
    }
    T pop() {
        T v;
        while (!try_pop(v)) cpu_relax();
        return v;
    }

private:
    struct alignas(kCacheLine) Cell {
        std::atomic<std::size_t> seq;
        alignas(T) unsigned char storage[sizeof(T)];
    };

    alignas(kFalseSharingRange) std::atomic<std::size_t> enqueue_pos_{0};
    alignas(kFalseSharingRange) std::atomic<std::size_t> dequeue_pos_{0};
    alignas(kFalseSharingRange) const std::size_t cap_;
    const std::size_t mask_;
    std::unique_ptr<Cell[]> cells_;
};

}  // namespace llds
