// Exercise 10.1: a batching Disruptor consumer step.
//
// Implement drain(): process every entry currently available to consumer
// `me` (given its dependencies), in order, calling
//     handler(entry, sequence, end_of_batch)
// for each one, and then publish progress with ONE store to `me`.
// Return how many entries were processed. Never block.
//
// Hints: me.get() is the last sequence this consumer finished (-1 at the
// start). ring.available(deps) is the highest sequence it may read.
#pragma once

#include <cstdint>
#include <vector>

#include <llds/queue/disruptor.hpp>

template <class T, class Handler>
std::size_t drain(llds::Disruptor<T>& ring, llds::Sequence& me,
                  const std::vector<const llds::Sequence*>& deps, Handler&& handler) {
    // TODO
    (void)ring;
    (void)me;
    (void)deps;
    (void)handler;
    return 0;
}
