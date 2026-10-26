// Exercise 11.1: a single-threaded fixed-size object pool.
//
// Requirements:
//   * all memory is allocated once, in the constructor;
//   * allocate() and deallocate() are O(1) and never call malloc;
//   * free slots form an intrusive linked list stored *inside* the free
//     slots themselves (no side table);
//   * create() constructs a T in place (and does not leak the slot if the
//     constructor throws); destroy() runs the destructor and frees the slot;
//   * allocate() returns nullptr when exhausted.
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
        // TODO: build the free list
    }

    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;

    void* allocate() noexcept { return nullptr; /* TODO */ }
    void deallocate(void* p) noexcept { (void)p; /* TODO */ }

    template <class... Args>
    T* create(Args&&... args) {
        // TODO
        ((void)args, ...);
        return nullptr;
    }

    void destroy(T* p) noexcept { (void)p; /* TODO */ }

    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t in_use() const noexcept { return in_use_; }

private:
    std::unique_ptr<Slot[]> slots_;
    std::size_t capacity_;
    Slot* free_ = nullptr;
    std::size_t in_use_ = 0;
};
