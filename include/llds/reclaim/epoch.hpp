// llds/reclaim/epoch.hpp
// Epoch-based reclamation (Chapter 12), after Fraser (2004).
//
// Threads enter a critical region by announcing the current global epoch.
// A node retired in epoch e can be freed once every thread inside a critical
// region has announced an epoch later than e. Concretely, the global epoch
// only advances from e to e+1 when every active thread is in e, so after two
// advances (e+2) no thread can still hold a pointer read during e.
//
// Compared to hazard pointers: reads are cheaper (one store per region, not
// per pointer), but a single stalled reader blocks all reclamation, so memory
// use is unbounded in the worst case.
//
// kMaxThreads bounds the threads registered at the same time, not over the
// life of the process: a thread's record is released when it exits, and the
// nodes it retired but could not yet free move to a shared orphan list.
#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "../common/cacheline.hpp"

namespace llds {

class EpochDomain {
public:
    static constexpr std::size_t kMaxThreads = 128;
    static constexpr std::uint64_t kInactive = ~std::uint64_t{0};
    static constexpr std::size_t kCollectEvery = 64;

    struct Retired {
        void* ptr;
        void (*deleter)(void*);
    };

    struct alignas(kFalseSharingRange) Record {
        std::atomic<bool> owned{false};
        std::atomic<std::uint64_t> local{kInactive};
        unsigned nesting = 0;             // Guards may nest; only the outermost announces
        std::uint64_t retire_count = 0;
        std::uint64_t freed_through = 0;  // highest epoch whose bucket we emptied
        std::vector<Retired> limbo[3];    // retired nodes, by epoch % 3
    };

    static EpochDomain& global() {
        static EpochDomain d;
        return d;
    }

    ~EpochDomain() {
        for (auto& r : records_)
            for (auto& bucket : r.limbo)
                for (auto& x : bucket) x.deleter(x.ptr);
        for (auto& o : orphans_) o.node.deleter(o.node.ptr);
    }

    // RAII critical region. Pointers loaded inside it stay valid until it ends.
    class Guard {
    public:
        Guard() : rec_(EpochDomain::global().my_record()) {
            if (rec_.nesting++ == 0) {
                auto e = EpochDomain::global().epoch_.load(std::memory_order_relaxed);
                rec_.local.store(e, std::memory_order_relaxed);
                // The announcement must be visible before any shared load.
                std::atomic_thread_fence(std::memory_order_seq_cst);
            }
        }
        ~Guard() {
            if (--rec_.nesting == 0) rec_.local.store(kInactive, std::memory_order_release);
        }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

    private:
        Record& rec_;
    };

    template <class T>
    void retire(T* p) { retire(p, [](void* q) { delete static_cast<T*>(q); }); }

    void retire(void* p, void (*deleter)(void*)) {
        auto& rec = my_record();
        const auto e = epoch_.load(std::memory_order_acquire);
        rec.limbo[e % 3].push_back({p, deleter});
        if (++rec.retire_count % kCollectEvery == 0) collect(rec);
    }

    // Try to advance the epoch and free what is safe. Tests call it directly.
    void collect() { collect(my_record()); }

    std::uint64_t epoch() const noexcept { return epoch_.load(std::memory_order_acquire); }

private:
    struct ThreadHandle {
        Record* rec = nullptr;
        ~ThreadHandle() {
            if (!rec) return;
            auto& d = EpochDomain::global();
            d.collect(*rec);
            d.adopt(*rec);  // hand what is left to the orphan list
            rec->local.store(kInactive, std::memory_order_relaxed);
            rec->nesting = 0;
            rec->retire_count = 0;
            rec->freed_through = 0;
            rec->owned.store(false, std::memory_order_release);  // the record can be reused
        }
    };

    // A node retired by a thread that has exited, tagged with an epoch at or
    // after the one it was retired in. It is safe to free once the global
    // epoch has advanced twice past the tag.
    struct Orphan {
        std::uint64_t epoch;
        Retired node;
    };

    void adopt(Record& rec) {
        const auto e = epoch_.load(std::memory_order_acquire);
        std::lock_guard<std::mutex> g(orphan_lock_);
        for (auto& bucket : rec.limbo) {
            for (auto& x : bucket) orphans_.push_back({e, x});
            bucket.clear();
        }
        has_orphans_.store(!orphans_.empty(), std::memory_order_release);
    }

    void free_orphans(std::uint64_t e) {
        if (!has_orphans_.load(std::memory_order_acquire)) return;
        std::vector<Retired> ready;
        {
            std::lock_guard<std::mutex> g(orphan_lock_);
            std::vector<Orphan> keep;
            for (auto& o : orphans_) {
                if (e >= o.epoch + 2) ready.push_back(o.node);
                else keep.push_back(o);
            }
            orphans_.swap(keep);
            has_orphans_.store(!orphans_.empty(), std::memory_order_release);
        }
        for (auto& x : ready) x.deleter(x.ptr);
    }

    Record& my_record() {
        thread_local ThreadHandle h;
        if (!h.rec) {
            for (auto& r : records_) {
                bool expected = false;
                if (r.owned.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                    h.rec = &r;
                    break;
                }
            }
            if (!h.rec) {  // checked in every build type, not only with assertions on
                std::fputs("llds::EpochDomain: more than kMaxThreads threads are registered at once\n", stderr);
                std::abort();
            }
        }
        return *h.rec;
    }

    bool try_advance(std::uint64_t e) {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        for (auto& r : records_) {
            if (!r.owned.load(std::memory_order_acquire)) continue;
            const auto l = r.local.load(std::memory_order_acquire);
            if (l != kInactive && l != e) return false;  // someone still in an older epoch
        }
        return epoch_.compare_exchange_strong(e, e + 1, std::memory_order_acq_rel);
    }

    void collect(Record& rec) {
        auto e = epoch_.load(std::memory_order_acquire);
        try_advance(e);
        e = epoch_.load(std::memory_order_acquire);
        // Nodes retired in epoch <= e-2 are unreachable. Bucket (e+1)%3 holds
        // nodes from epoch e-2 (and older ones that share the bucket).
        if (e >= 2 && rec.freed_through < e - 1) {
            auto& bucket = rec.limbo[(e + 1) % 3];
            for (auto& x : bucket) x.deleter(x.ptr);
            bucket.clear();
            rec.freed_through = e - 1;
        }
        free_orphans(e);
    }

    alignas(kCacheLine) std::atomic<std::uint64_t> epoch_{0};
    Record records_[kMaxThreads];
    std::mutex orphan_lock_;
    std::atomic<bool> has_orphans_{false};
    std::vector<Orphan> orphans_;
};

}  // namespace llds
