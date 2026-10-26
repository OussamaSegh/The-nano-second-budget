// llds/map/striped_hash_map.hpp
// Lock striping (Chapter 13): N independent locked shards selected by hash.
//
// The simplest concurrent map that scales. Contention drops roughly by the
// stripe count, every operation is easy to reason about, and any value type
// works. Its weakness is the read path: readers take a lock and write the
// lock's cache line, so read-mostly workloads should use RcuMap or
// ConcurrentIntMap instead.
#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "../common/cacheline.hpp"
#include "../sync/spinlock.hpp"

namespace llds {

template <class K, class V, std::size_t Stripes = 64, class Hash = std::hash<K>,
          class Lock = TtasLock>
class StripedHashMap {
    static_assert(is_pow2(Stripes));

public:
    bool insert_or_assign(const K& k, V v) {
        auto& s = shard(k);
        std::lock_guard g(s.lock);
        auto [it, inserted] = s.map.insert_or_assign(k, std::move(v));
        (void)it;
        return inserted;
    }

    bool insert(const K& k, V v) {
        auto& s = shard(k);
        std::lock_guard g(s.lock);
        return s.map.emplace(k, std::move(v)).second;
    }

    std::optional<V> find(const K& k) const {
        auto& s = shard(k);
        std::lock_guard g(s.lock);
        auto it = s.map.find(k);
        if (it == s.map.end()) return std::nullopt;
        return it->second;
    }

    bool erase(const K& k) {
        auto& s = shard(k);
        std::lock_guard g(s.lock);
        return s.map.erase(k) != 0;
    }

    // Apply f(V&) under the shard lock; inserts V{} if absent.
    template <class F>
    void update(const K& k, F&& f) {
        auto& s = shard(k);
        std::lock_guard g(s.lock);
        f(s.map[k]);
    }

    std::size_t size() const {
        std::size_t n = 0;
        for (auto& s : shards_) {
            std::lock_guard g(s.lock);
            n += s.map.size();
        }
        return n;
    }

private:
    struct alignas(kFalseSharingRange) Shard {
        mutable Lock lock;
        std::unordered_map<K, V, Hash> map;
    };

    Shard& shard(const K& k) const {
        // Mix the hash: std::hash<int> is the identity on libstdc++, and we
        // index stripes with the low bits.
        std::size_t h = Hash{}(k);
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        return const_cast<Shard&>(shards_[h & (Stripes - 1)]);
    }

    std::array<Shard, Stripes> shards_;
};

}  // namespace llds
