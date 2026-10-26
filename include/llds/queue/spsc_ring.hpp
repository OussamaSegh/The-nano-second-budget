// llds/queue/spsc_ring.hpp
// Bounded single-producer single-consumer ring buffer (Chapter 8).
//
// Design choices, each worth measuring (examples/ch08_spsc_steps.cpp walks
// through them one at a time):
//   1. Power-of-two capacity, so the index wrap is a mask, not a modulo.
//   2. Monotonic 64-bit head/tail counters; never wrap, never ambiguous
//      between "full" and "empty".
//   3. head_ and tail_ live on separate cache lines (no false sharing).
//   4. Each side caches the other side's index and only re-reads the shared
//      atomic when the cached value says "full" or "empty". In steady state
//      this removes almost all cross-core traffic on the index lines.
//      (Each side reads its *own* index with a relaxed load of the atomic:
//      it is the only writer, so the value is always current. Keeping a
//      second, private copy of it as well measured slower, not faster.)
//   5. acquire/release only; no seq_cst, no RMW instructions at all.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#include "../common/cacheline.hpp"

namespace llds {

template <class T>
class SpscRing {
public:
    explicit SpscRing(std::size_t capacity)
        : cap_(next_pow2(capacity < 2 ? 2 : capacity)),
          mask_(cap_ - 1),
          slots_(static_cast<Slot*>(::operator new[](cap_ * sizeof(Slot), std::align_val_t{alignof(Slot)}))) {}

    ~SpscRing() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            while (front()) pop_front();
        }
        ::operator delete[](slots_, std::align_val_t{alignof(Slot)});
    }

    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;

    std::size_t capacity() const noexcept { return cap_; }

    // ---- producer side -------------------------------------------------
    template <class... Args>
    bool try_emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        const auto t = tail_.load(std::memory_order_relaxed);  // we are the only writer
        if (t - prod_.head_cache == cap_) {
            prod_.head_cache = head_.load(std::memory_order_acquire);
            if (t - prod_.head_cache == cap_) return false;  // really full
        }
        new (&slots_[t & mask_].storage) T(std::forward<Args>(args)...);
        tail_.store(t + 1, std::memory_order_release);  // publish the element
        return true;
    }

    bool try_push(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) { return try_emplace(v); }
    bool try_push(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) { return try_emplace(std::move(v)); }

    // ---- consumer side -------------------------------------------------
    bool try_pop(T& out) noexcept(std::is_nothrow_move_assignable_v<T>) {
        T* p = front();
        if (!p) return false;
        out = std::move(*p);
        pop_front();
        return true;
    }

    std::optional<T> try_pop() {
        T* p = front();
        if (!p) return std::nullopt;
        std::optional<T> out(std::move(*p));
        pop_front();
        return out;
    }

    // Zero-copy consumer API: peek at the oldest element, then release it.
    T* front() noexcept {
        const auto h = head_.load(std::memory_order_relaxed);  // we are the only writer
        if (h == cons_.tail_cache) {
            cons_.tail_cache = tail_.load(std::memory_order_acquire);
            if (h == cons_.tail_cache) return nullptr;  // really empty
        }
        return std::launder(reinterpret_cast<T*>(&slots_[h & mask_].storage));
    }

    void pop_front() noexcept {
        const auto h = head_.load(std::memory_order_relaxed);
        std::launder(reinterpret_cast<T*>(&slots_[h & mask_].storage))->~T();
        head_.store(h + 1, std::memory_order_release);  // hand the slot back
    }

    // Approximate; exact only when called from one side with the other idle.
    std::size_t size_approx() const noexcept {
        return static_cast<std::size_t>(tail_.load(std::memory_order_acquire) -
                                        head_.load(std::memory_order_acquire));
    }
    bool empty_approx() const noexcept { return size_approx() == 0; }

private:
    struct Slot {
        alignas(T) unsigned char storage[sizeof(T)];
    };

    // Shared, written by the consumer.
    alignas(kFalseSharingRange) std::atomic<std::uint64_t> head_{0};
    // Shared, written by the producer.
    alignas(kFalseSharingRange) std::atomic<std::uint64_t> tail_{0};

    // Producer-private cache of head_ (own cache line).
    struct alignas(kFalseSharingRange) ProducerState {
        std::uint64_t head_cache = 0;
    } prod_;

    // Consumer-private cache of tail_ (own cache line).
    struct alignas(kFalseSharingRange) ConsumerState {
        std::uint64_t tail_cache = 0;
    } cons_;

    // Read-only after construction; may share a line with each other.
    alignas(kFalseSharingRange) const std::size_t cap_;
    const std::size_t mask_;
    Slot* const slots_;
};

}  // namespace llds
