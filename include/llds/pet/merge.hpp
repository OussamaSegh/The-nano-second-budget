// llds/pet/merge.hpp
// Merge several time-ordered hit streams into one, using watermarks
// (Chapter 20).
//
// Each source delivers its own hits in tick order, but the sources run
// independently, so the merger cannot know whether a later hit from a slow
// source will still arrive with an earlier tick. A source therefore sends,
// with each batch, a *promise*: "no hit I send from now on has a tick below
// P". The merger's watermark is the minimum promise over all open sources,
// and any buffered hit below the watermark is final and can be emitted.
//
// The weak point is visible in the definition: a source that stops sending
// stops the watermark, and with it the whole merge. That is not a bug in the
// merger, it is the contract; the owner must detect the silent source (a
// timeout) and close() or exclude it, recording that its data is missing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "../common/cacheline.hpp"
#include "hit.hpp"

namespace llds::pet {

class WatermarkMerger {
public:
    static constexpr std::uint64_t kClosed = std::numeric_limits<std::uint64_t>::max();

    // `per_source_capacity` bounds the hits buffered for each source.
    WatermarkMerger(std::size_t n_sources, std::size_t per_source_capacity)
        : cap_(next_pow2(per_source_capacity < 1 ? 1 : per_source_capacity)), mask_(cap_ - 1),
          src_(n_sources) {
        if (n_sources == 0) throw std::invalid_argument("WatermarkMerger: no sources");
        for (auto& s : src_) s.buf.resize(cap_);
    }

    // Buffer one hit. Returns false, without buffering it, if the source's
    // buffer is full or the hit breaks the source's own promise or order.
    bool add(std::size_t source, const Hit& h) noexcept {
        Source& s = src_[source];
        if (s.closed || h.tick < s.promise || h.tick < s.last_tick || s.tail - s.head == cap_) return false;
        s.buf[s.tail++ & mask_] = h;
        s.last_tick = h.tick;
        return true;
    }

    // The source promises that no future hit will have a tick below `p`.
    // Promises only move forward; an older promise is ignored.
    void promise(std::size_t source, std::uint64_t p) noexcept {
        Source& s = src_[source];
        if (!s.closed && p > s.promise) s.promise = p;
    }

    // No more hits from this source (normal end, or excluded after a fault).
    void close(std::size_t source) noexcept {
        src_[source].closed = true;
        src_[source].promise = kClosed;
    }

    std::uint64_t watermark() const noexcept {
        std::uint64_t w = kClosed;
        for (const auto& s : src_) w = s.promise < w ? s.promise : w;
        return w;
    }

    // Emit, in tick order, every buffered hit whose tick is below the
    // watermark (all of them once every source is closed). Ties between
    // sources are broken by source index, so the output is deterministic.
    template <class Sink>
    std::size_t drain(Sink&& sink) {
        const std::uint64_t w = watermark();
        std::size_t n = 0;
        for (;;) {
            Source* best = nullptr;
            for (auto& s : src_) {
                if (s.head == s.tail) continue;
                const Hit& h = s.buf[s.head & mask_];
                if (h.tick >= w && w != kClosed) continue;
                if (!best || h.tick < best->buf[best->head & mask_].tick) best = &s;
            }
            if (!best) return n;
            sink(best->buf[best->head++ & mask_]);
            ++n;
        }
    }

    std::size_t buffered(std::size_t source) const noexcept {
        return static_cast<std::size_t>(src_[source].tail - src_[source].head);
    }
    bool all_closed_and_empty() const noexcept {
        for (const auto& s : src_)
            if (!s.closed || s.head != s.tail) return false;
        return true;
    }

private:
    struct Source {
        std::vector<Hit> buf;
        std::uint64_t head = 0, tail = 0;
        std::uint64_t promise = 0;
        std::uint64_t last_tick = 0;
        bool closed = false;
    };
    std::size_t cap_;
    std::size_t mask_;
    std::vector<Source> src_;
};

}  // namespace llds::pet
