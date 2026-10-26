// llds/ordered/lazy_skiplist.hpp
// The lazy concurrent skip list (Chapter 14), after Herlihy, Lev, Luchangco
// and Shavit, "A Simple Optimistic Skiplist Algorithm" (2007).
//
//  * contains()/find() take no locks and never retry: they are wait-free.
//  * insert() and erase() lock only the predecessors they modify, after an
//    optimistic, lock-free search, and then validate.
//  * A node is logically in the set when fully_linked && !marked. Erase
//    first marks (logical delete), then unlinks (physical delete).
//  * Unlinked nodes are retired through EpochDomain, so a concurrent reader
//    that still holds a pointer never touches freed memory.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <utility>

#include "../common/cpu.hpp"
#include "../reclaim/epoch.hpp"
#include "../sync/spinlock.hpp"

namespace llds {

template <class K, class V, int MaxLevel = 16>
class LazySkipList {
    enum class Kind : std::uint8_t { Head, Data, Tail };

    struct Node {
        Kind kind;
        K key{};
        V value{};
        int top_level;
        std::atomic<bool> marked{false};
        std::atomic<bool> fully_linked{false};
        TtasLock lock;
        std::array<std::atomic<Node*>, MaxLevel> next{};

        Node(Kind k, int top) : kind(k), top_level(top) {}
        Node(K k, V v, int top) : kind(Kind::Data), key(std::move(k)), value(std::move(v)), top_level(top) {}
    };

    // a < k, treating Head as -inf and Tail as +inf.
    static bool less(const Node* a, const K& k) noexcept {
        if (a->kind == Kind::Head) return true;
        if (a->kind == Kind::Tail) return false;
        return a->key < k;
    }
    static bool equals(const Node* a, const K& k) noexcept {
        return a->kind == Kind::Data && !(a->key < k) && !(k < a->key);
    }

public:
    LazySkipList() : head_(new Node(Kind::Head, MaxLevel - 1)), tail_(new Node(Kind::Tail, MaxLevel - 1)) {
        for (auto& n : head_->next) n.store(tail_, std::memory_order_relaxed);
        head_->fully_linked.store(true, std::memory_order_relaxed);
        tail_->fully_linked.store(true, std::memory_order_relaxed);
    }

    ~LazySkipList() {
        Node* n = head_;
        while (n) {
            Node* next = n == tail_ ? nullptr : n->next[0].load(std::memory_order_relaxed);
            delete n;
            n = next;
        }
    }

    LazySkipList(const LazySkipList&) = delete;
    LazySkipList& operator=(const LazySkipList&) = delete;

    bool contains(const K& k) const {
        EpochDomain::Guard g;
        Node* preds[MaxLevel];
        Node* succs[MaxLevel];
        const int lf = find_node(k, preds, succs);
        return lf != -1 && succs[lf]->fully_linked.load(std::memory_order_acquire) &&
               !succs[lf]->marked.load(std::memory_order_acquire);
    }

    std::optional<V> find(const K& k) const {
        EpochDomain::Guard g;
        Node* preds[MaxLevel];
        Node* succs[MaxLevel];
        const int lf = find_node(k, preds, succs);
        if (lf == -1) return std::nullopt;
        Node* n = succs[lf];
        if (!n->fully_linked.load(std::memory_order_acquire) || n->marked.load(std::memory_order_acquire))
            return std::nullopt;
        return n->value;  // immutable after insertion
    }

