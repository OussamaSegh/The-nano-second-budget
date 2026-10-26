// Exercise 19.1: detect lost and late records from sequence numbers.
//
// Each source numbers its records 0, 1, 2, ... in a 32-bit counter that
// wraps from 0xffffffff to 0. The tracker keeps, per source, the next
// expected number. The first record seen from a source synchronises the
// tracker (it may have attached mid-stream), so nothing is counted missing
// before it.
//
//   observe(src, seq) returns the number of records missing immediately
//     before `seq` and adds it to missing(src). A record that is *behind*
//     the expected number (more than 2^31 behind, modulo 2^32, counts as
//     ahead) is late or a duplicate: count it in late(src), return 0, and do
//     not move the expected number backwards.
//   finish(src, next) is called at the end of the stream with the number the
//     source would have used next; it returns, and counts, the records
//     missing at the very end (all `next` of them if the source was never
//     heard from).
//
// The arithmetic must be right across the wrap. No signed overflow.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class SequenceTracker {
public:
    explicit SequenceTracker(std::size_t sources) : s_(sources) {}

    std::uint32_t observe(std::size_t src, std::uint32_t seq) noexcept {
        // TODO
        (void)src;
        (void)seq;
        return 0;
    }

    std::uint32_t finish(std::size_t src, std::uint32_t next) noexcept {
        // TODO
        (void)src;
        (void)next;
        return 0;
    }

    std::uint64_t missing(std::size_t src) const noexcept { return s_[src].missing; }
    std::uint64_t late(std::size_t src) const noexcept { return s_[src].late; }

private:
    struct State {
        bool started = false;
        std::uint32_t expect = 0;
        std::uint64_t missing = 0;
        std::uint64_t late = 0;
    };
    std::vector<State> s_;
};
