// llds/mem/object_pool.hpp
// Fixed-capacity object pools (Chapter 11).
//
//  * LockFreePool<T>: any thread may allocate and free. Free slots form an
//    intrusive Treiber stack with a tagged head (no ABA, no reclamation
//    problem: slots are never returned to the OS while the pool lives).
//  * The memory is allocated once, up front, and optionally pre-faulted so
//    the first use of each page does not take a page fault on the hot path.
#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <utility>

#include "../common/cacheline.hpp"
#include "../stack/treiber_stack.hpp"

namespace llds {

template <class T>
class LockFreePool {
    // The free-list hook is kept *beside* the object, not overlaid on it.
    // Overlaying saves 8 bytes per slot but creates a data race: a thread
    // still inside pop() may read hook.next of a slot that another thread
    // has just allocated and is constructing a T over.
    struct Slot {
        TaggedStackHook hook;
        alignas(T) unsigned char storage[sizeof(T)];
    };

public:
    explicit LockFreePool(std::size_t capacity, bool prefault = true)
        : capacity_(capacity), slots_(new Slot[capacity]) {
        if (prefault) std::memset(static_cast<void*>(slots_.get()), 0, capacity * sizeof(Slot));
        // Push in reverse so the first allocations hand out low addresses
        // (friendlier to the prefetcher in examples).
        for (std::size_t i = capacity; i-- > 0;) free_.push(&slots_[i].hook);
    }

    LockFreePool(const LockFreePool&) = delete;
    LockFreePool& operator=(const LockFreePool&) = delete;

    // Returns nullptr when the pool is exhausted; never calls malloc.
    template <class... Args>
    T* create(Args&&... args) {
        void* mem = allocate();
        if (!mem) return nullptr;
        try {
            return new (mem) T(std::forward<Args>(args)...);
        } catch (...) {
            deallocate(mem);
            throw;
        }
    }

    void destroy(T* p) noexcept {
        if (!p) return;
        p->~T();
        deallocate(p);
    }

    void* allocate() noexcept {
        TaggedStackHook* h = free_.pop();
        return h ? static_cast<void*>(reinterpret_cast<Slot*>(h)->storage) : nullptr;
    }

    void deallocate(void* p) noexcept {
        auto* base = reinterpret_cast<unsigned char*>(slots_.get());
        const auto idx = static_cast<std::size_t>(static_cast<unsigned char*>(p) - base) / sizeof(Slot);
        free_.push(&slots_[idx].hook);
    }

    bool owns(const void* p) const noexcept {
        auto* b = reinterpret_cast<const unsigned char*>(slots_.get());
        auto* q = static_cast<const unsigned char*>(p);
        return q >= b && q < b + capacity_ * sizeof(Slot);
    }

    std::size_t capacity() const noexcept { return capacity_; }

private:
    std::size_t capacity_;
    std::unique_ptr<Slot[]> slots_;
    TaggedStack free_;
};

// A single-threaded bump allocator: allocation is a pointer increment and
// freeing is resetting the whole arena at once (for example at the end of
// each message or each frame).
class Arena {
public:
    explicit Arena(std::size_t bytes)
        : buf_(static_cast<std::byte*>(::operator new(bytes, std::align_val_t{kCacheLine}))),
          size_(bytes) {
        std::memset(buf_, 0, bytes);  // pre-fault
    }
    ~Arena() { ::operator delete(buf_, std::align_val_t{kCacheLine}); }
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* allocate(std::size_t n, std::size_t align = alignof(std::max_align_t)) noexcept {
        std::size_t p = (used_ + align - 1) & ~(align - 1);
        if (p + n > size_) return nullptr;
        used_ = p + n;
        return buf_ + p;
    }

    template <class T, class... Args>
    T* make(Args&&... args) {
        void* m = allocate(sizeof(T), alignof(T));
        return m ? new (m) T(std::forward<Args>(args)...) : nullptr;
    }

    void reset() noexcept { used_ = 0; }
    std::size_t used() const noexcept { return used_; }
    std::size_t capacity() const noexcept { return size_; }

private:
    std::byte* buf_;
    std::size_t size_;
    std::size_t used_ = 0;
};

}  // namespace llds