    bool insert(const K& k, V v) {
        const int top = random_level();
        Node* preds[MaxLevel];
        Node* succs[MaxLevel];
        for (;;) {
            EpochDomain::Guard g;
            const int lf = find_node(k, preds, succs);
            if (lf != -1) {
                Node* found = succs[lf];
                if (!found->marked.load(std::memory_order_acquire)) {
                    // Someone inserted k; wait until it is fully linked so that
                    // our "false" is linearizable after their "true".
                    while (!found->fully_linked.load(std::memory_order_acquire)) cpu_relax();
                    return false;
                }
                continue;  // found a node being deleted: retry
            }

            int highest_locked = -1;
            bool valid = true;
            Node* prev_pred = nullptr;
            for (int l = 0; valid && l <= top; ++l) {
                Node* pred = preds[l];
                Node* succ = succs[l];
                if (pred != prev_pred) {
                    pred->lock.lock();
                    highest_locked = l;
                    prev_pred = pred;
                }
                valid = !pred->marked.load(std::memory_order_acquire) &&
                        !succ->marked.load(std::memory_order_acquire) &&
                        pred->next[l].load(std::memory_order_acquire) == succ;
            }
            if (!valid) {
                unlock_preds(preds, highest_locked);
                continue;
            }

            Node* n = new Node(k, std::move(v), top);
            for (int l = 0; l <= top; ++l) n->next[l].store(succs[l], std::memory_order_relaxed);
            for (int l = 0; l <= top; ++l) preds[l]->next[l].store(n, std::memory_order_release);
            n->fully_linked.store(true, std::memory_order_release);  // linearization point
            unlock_preds(preds, highest_locked);
            size_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }

    bool erase(const K& k) {
        Node* victim = nullptr;
        bool is_marked = false;
        int top = -1;
        Node* preds[MaxLevel];
        Node* succs[MaxLevel];
        for (;;) {
            EpochDomain::Guard g;
            const int lf = find_node(k, preds, succs);
            if (lf != -1) victim = succs[lf];
            if (is_marked ||
                (lf != -1 && victim->fully_linked.load(std::memory_order_acquire) &&
                 victim->top_level == lf && !victim->marked.load(std::memory_order_acquire))) {
                if (!is_marked) {
                    top = victim->top_level;
                    victim->lock.lock();
                    if (victim->marked.load(std::memory_order_relaxed)) {
                        victim->lock.unlock();
                        return false;  // someone else is deleting it
                    }
                    victim->marked.store(true, std::memory_order_release);  // linearization point
                    is_marked = true;
                }
                int highest_locked = -1;
                bool valid = true;
                Node* prev_pred = nullptr;
                for (int l = 0; valid && l <= top; ++l) {
                    Node* pred = preds[l];
                    if (pred != prev_pred) {
                        pred->lock.lock();
                        highest_locked = l;
                        prev_pred = pred;
                    }
                    valid = !pred->marked.load(std::memory_order_acquire) &&
                            pred->next[l].load(std::memory_order_acquire) == victim;
                }
                if (!valid) {
                    unlock_preds(preds, highest_locked);
                    continue;
                }
                for (int l = top; l >= 0; --l)
                    preds[l]->next[l].store(victim->next[l].load(std::memory_order_relaxed),
                                            std::memory_order_release);
                victim->lock.unlock();
                unlock_preds(preds, highest_locked);
                EpochDomain::global().retire(victim);
                size_.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
            return false;
        }
    }

    // Visit keys in [lo, hi) in order. Not a snapshot: concurrent updates may
    // or may not be seen, but every key reported was present at some point.
    template <class F>
    void for_range(const K& lo, const K& hi, F&& f) const {
        EpochDomain::Guard g;
        Node* preds[MaxLevel];
        Node* succs[MaxLevel];
        find_node(lo, preds, succs);
        for (Node* n = succs[0]; n->kind == Kind::Data && n->key < hi;
             n = n->next[0].load(std::memory_order_acquire)) {
            if (n->fully_linked.load(std::memory_order_acquire) && !n->marked.load(std::memory_order_acquire))
                f(n->key, n->value);
        }
    }

    std::size_t size_approx() const noexcept { return size_.load(std::memory_order_relaxed); }

private:
    int find_node(const K& k, Node** preds, Node** succs) const {
        int found = -1;
        Node* pred = head_;
        for (int l = MaxLevel - 1; l >= 0; --l) {
            Node* curr = pred->next[l].load(std::memory_order_acquire);
            while (less(curr, k)) {
                pred = curr;
                curr = pred->next[l].load(std::memory_order_acquire);
            }
            if (found == -1 && equals(curr, k)) found = l;
            preds[l] = pred;
            succs[l] = curr;
        }
        return found;
    }

    static void unlock_preds(Node** preds, int highest_locked) {
        Node* prev = nullptr;
        for (int l = 0; l <= highest_locked; ++l) {
            if (preds[l] != prev) {
                preds[l]->lock.unlock();
                prev = preds[l];
            }
        }
    }

    static int random_level() {
        // Geometric with p = 1/2 from a thread-local xorshift generator.
        thread_local std::uint64_t s = 0x9E3779B97F4A7C15ULL ^
                                       reinterpret_cast<std::uintptr_t>(&s);
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        int lvl = __builtin_ctzll(s | (std::uint64_t{1} << (MaxLevel - 1)));
        return lvl;  // 0 .. MaxLevel-1
    }

    Node* head_;
    Node* tail_;
    std::atomic<std::size_t> size_{0};
};

}  // namespace llds
