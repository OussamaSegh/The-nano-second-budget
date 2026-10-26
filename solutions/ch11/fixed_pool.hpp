// Exercise 11.1 (solution): a single-threaded fixed-size object pool.
//
// Free slots are threaded into an intrusive singly linked list through the
// slots themselves: a free slot's storage holds the pointer to the next
// free slot, so the pool needs no memory beyond the slots. This is safe
// here (unlike in LockFreePool) because only one thread touches the pool.
#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <utility>

template <class T>
class FixedPool {
    union Slot {
        Slot* next;
        alignas(T) unsigned char storage[sizeof(T)];
    };

public:
    explicit FixedPool(std::size_t capacity) : slots_(new Slot[capacity]), capacity_(capacity) {
        for (std::size_t i = 0; i < capacity; ++i) slots_[i].next = i + 1 < capacity ? &slots_[i + 1] : nullptr;
        free_ = capacity ? &slots_[0] : nullptr;
    }

    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;

    void* allocate() noexcept {
        Slot* s = free_;
        if (!s) return nullptr;
        free_ = s->next;
        ++in_use_;
        return s->storage;
    }

    void deallocate(void* p) noexcept {
        Slot* s = reinterpret_cast<Slot*>(p);
        s->next = free_;
        free_ = s;
        --in_use_;
    }

    template <class... Args>
    T* create(Args&&... args) {
        void* m = allocate();
        if (!m) return nullptr;
        try {
            return ::new (m) T(std::forward<Args>(args)...);
        } catch (...) {
            deallocate(m);
            throw;
        }
    }

    void destroy(T* p) noexcept {
        if (!p) return;
        p->~T();
        deallocate(p);
    }

    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t in_use() const noexcept { return in_use_; }

private:
    std::unique_ptr<Slot[]> slots_;
    std::size_t capacity_;
    Slot* free_ = nullptr;
    std::size_t in_use_ = 0;
};
