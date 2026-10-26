// Exercise 19.1 (solution): detect lost and late records from sequence numbers.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class SequenceTracker {
public:
    explicit SequenceTracker(std::size_t sources) : s_(sources) {}

    // Returns how many records are missing immediately before `seq`.
    std::uint32_t observe(std::size_t src, std::uint32_t seq) noexcept {
        State& st = s_[src];
        if (!st.started) {  // the first record synchronises: we cannot know what came before
            st.started = true;
            st.expect = seq + 1;
            return 0;
        }
        // Modular distance from the expected number. Unsigned subtraction is
        // exact modulo 2^32, so this is right across the wrap. Distances in
        // the upper half are read as "behind": a late or duplicate record.
        const std::uint32_t ahead = seq - st.expect;
        if (ahead >= 0x8000'0000u) {
            ++st.late;
            return 0;  // do not move `expect` backwards
        }
        st.missing += ahead;
        st.expect = seq + 1;  // wraps to 0 after 0xffffffff, as the source does
        return ahead;
    }

    // End of stream: the source says the next number it would have used.
    // Records missing at the very end leave no gap behind them, so only this
    // reveals them.
    std::uint32_t finish(std::size_t src, std::uint32_t next) noexcept {
        State& st = s_[src];
        if (!st.started) {  // never heard from: everything up to `next` is missing
            st.started = true;
            st.missing += next;
            st.expect = next;
            return next;
        }
        const std::uint32_t tail = next - st.expect;
        if (tail >= 0x8000'0000u) return 0;  // inconsistent final count: not a loss
        st.missing += tail;
        st.expect = next;
        return tail;
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
