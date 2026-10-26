// llds/reclaim/hazard_pointers.hpp
// A compact hazard-pointer domain (Chapter 12), after Maged Michael (2004).
//
// A reader "protects" a pointer by publishing it in a per-thread slot and
// then re-checking that the source still holds it. A thread that removes a
// node "retires" it; retired nodes are freed only when a scan shows that no
// slot holds them. Memory overhead is bounded: at most
// kMaxThreads * kSlotsPerThread nodes can be protected, so each thread's
// retire list stays below roughly twice that before a scan frees the rest.
//
// The C++26 std::hazard_pointer (P2530) has the same shape:
//   hazard_pointer hp = make_hazard_pointer();  T* p = hp.protect(src);
//   obj->retire();
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "../common/cacheline.hpp"

namespace llds {

class HazardDomain {
public:
    static constexpr std::size_t kMaxThreads = 128;
    static constexpr std::size_t kSlotsPerThread = 2;
    static constexpr std::size_t kScanThreshold = 2 * kMaxThreads * kSlotsPerThread;

    static HazardDomain& global() {
        static HazardDomain d;
        return d;
    }

    ~HazardDomain() {
        // Everything still retired is unreachable by definition at shutdown.
        for (auto& rec : records_)
            for (auto& r : rec.retired) r.deleter(r.ptr);
        for (auto& r : orphans_) r.deleter(r.ptr);
    }

    // RAII handle on one hazard slot of the calling thread.
    class Guard {
    public:
        Guard() : slot_(&HazardDomain::global().acquire_slot()) {}
        ~Guard() {
            slot_->store(nullptr, std::memory_order_release);
            HazardDomain::global().release_slot(*slot_);
        }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        // Load src and keep it protected until reset() or destruction.
        //
        // The slot store and the re-read of src must not be reordered: if the
        // re-read could happen first, a scanner could miss our hazard while we
        // still use a node it frees. In C++ only seq_cst on both operations (or
        // a seq_cst fence between them) forbids that store->load reordering.
        // An acquire re-read is not enough: on ARMv8.3+ it compiles to ldapr,
        // which may pass the earlier stlr, and on POWER no barrier separates them.
        template <class T>
        T* protect(const std::atomic<T*>& src) noexcept {
            T* p = src.load(std::memory_order_relaxed);
            for (;;) {
                slot_->store(p, std::memory_order_seq_cst);     // announce
                T* again = src.load(std::memory_order_seq_cst);  // validate
                if (again == p) return p;
                p = again;
            }
        }

        template <class T>
        void reset_protection(T* p) noexcept { slot_->store(p, std::memory_order_seq_cst); }
        void reset() noexcept { slot_->store(nullptr, std::memory_order_release); }

    private:
        std::atomic<void*>* slot_;
    };

    template <class T>
    void retire(T* p) { retire(p, [](void* q) { delete static_cast<T*>(q); }); }

    void retire(void* p, void (*deleter)(void*)) {
        auto& rec = my_record();
        rec.retired.push_back({p, deleter});
        if (rec.retired.size() >= kScanThreshold) scan(rec);
    }

    // Force a scan of the calling thread's retire list (tests use this).
    void flush() { scan(my_record()); }

    std::size_t retired_count_for_this_thread() { return my_record().retired.size(); }

private:
    struct Retired {
        void* ptr;
        void (*deleter)(void*);
    };

    struct alignas(kFalseSharingRange) Record {
        std::atomic<bool> owned{false};
        std::atomic<void*> slots[kSlotsPerThread] = {};
        std::atomic<unsigned> in_use{0};  // bitmask of handed-out slots
        std::vector<Retired> retired;     // touched only by the owner
    };

    struct ThreadHandle {
        Record* rec = nullptr;
        ~ThreadHandle() {
            if (!rec) return;
            HazardDomain::global().scan(*rec);
            // Leftovers are still protected by someone; adopt them into the
            // orphan list so they are not leaked.
            HazardDomain::global().adopt(*rec);
            rec->owned.store(false, std::memory_order_release);
        }
    };

    Record& my_record() {
        thread_local ThreadHandle h;
        if (!h.rec) {
            for (auto& r : records_) {
                bool expected = false;
                if (!r.owned.load(std::memory_order_relaxed) &&
                    r.owned.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                    h.rec = &r;
                    break;
                }
            }
            if (!h.rec) {  // checked in every build type, not only with assertions on
                std::fputs("llds::HazardDomain: more than kMaxThreads threads are alive at once\n", stderr);
                std::abort();
            }
        }
        return *h.rec;
    }

    std::atomic<void*>& acquire_slot() {
        auto& rec = my_record();
        auto used = rec.in_use.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < kSlotsPerThread; ++i) {
            if (!(used & (1u << i))) {
                rec.in_use.store(used | (1u << i), std::memory_order_relaxed);
                return rec.slots[i];
            }
        }
        std::fputs("llds::HazardDomain: out of hazard slots for this thread\n", stderr);
        std::abort();
    }

    void release_slot(std::atomic<void*>& slot) {
        auto& rec = my_record();
        for (std::size_t i = 0; i < kSlotsPerThread; ++i)
            if (&rec.slots[i] == &slot)
                rec.in_use.store(rec.in_use.load(std::memory_order_relaxed) & ~(1u << i),
                                 std::memory_order_relaxed);
    }

    void scan(Record& rec) {
        // Pairs with the seq_cst store and load in protect(). The node was
        // unlinked before this fence; either our slot reads below see the
        // reader's hazard, or the reader's seq_cst re-read of src sees the
        // unlink and it retries.
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::vector<void*> hazards;
        hazards.reserve(kMaxThreads * kSlotsPerThread);
        for (auto& r : records_)
            for (auto& s : r.slots)
                if (void* p = s.load(std::memory_order_acquire)) hazards.push_back(p);
        std::sort(hazards.begin(), hazards.end());

        take_orphans(rec);
        std::vector<Retired> keep;
        for (auto& r : rec.retired) {
            if (std::binary_search(hazards.begin(), hazards.end(), r.ptr)) keep.push_back(r);
            else r.deleter(r.ptr);
        }
        rec.retired.swap(keep);
    }

    void adopt(Record& rec) {
        if (rec.retired.empty()) return;
        std::lock_guard<SpinFlag> g(orphan_lock_);
        orphans_.insert(orphans_.end(), rec.retired.begin(), rec.retired.end());
        rec.retired.clear();
        has_orphans_.store(true, std::memory_order_release);
    }

    void take_orphans(Record& rec) {
        if (!has_orphans_.load(std::memory_order_acquire)) return;
        std::lock_guard<SpinFlag> g(orphan_lock_);
        rec.retired.insert(rec.retired.end(), orphans_.begin(), orphans_.end());
        orphans_.clear();
        has_orphans_.store(false, std::memory_order_release);
    }

    struct SpinFlag {
        std::atomic_flag f = ATOMIC_FLAG_INIT;
        void lock() { while (f.test_and_set(std::memory_order_acquire)) {} }
        void unlock() { f.clear(std::memory_order_release); }
    };

    Record records_[kMaxThreads];
    SpinFlag orphan_lock_;
    std::atomic<bool> has_orphans_{false};
    std::vector<Retired> orphans_;
};

}  // namespace llds
