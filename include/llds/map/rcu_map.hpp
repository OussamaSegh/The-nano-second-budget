// llds/map/rcu_map.hpp
// A read-mostly map using read-copy-update with epoch reclamation (Ch. 13).
//
// Readers load one atomic pointer to an immutable snapshot and look the key
// up in it. They never write shared memory except their own epoch slot, so
// read throughput scales linearly with cores and read latency is the cost of
// a hash lookup. Writers copy the whole snapshot, modify the copy, publish
// it with one atomic store, and retire the old snapshot.
//
// Use it for configuration, routing tables, symbol or device registries:
// data read millions of times per second and changed a few times a minute.
#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "../reclaim/epoch.hpp"

namespace llds {

template <class K, class V>
class RcuMap {
    using Snapshot = std::unordered_map<K, V>;

public:
    RcuMap() : current_(new Snapshot()) {}
    ~RcuMap() { delete current_.load(std::memory_order_relaxed); }

    RcuMap(const RcuMap&) = delete;
    RcuMap& operator=(const RcuMap&) = delete;

    std::optional<V> find(const K& k) const {
        EpochDomain::Guard g;
        const Snapshot* s = current_.load(std::memory_order_acquire);
        auto it = s->find(k);
        if (it == s->end()) return std::nullopt;
        return it->second;
    }

    // Read several keys from one consistent snapshot.
    template <class F>
    void read(F&& f) const {
        EpochDomain::Guard g;
        f(*current_.load(std::memory_order_acquire));
    }

    template <class F>
    void update(F&& mutate) {
        std::lock_guard lock(writer_mutex_);  // writers are serialized
        const Snapshot* old = current_.load(std::memory_order_relaxed);
        auto* fresh = new Snapshot(*old);
        mutate(*fresh);
        current_.store(fresh, std::memory_order_release);
        EpochDomain::global().retire(const_cast<Snapshot*>(old));
    }

    void insert_or_assign(const K& k, V v) {
        update([&](Snapshot& s) { s.insert_or_assign(k, std::move(v)); });
    }
    void erase(const K& k) {
        update([&](Snapshot& s) { s.erase(k); });
    }

private:
    std::atomic<const Snapshot*> current_;
    std::mutex writer_mutex_;
};

}  // namespace llds
