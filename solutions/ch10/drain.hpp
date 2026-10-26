// Exercise 10.1 (solution): a batching Disruptor consumer step.
#pragma once

#include <cstdint>
#include <vector>

#include <llds/queue/disruptor.hpp>

// Process every entry that is available to consumer `me` (whose
// dependencies are `deps`), calling handler(entry, sequence, end_of_batch)
// for each, then publish progress with ONE store. Returns the number of
// entries processed (0 if nothing was available). Never blocks.
template <class T, class Handler>
std::size_t drain(llds::Disruptor<T>& ring, llds::Sequence& me,
                  const std::vector<const llds::Sequence*>& deps, Handler&& handler) {
    const std::int64_t next = me.get() + 1;
    const std::int64_t avail = ring.available(deps);
    if (avail < next) return 0;
    for (std::int64_t s = next; s <= avail; ++s) handler(ring[s], s, s == avail);
    me.set(avail);  // a single release store for the whole batch
    return static_cast<std::size_t>(avail - next + 1);
}
