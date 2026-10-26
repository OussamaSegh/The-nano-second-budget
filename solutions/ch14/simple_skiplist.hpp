// Exercise 14.1 (solution): a sequential skip list with lower_bound.
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

    bool insert(const K& k) {
        Node* preds[MaxLevel];
        Node* n = find(k, preds);
        if (n && !(k < n->key)) return false;  // already present
        const int top = random_level();
        Node* node = new Node{k, top};
        for (int l = 0; l <= top; ++l) {
            node->next[l] = preds[l]->next[l];
            preds[l]->next[l] = node;
        }
        ++size_;
        return true;
    }

    bool contains(const K& k) const {
        Node* preds[MaxLevel];
        Node* n = const_cast<SimpleSkipList*>(this)->find(k, preds);
        return n && !(k < n->key);
    }

    bool erase(const K& k) {
        Node* preds[MaxLevel];
        Node* n = find(k, preds);
        if (!n || k < n->key) return false;
        for (int l = 0; l <= n->top; ++l) preds[l]->next[l] = n->next[l];
        delete n;
        --size_;
        return true;
    }

    // Smallest key >= k, if any.
    std::optional<K> lower_bound(const K& k) const {
        Node* preds[MaxLevel];
        Node* n = const_cast<SimpleSkipList*>(this)->find(k, preds);
        if (!n) return std::nullopt;
        return n->key;
    }

    std::size_t size() const noexcept { return size_; }

private:
    // Returns the first node with key >= k (or nullptr) and fills preds[l]
    // with the last node at level l whose key < k.
    Node* find(const K& k, Node** preds) {
        Node* x = head_;
        for (int l = MaxLevel - 1; l >= 0; --l) {
            while (x->next[l] && x->next[l]->key < k) x = x->next[l];
            preds[l] = x;
        }
        return x->next[0];
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
