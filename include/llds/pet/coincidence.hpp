// llds/pet/coincidence.hpp
// Coincidence pairing for a time-ordered hit stream (Chapter 20).
//
// The pairing rule implemented here is deliberately simple and fully
// specified, so that three implementations can be checked against each
// other:
//
//   Given hits in non-decreasing tick order, emit every pair (i, j), i < j,
//   whose modules differ and whose tick distance is at most `window`.
//
// This is the "take all pairs, multiple window" policy. Real scanners use
// several other policies (single window, kill multiples, take winner) that
// change the scientific output; Chapter 20 discusses them and Exercise 20.1
// implements one. Performance work must never silently change the policy.
//
//   pairs_oracle      O(n^2) brute force: the executable specification
//   window_pairs      batch sliding window, O(n + C), C = candidates examined
//   StreamingSorter   the same output, hit by hit, with bounded retained state
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "../common/cacheline.hpp"
#include "hit.hpp"

namespace llds::pet {

struct Coincidence {
    Hit first;   // earlier in stream order
    Hit second;  // later in stream order
    friend bool operator==(const Coincidence&, const Coincidence&) = default;
};

constexpr bool eligible(const Hit& a, const Hit& b, std::uint64_t window) noexcept {
    return a.module != b.module && tick_distance(a.tick, b.tick) <= window;
}

// The specification, executed. Emits pairs ordered by j, then i, which is
// the order the faster implementations produce naturally.
inline std::vector<Coincidence> pairs_oracle(std::span<const Hit> hits, std::uint64_t window) {
    std::vector<Coincidence> out;
    for (std::size_t j = 0; j < hits.size(); ++j)
        for (std::size_t i = 0; i < j; ++i)
            if (eligible(hits[i], hits[j], window)) out.push_back({hits[i], hits[j]});
    return out;
}

// Batch sliding window. `first` only moves forward: a hit too old for hit j
// is too old for every later hit, because the input is sorted. That bounds
// the pointer moves by n, but not the candidates inside each window: in a
// dense window the work, and the output, are quadratic.
inline std::vector<Coincidence> window_pairs(std::span<const Hit> hits, std::uint64_t window) {
    std::vector<Coincidence> out;
    std::size_t first = 0;
    for (std::size_t j = 0; j < hits.size(); ++j) {
        if (j && hits[j].tick < hits[j - 1].tick) throw std::invalid_argument("window_pairs: input not sorted");
        while (hits[j].tick - hits[first].tick > window) ++first;  // first <= j, so no underflow
        for (std::size_t i = first; i < j; ++i)
            if (hits[i].module != hits[j].module) out.push_back({hits[i], hits[j]});
    }
    return out;
}

// Streaming version: feed hits one at a time, in tick order, from any number
// of chunks. It keeps only the hits that can still pair with a future hit,
// in a fixed-capacity ring allocated up front, so push() never allocates.
//
// Its output equals window_pairs() over the concatenated input, provided no
// hit was rejected and nothing overflowed; the counters say whether that
// held. Both failure cases are counted and reported rather than hidden:
//   * an out-of-order hit is rejected (not paired);
//   * if more than `max_retained` hits fall inside one window, the oldest is
//     evicted early and the pairs it would have formed are lost.
class StreamingSorter {
public:
    enum class Status : std::uint8_t { ok, out_of_order, evicted_oldest };

    struct Counters {
        std::uint64_t hits_in = 0;
        std::uint64_t pairs_out = 0;
        std::uint64_t out_of_order = 0;
        std::uint64_t forced_evictions = 0;
        std::size_t retained_peak = 0;
    };

    StreamingSorter(std::uint64_t window, std::size_t max_retained)
        : window_(window), cap_(next_pow2(max_retained < 1 ? 1 : max_retained)), mask_(cap_ - 1), ring_(cap_) {}

    template <class Sink>
    Status push(const Hit& h, Sink&& sink) {
        if (have_last_ && h.tick < last_tick_) {
            ++c_.out_of_order;
            return Status::out_of_order;
        }
        have_last_ = true;
        last_tick_ = h.tick;
        ++c_.hits_in;
        // Retire hits that are now too old for this and every later hit.
        while (head_ != tail_ && h.tick - ring_[head_ & mask_].tick > window_) ++head_;
        for (auto i = head_; i != tail_; ++i) {
            const Hit& r = ring_[i & mask_];
            if (r.module != h.module) {
                sink(Coincidence{r, h});
                ++c_.pairs_out;
            }
        }
        Status st = Status::ok;
        if (tail_ - head_ == cap_) {  // full: drop the oldest, and say so
            ++head_;
            ++c_.forced_evictions;
            st = Status::evicted_oldest;
        }
        ring_[tail_++ & mask_] = h;
        if (tail_ - head_ > c_.retained_peak) c_.retained_peak = static_cast<std::size_t>(tail_ - head_);
        return st;
    }

    const Counters& counters() const noexcept { return c_; }
    std::size_t retained() const noexcept { return static_cast<std::size_t>(tail_ - head_); }
    std::size_t capacity() const noexcept { return cap_; }

private:
    std::uint64_t window_;
    std::size_t cap_;
    std::size_t mask_;
    std::vector<Hit> ring_;
    std::uint64_t head_ = 0, tail_ = 0;  // monotonic, like SpscRing (Chapter 8)
    std::uint64_t last_tick_ = 0;
    bool have_last_ = false;
    Counters c_;
};

}  // namespace llds::pet
