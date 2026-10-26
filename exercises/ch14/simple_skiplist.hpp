// Exercise 14.1: a sequential skip list.
//
// Before making a skip list concurrent, build one. Implement insert,
// contains, erase and lower_bound (smallest key >= k) in expected O(log n),
// using the helper find() that every operation shares: it returns the first
// node whose key is >= k and records, for each level, the last node before
// that position (the "predecessors" that insert and erase must relink).
#pragma once

#include <cstdint>
#include <optional>
#include <random>

template <class K, int MaxLevel = 16>
class SimpleSkipList {
    struct Node {
        K key;
        int top;
        Node* next[MaxLevel] = {};
    };

public:
    SimpleSkipList() : head_(new Node{K{}, MaxLevel - 1}) {}
    ~SimpleSkipList() {
        Node* n = head_;
        while (n) {
            Node* nx = n->next[0];
            delete n;
            n = nx;
        }
    }
    SimpleSkipList(const SimpleSkipList&) = delete;
    SimpleSkipList& operator=(const SimpleSkipList&) = delete;

    bool insert(const K& k) { (void)k; return false; /* TODO */ }
    bool contains(const K& k) const { (void)k; return false; /* TODO */ }
    bool erase(const K& k) { (void)k; return false; /* TODO */ }
    std::optional<K> lower_bound(const K& k) const { (void)k; return std::nullopt; /* TODO */ }
    std::size_t size() const noexcept { return size_; }

private:
    Node* find(const K& k, Node** preds) {
        // TODO: walk from the top level down
        (void)k;
        (void)preds;
        return nullptr;
    }

    int random_level() {
        int lvl = 0;
        while (lvl < MaxLevel - 1 && (rng_() & 1)) ++lvl;
        return lvl;
    }

    Node* head_;
    std::size_t size_ = 0;
    std::mt19937 rng_{12345};
};
