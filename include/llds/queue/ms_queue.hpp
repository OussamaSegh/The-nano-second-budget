// llds/queue/ms_queue.hpp
// The Michael-Scott unbounded lock-free MPMC queue (Chapter 9), with hazard
// pointers for memory reclamation (Chapter 12).
//
// The queue is a singly linked list with a dummy node at the head. Enqueue
// links a new node after the tail and then swings tail_; dequeue swings
// head_ forward and returns the value stored in the *new* head. Any thread
// that finds tail_ lagging behind helps by advancing it, which is what makes
// the algorithm lock-free rather than merely non-blocking in the common case.
//
// Every operation allocates or frees a node, so this queue is usually slower
// than MpmcBoundedQueue. Use it when you genuinely cannot bound the queue,
// and pair it with a pool allocator (Chapter 11) on hot paths.
#pragma once

#include <atomic>
#include <optional>
#include <utility>

#include "../common/cacheline.hpp"
#include "../reclaim/hazard_pointers.hpp"

namespace llds {

template <class T>
class MsQueue {
    struct Node {
        std::atomic<Node*> next{nullptr};
        std::optional<T> value;  // empty in the dummy node
        Node() = default;
        template <class U>
        explicit Node(U&& v) : value(std::forward<U>(v)) {}
    };

public:
    MsQueue() {
        Node* dummy = new Node();
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }

    ~MsQueue() {
        Node* n = head_.load(std::memory_order_relaxed);
        while (n) {
            Node* next = n->next.load(std::memory_order_relaxed);
            delete n;
            n = next;
        }
    }

    MsQueue(const MsQueue&) = delete;
    MsQueue& operator=(const MsQueue&) = delete;

    template <class U>
    void push(U&& v) {
        Node* node = new Node(std::forward<U>(v));
        HazardDomain::Guard hp;
        for (;;) {
            Node* tail = hp.protect(tail_);
            Node* next = tail->next.load(std::memory_order_acquire);
            if (tail != tail_.load(std::memory_order_acquire)) continue;  // tail moved
            if (next) {
                // Tail is lagging: help the other enqueuer and retry.
                tail_.compare_exchange_weak(tail, next, std::memory_order_release,
                                            std::memory_order_relaxed);
                continue;
            }
            Node* expected = nullptr;
            if (tail->next.compare_exchange_weak(expected, node, std::memory_order_release,
                                                 std::memory_order_relaxed)) {
                // Linked. Swinging tail_ may fail if someone helped; that is fine.
                tail_.compare_exchange_strong(tail, node, std::memory_order_release,
                                              std::memory_order_relaxed);
                return;
            }
        }
    }

    std::optional<T> try_pop() {
        HazardDomain::Guard hp_head;
        HazardDomain::Guard hp_next;
        for (;;) {
            Node* head = hp_head.protect(head_);
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = hp_next.protect(head->next);
            if (head != head_.load(std::memory_order_acquire)) continue;
            if (!next) return std::nullopt;  // empty (head == dummy, no successor)
            if (head == tail) {
                // Tail lagging behind a completed enqueue; help it along.
                tail_.compare_exchange_weak(tail, next, std::memory_order_release,
                                            std::memory_order_relaxed);
                continue;
            }
            if (head_.compare_exchange_weak(head, next, std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
                // `next` is the new dummy. Only the thread that won the CAS
                // may move its value out; other threads never read it.
                std::optional<T> out(std::move(next->value));
                next->value.reset();
                hp_next.reset();
                hp_head.reset();
                HazardDomain::global().retire(head);
                return out;
            }
        }
    }

    bool empty() const {
        HazardDomain::Guard hp;
        Node* head = hp.protect(head_);
        return head->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    alignas(kFalseSharingRange) std::atomic<Node*> head_;
    alignas(kFalseSharingRange) std::atomic<Node*> tail_;
};

}  // namespace llds
