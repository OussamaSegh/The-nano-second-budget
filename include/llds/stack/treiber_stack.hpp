// llds/stack/treiber_stack.hpp
// Treiber's lock-free stack (Chapter 6), in two flavours.
//
//  * TaggedStack<T>: nodes come from a caller-owned free list and are never
//    returned to the OS while the stack lives, and a 16-bit version tag
//    packed into the pointer defeats the ABA problem. This is the classic
//    "lock-free free list" used inside pools (Chapter 11).
//
//  * The ABA-prone naive stack lives in aba_demo() in examples/ch06_atomics.cpp
//    so readers can watch it break.
#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

#include "../common/cacheline.hpp"

namespace llds {

// Pack a 48-bit user-space pointer with a 16-bit tag. x86-64 and AArch64
// (without top-byte-ignore tricks) use at most 48 bits of virtual address in
// user space on Linux by default; with 5-level paging (LA57) this breaks, and
// a production version would use double-width CAS instead (see the text).
class TaggedPtr {
public:
    static constexpr std::uint64_t kPtrMask = (std::uint64_t{1} << 48) - 1;

    TaggedPtr() = default;
    TaggedPtr(void* p, std::uint16_t tag) noexcept
        : bits_((reinterpret_cast<std::uint64_t>(p) & kPtrMask) |
                (static_cast<std::uint64_t>(tag) << 48)) {}

    template <class T>
    T* ptr() const noexcept { return reinterpret_cast<T*>(bits_ & kPtrMask); }
    std::uint16_t tag() const noexcept { return static_cast<std::uint16_t>(bits_ >> 48); }
    bool operator==(const TaggedPtr&) const = default;

private:
    std::uint64_t bits_ = 0;
};

// An intrusive lock-free LIFO of nodes that derive from TaggedStackHook.
struct TaggedStackHook {
    std::atomic<TaggedStackHook*> next{nullptr};
};

class TaggedStack {
public:
    void push(TaggedStackHook* n) noexcept {
        TaggedPtr old = head_.load(std::memory_order_relaxed);
        for (;;) {
            n->next.store(old.ptr<TaggedStackHook>(), std::memory_order_relaxed);
            TaggedPtr desired(n, static_cast<std::uint16_t>(old.tag() + 1));
            if (head_.compare_exchange_weak(old, desired, std::memory_order_release,
                                            std::memory_order_relaxed))
                return;
        }
    }

    TaggedStackHook* pop() noexcept {
        TaggedPtr old = head_.load(std::memory_order_acquire);
        for (;;) {
            auto* top = old.ptr<TaggedStackHook>();
            if (!top) return nullptr;
            // Reading top->next is safe only because nodes are never freed
            // while the stack exists; the value may be stale, in which case
            // the tag makes the CAS fail.
            auto* next = top->next.load(std::memory_order_relaxed);
            TaggedPtr desired(next, static_cast<std::uint16_t>(old.tag() + 1));
            if (head_.compare_exchange_weak(old, desired, std::memory_order_acquire,
                                            std::memory_order_acquire))
                return top;
        }
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire).ptr<TaggedStackHook>() == nullptr;
    }

private:
    alignas(kCacheLine) std::atomic<TaggedPtr> head_{};
};

static_assert(std::atomic<TaggedPtr>::is_always_lock_free);

}  // namespace llds
