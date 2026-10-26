// llds/queue/disruptor.hpp
// A single-producer, multi-consumer ring in the style of the LMAX Disruptor
// (Chapter 10).
//
// Unlike a queue, entries are not removed: every consumer reads every entry,
// at its own pace, tracked by its own Sequence. The producer may overwrite
// slot s only after the slowest *gating* consumer has passed s - capacity.
// Consumers can depend on other consumers, which gives you a pipeline
// (parse -> validate -> process) without any queue between stages.
#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

#include "../common/cacheline.hpp"
#include "../common/cpu.hpp"

namespace llds {

// A padded monotonically increasing counter. -1 means "nothing yet".
struct alignas(kFalseSharingRange) Sequence {
    std::atomic<std::int64_t> value{-1};
    std::int64_t get() const noexcept { return value.load(std::memory_order_acquire); }
    void set(std::int64_t v) noexcept { value.store(v, std::memory_order_release); }
};

template <class T>
class Disruptor {
public:
    explicit Disruptor(std::size_t capacity)
        : cap_(static_cast<std::int64_t>(next_pow2(capacity))), mask_(cap_ - 1),
          entries_(new T[static_cast<std::size_t>(cap_)]()) {}

    std::int64_t capacity() const noexcept { return cap_; }

    // ---- wiring (before any thread starts) ------------------------------

    // Register a consumer that reads entries after `deps` have processed
    // them (empty deps: right after the producer publishes).
    Sequence& add_consumer(std::vector<const Sequence*> deps = {}) {
        auto& c = consumers_.emplace_back(std::make_unique<ConsumerInfo>());
        c->deps = std::move(deps);
        return c->seq;
    }

    // The producer waits on these. By default: every registered consumer.
    // (Gating only on the last stage of a pipeline is enough and cheaper.)
    void set_gating(std::vector<const Sequence*> gating) { gating_ = std::move(gating); }

    // ---- producer -------------------------------------------------------

    // Claim the next n slots; returns the highest sequence claimed. Blocks
    // (spinning) while the ring is full.
    std::int64_t claim(std::int64_t n = 1) noexcept {
        assert(n >= 1 && n <= cap_);
        const std::int64_t next = claimed_ + n;
        const std::int64_t wrap_point = next - cap_;
        if (wrap_point > gating_cache_) {
            std::int64_t min_seq;
            while (wrap_point > (min_seq = min_gating())) cpu_relax();
            gating_cache_ = min_seq;
        }
        claimed_ = next;
        return next;
    }

    T& operator[](std::int64_t seq) noexcept { return entries_[static_cast<std::size_t>(seq & mask_)]; }
    const T& operator[](std::int64_t seq) const noexcept {
        return entries_[static_cast<std::size_t>(seq & mask_)];
    }

    // Make everything up to and including `seq` visible to consumers.
    void publish(std::int64_t seq) noexcept { cursor_.set(seq); }

    const Sequence& cursor() const noexcept { return cursor_; }

    // ---- consumer -------------------------------------------------------

    // Wait until `seq` is available to a consumer with the given deps and
    // return the highest available sequence (which may be > seq: batching).
    std::int64_t wait_for(std::int64_t seq, const std::vector<const Sequence*>& deps) const noexcept {
        for (;;) {
            std::int64_t avail = cursor_.get();
            for (const Sequence* d : deps) avail = std::min(avail, d->get());
            if (avail >= seq) return avail;
            cpu_relax();
        }
    }

    // Non-blocking variant: returns the highest available sequence, which may
    // be < seq (nothing new).
    std::int64_t available(const std::vector<const Sequence*>& deps) const noexcept {
        std::int64_t avail = cursor_.get();
        for (const Sequence* d : deps) avail = std::min(avail, d->get());
        return avail;
    }

    const std::vector<const Sequence*>& deps_of(const Sequence& consumer) const {
        for (auto& c : consumers_)
            if (&c->seq == &consumer) return c->deps;
        static const std::vector<const Sequence*> none;
        return none;
    }

private:
    struct ConsumerInfo {
        Sequence seq;
        std::vector<const Sequence*> deps;
    };

    std::int64_t min_gating() const noexcept {
        std::int64_t m = cursor_.get();
        if (gating_.empty()) {
            for (auto& c : consumers_) m = std::min(m, c->seq.get());
        } else {
            for (const Sequence* s : gating_) m = std::min(m, s->get());
        }
        return m;
    }

    const std::int64_t cap_;
    const std::int64_t mask_;
    std::unique_ptr<T[]> entries_;
    Sequence cursor_;
    std::vector<std::unique_ptr<ConsumerInfo>> consumers_;
    std::vector<const Sequence*> gating_;

    // Producer-private.
    alignas(kFalseSharingRange) std::int64_t claimed_ = -1;
    std::int64_t gating_cache_ = -1;
};

}  // namespace llds
