// llds/map/concurrent_int_map.hpp
// A fixed-capacity lock-free hash map from uint64 keys to uint64 values
// (Chapter 13), in the style of Jeff Preshing's "world's simplest lock-free
// hash table" and Cliff Click's non-blocking hash map.
//
//  * Open addressing with linear probing over one flat array of cells.
//  * A key, once written into a cell, never changes. That single rule makes
//    the probing logic lock-free without any reclamation scheme.
//  * Values are atomics, updated in place with store or CAS.
//  * Erase writes a tombstone value; the key stays (slot is not reused).
//  * find() is wait-free: a bounded scan of plain atomic loads.
//
// Key 0 is reserved for "empty" and value kTombstone for "erased". Debug
// builds assert on both; in release builds a value of kTombstone reads as
// "erased", and put(0, v) corrupts the table (the next key to claim that cell
// inherits v). The map does not resize, so size it for the worst case
// (load factor <= 0.5).
#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>

#include "../common/cacheline.hpp"

namespace llds {

class ConcurrentIntMap {
public:
    static constexpr std::uint64_t kEmptyKey = 0;
    static constexpr std::uint64_t kTombstone = ~std::uint64_t{0};

    explicit ConcurrentIntMap(std::size_t capacity)
        : cap_(next_pow2(capacity)), mask_(cap_ - 1), cells_(new Cell[cap_]) {}

    // Insert or overwrite. Returns false only if the table is full.
    bool put(std::uint64_t key, std::uint64_t value) noexcept {
        assert(key != kEmptyKey && "ConcurrentIntMap: key 0 is reserved");
        assert(value != kTombstone && "ConcurrentIntMap: value ~0 is reserved");
        Cell* c = claim(key);
        if (!c) return false;
        c->value.store(value, std::memory_order_release);
        return true;
    }

    // Insert only if absent (or erased). Returns true if this call inserted.
    bool insert(std::uint64_t key, std::uint64_t value) noexcept {
        assert(key != kEmptyKey && "ConcurrentIntMap: key 0 is reserved");
        assert(value != kTombstone && "ConcurrentIntMap: value ~0 is reserved");
        Cell* c = claim(key);
        if (!c) return false;
        std::uint64_t cur = c->value.load(std::memory_order_acquire);
        while (cur == kTombstone) {
            if (c->value.compare_exchange_weak(cur, value, std::memory_order_acq_rel))
                return true;
        }
        return false;
    }

    std::optional<std::uint64_t> find(std::uint64_t key) const noexcept {
        for (std::size_t i = hash(key), n = 0; n < cap_; i = (i + 1) & mask_, ++n) {
            const auto k = cells_[i].key.load(std::memory_order_acquire);
            if (k == key) {
                const auto v = cells_[i].value.load(std::memory_order_acquire);
                if (v == kTombstone) return std::nullopt;
                return v;
            }
            if (k == kEmptyKey) return std::nullopt;  // probe chain ends here
        }
        return std::nullopt;
    }

    bool erase(std::uint64_t key) noexcept {
        for (std::size_t i = hash(key), n = 0; n < cap_; i = (i + 1) & mask_, ++n) {
            const auto k = cells_[i].key.load(std::memory_order_acquire);
            if (k == key)
                return cells_[i].value.exchange(kTombstone, std::memory_order_acq_rel) != kTombstone;
            if (k == kEmptyKey) return false;
        }
        return false;
    }

    // Atomically add delta to the value (creating it at 0). Handy for counters.
    bool fetch_add(std::uint64_t key, std::uint64_t delta) noexcept {
        assert(key != kEmptyKey && "ConcurrentIntMap: key 0 is reserved");
        Cell* c = claim(key);
        if (!c) return false;
        std::uint64_t cur = c->value.load(std::memory_order_relaxed);
        for (;;) {
            const std::uint64_t base = (cur == kTombstone) ? 0 : cur;
            assert(base + delta != kTombstone && "ConcurrentIntMap: counter wrapped onto the tombstone");
            if (c->value.compare_exchange_weak(cur, base + delta, std::memory_order_acq_rel))
                return true;
        }
    }

    std::size_t capacity() const noexcept { return cap_; }

private:
    struct Cell {
        std::atomic<std::uint64_t> key{kEmptyKey};
        std::atomic<std::uint64_t> value{kTombstone};
    };

    std::size_t hash(std::uint64_t k) const noexcept {
        // MurmurHash3 finalizer: cheap and spreads sequential IDs well.
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return static_cast<std::size_t>(k) & mask_;
    }

    // Find the cell for key, writing the key into an empty cell if needed.
    Cell* claim(std::uint64_t key) noexcept {
        for (std::size_t i = hash(key), n = 0; n < cap_; i = (i + 1) & mask_, ++n) {
            auto k = cells_[i].key.load(std::memory_order_acquire);
            if (k == key) return &cells_[i];
            if (k == kEmptyKey) {
                if (cells_[i].key.compare_exchange_strong(k, key, std::memory_order_acq_rel))
                    return &cells_[i];
                if (k == key) return &cells_[i];  // another thread inserted the same key
                // otherwise a different key took the cell: keep probing
            }
        }
        return nullptr;  // full
    }

    const std::size_t cap_;
    const std::size_t mask_;
    std::unique_ptr<Cell[]> cells_;
};

}  // namespace llds
