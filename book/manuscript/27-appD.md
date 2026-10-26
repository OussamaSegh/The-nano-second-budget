## Appendix D. Solutions to the Exercises {#appD}

This appendix gives a solution or a worked answer for every exercise in the book. For the hands-on exercises, the complete reference code is in `solutions/chNN/` (and `solutions/fNN/` for Part 0) of the companion project, and the tests in `tests/exercises/` check it; here we show the essential part of each solution and discuss the choices. For hands-on exercises whose result depends on your machine, we describe what to expect and how to interpret it, and give the reference machine's figures where we have them.

### Chapter F1: Modern C++ Without Mystery

**1. Reserve versus resize.** `reserve(10)` allocates room for ten elements but creates none, so `size()` is 0 and `v[3]` refers to an element that does not exist; writing through it is undefined behaviour (it may appear to work, and a debug standard library will usually abort). Two correct versions: `std::vector<int> v(10); v[3] = 1;` (ten zero-initialised elements), or `std::vector<int> v; v.resize(10); v[3] = 1;`. A third, `v.assign(10, 0)`, is equivalent.

**2. Dangling views.** (a) Dangles: the span refers to the elements of the local `v`, which are destroyed when `f` returns. (b) Does not dangle by itself: the span refers to the caller's vector, and is valid as long as that vector lives and does not reallocate; it dangles if the caller passes a temporary, as in `g(std::vector<int>{1, 2})`, and keeps the result. (c) Does not dangle: string literals have static storage duration and live for the whole program.

**3. Zero is not absence.** If 0 means "not found", module 0 can never be found, or worse, a caller that forgets to check treats "not found" as module 0 and processes data against the wrong module. Rewrites: `std::optional<std::uint16_t> find_module(...)`, where the empty optional means not found; or `enum class FindStatus { found, no_such_crystal, map_not_loaded }; FindStatus find_module(..., std::uint16_t& out);`, which also says *why* nothing was found.

**4. Linkage.** Each translation unit compiles fine on its own. At link time, both object files contain a definition of `twice`, and the linker reports a multiple-definition error (the one-definition rule is violated). `#pragma once` only prevents a header from being included twice into the *same* translation unit; here it is included once into each of two. Mark the function `inline` (or `constexpr`, which implies inline for functions), or declare it in the header and define it in one `.cpp` file.

### Chapter F2: Ownership, RAII, Moves and Exceptions

**1. Lock leak.**

```cpp
std::lock_guard lock(m);
if (!valid(x)) return false;
apply(x);
return true;
```

The original leaks `m` on the early `return false` (the unlock is never reached) and if `apply(x)` (or `valid(x)`) throws. With the guard, both paths run its destructor.

**2. Destruction order.** Members are destroyed in reverse order of declaration, so `buffer_` is destroyed *first*, while the `jthread` is still running and reading it; only then does `worker_`'s destructor request a stop and join. The worker reads freed memory in between. Fixes: declare `buffer_` before `worker_` (so the thread is destroyed first), or, more robustly, give the class an explicit `stop()` that requests the stop and joins, and call it at the start of the destructor, so the order does not depend on declaration order at all.

**3. Shared is not safe.** The reference count is not a problem: `shared_ptr`'s control block is updated atomically, so copying and destroying the two handles concurrently is safe. The increment is a problem: `stats->count++` is a plain read-modify-write of the same `Stats` object from two threads, a data race. Shared ownership says nothing about the object's contents. Make `count` a `std::atomic`, protect it with a mutex, or give each thread its own counter and add them later.

**4. Hand-off.** *Owning buffer:* the producer owns the buffer until the push; the push transfers ownership to the queue (the mutex release in `push` and acquire in `pop` order it); the consumer owns it after `pop` and frees or recycles it. The producer starts a new buffer and never touches the old one again. *Copy into the queue:* the producer owns its reusable buffer throughout; the push copies the data into storage the queue owns; the producer may overwrite its buffer as soon as `push` returns. *Lend and return:* the producer owns the buffer until the push; the consumer owns it from `pop` until it returns it, for example by pushing its pointer onto a "free" queue; the producer may reuse it only after taking it back off the free queue. The event that transfers ownership back is the return, and the producer must have a policy for when no buffer has been returned (wait, drop, or fault).

### Chapter F3: Templates, Lambdas and Type Traits

**1. Instantiation.** Three classes: `ModRing<int, 8>`, `ModRing<int, 16>` and `ModRing<unsigned, 8>` are three distinct types, each compiled separately (and each member function only if used). A header-only library is recompiled in every translation unit that includes it and instantiated for every argument combination used, which costs compile time; but at run time each instantiation is ordinary, fully specialised code, often better optimised than a generic version would be.

**2. Choose the trait.** (a) `std::is_trivially_copyable_v<T>`: copying with `memcpy` is only valid for trivially copyable types. (b) `std::is_nothrow_copy_assignable_v<T>` (and, since slots are created up front, `std::is_nothrow_default_constructible_v<T>`): otherwise `try_push` could not honestly be `noexcept`, and a throw half-way through would leave a half-written slot. (c) `std::is_trivially_destructible_v<T>`: skipping the destructor is only harmless if the destructor does nothing.

**3. Capture bug.** `[&]` captures `i` by reference. Each thread reads `i` whenever it gets round to it, by which time the loop may have incremented it, or finished, in which case `i` no longer exists. Several threads may see the same value, some may see 8, and the program has a data race on `i`. The fix is to capture by value: `[=]`, or better, explicitly `[i]`.

**4. Callback cost.** With `template <class F>`, the compiler sees the lambda's body at the call site and typically inlines it into the loop: the loop body is a few compare-and-branch instructions with no call. With `std::function`, each invocation is an indirect call through the function object's stored pointer, which the compiler usually cannot inline or vectorise across; for a large capture it may also have allocated when the `std::function` was constructed. The difference per call is a few nanoseconds, which matters on a per-hit path and not on a per-batch one.

### Chapter F4: Bytes, Integers and Defensive Parsing

**1. Endianness by hand.** At offset 0: 16-bit 0x0201 = 513; 32-bit 0x04030201 = 67,305,985; 64-bit 0x0807060504030201. At offset 6 of an 8-byte buffer only two bytes remain, so a 32-bit read does not fit and `read_le<std::uint32_t>` returns `std::nullopt`.

**2. The overflowing bounds check.** With a 64-bit `size_t`, `at = 2^64 - 2` gives `at + 4 = 2`, which wraps around, and `2 > size` is false for any buffer of at least two bytes, so the check passes and the read goes wildly out of bounds. The safe form compares without adding to `at`: `if (at > size || size - at < 4)`.

**3. NaN through the gate.** For NaN, both `e < lo` and `e > hi` are false, so the `if` does not reject it and NaN is *kept*. Write the condition as the acceptance test and reject everything else: `if (!(e >= lo && e <= hi)) reject();`, which rejects NaN because both comparisons are false. Or check `std::isfinite(e)` explicitly first, which also documents the intent.

**4. Counter extension.** 2^32 ticks at 100 MHz is about 42.9 s. Keep a 64-bit `extended` value and the last raw 32-bit reading. On each new reading `raw`, compute `delta = raw - last_raw` in 32-bit unsigned arithmetic (correct across one wrap), add it to `extended`, and store `raw` as `last_raw`. The assumption is that consecutive readings are less than one full period apart (less than 42.9 s, with margin for reordering); if two readings can be further apart, a wrap is missed and the extended time silently jumps back by 42.9 s. Real systems guarantee the reading rate, for example with a periodic timestamp marker in the data stream.

**5. Hands-on: Exercise F4.1.** The reference solution, in `solutions/f04/le_parse.hpp`, is the code shown in the chapter. The test that catches most first attempts is `read_le<std::uint16_t>(b, SIZE_MAX)`: a check written as `at + sizeof(U) > in.size()` accepts it. The other common mistake is `v |= std::to_integer<unsigned>(b[i]) << (8 * i)` for a 64-bit `U`: the shift happens in 32-bit `unsigned`, and shifting by 32 or more is undefined (on x86 it usually shifts by the count modulo 32, so the high bytes overwrite the low ones). The test with eight bytes of 0xff catches it.

### Chapter F5: Threads and Data Races

**1. Is it a race?** (a) No: concurrent reads never conflict. (b) No: `v[0]` and `v[1]` are different memory locations, so the writes do not conflict (they may falsely share a cache line, which is a performance issue, not a correctness one). (c) Yes: `push_back` writes the vector's size and possibly reallocates, which conflicts with any concurrent read, and the read may also follow a pointer into freed storage. (d) No: the worker's completion synchronises with `join()`, so its writes happen before the main thread's read.

**2. Lost updates.** Two. The classic interleaving: thread A loads 0 and stalls. Thread B performs all but its last increment, so `count` is N − 1. A stores 1. B loads 1 for its last increment and stalls. A performs all its remaining increments. B stores 2. Final value 2. (Real compilers make far worse outcomes possible, because the race allows them to transform the loop, but 2 is the answer for the naive interleaving model.)

**3. The sleep fix.** A sleep creates no happens-before relation, so the read is still a data race and the program still has undefined behaviour: the compiler may, for example, keep the result in a register and never reload it. The test now passes because, in practice, 100 ms is long enough for the worker to finish on an idle machine. On a loaded machine, or under a sanitizer that slows the worker down, it fails again. The fix is `join()`, or a proper synchronisation (a future, a condition variable, or a release/acquire flag).

**4. Hands-on: Exercise F5.1.** The solution is in `solutions/f05/parallel_sum.hpp` and in the chapter. Splitting `n` items over `t` threads as `n / t` each, plus one more for the first `n % t` threads, gives parts that differ by at most one and handles `t > n` (some parts are empty). The test with ten values of 0xffffffff catches an `int` or 32-bit accumulator. With a plain `std::uint64_t` counter, TSan reports a data race on the counter, naming both the read and the write inside the loop and the two threads; the count is also usually wrong, but not always, which is why the report is the reliable signal.

### Chapter F6: Mutexes, Condition Variables and a Queue That Can Stop

**1. The racy snapshot.** Updates in the order `received`, then `accepted` or `rejected`, mean that at any instant `accepted + rejected <= received`. Reading `accepted` and `rejected` *first* and `received` *last* can only see `received` at least as large as the values that preceded it, so the reader cannot observe `accepted + rejected > received` (with seq_cst atomics, or with acquire loads matching release stores). In the opposite order, reading `received` first and the others later, the reader can see an old `received` and newer `accepted`, and the sum exceeds it. A consistent snapshot needs all three read and written under one mutex, or packed into one atomic word, or published together as one immutable object, or read only when the writer is stopped.

**2. Lost wake-up.** Without a predicate: (a) a spurious wake-up returns from `wait` with the queue still empty, and the consumer pops from an empty queue; (b) two consumers are woken by `notify_all`, the first takes the only item, and the second pops from an empty queue. And with `if` rather than a loop, a consumer that checks `empty()` before the producer pushes but calls `wait` after it notified (if the check was not under the same lock) sleeps forever. The predicate form re-checks under the lock every time and handles all three.

**3. Close bugs.** (a) A producer blocked in `push` on a full queue, waiting on `not_full_`, is never woken, so it never sees `closed_` and never returns; the thread that joins it hangs. (b) With two consumers blocked on an empty queue, `notify_one` wakes one; it sees closed and empty and returns, and the other sleeps forever. (c) Writing `closed_` without the mutex is a data race with the waiters' predicate reads. In practice a waiter can check the predicate (false), then the closing thread sets the flag and notifies, and only then does the waiter go to sleep: the notification is lost and the waiter never wakes. Setting the flag under the mutex makes "check predicate, then sleep" atomic with respect to the close.

**4. Hands-on: Exercise F6.1.** The solution is in `solutions/f06/closable_queue.hpp` and in the chapter. The test with a writer blocked on a full queue checks the result loosely on purpose: the writer may complete its push before `close()` runs, or be woken by it, and both are correct; what is checked is that every item that was accepted is drained, in order, and that nothing hangs. The four-by-four test checks per-producer FIFO order as seen by each consumer, which holds for a FIFO queue even though the interleaving between producers is arbitrary.

### Chapter F7: Atomics and Memory Ordering, a First Look

**1. Load plus store.** No data race: every access is atomic. But the result is not always 2000, because `load` and `store` are separate operations and increments are lost when both threads load the same value. The smallest possible result is 2, by the same interleaving as in Chapter F5, Exercise 2. Use `x.fetch_add(1)`.

**2. Which order?** (a) Relaxed: it is a statistic that publishes nothing, read at the end after a join or as an approximate figure. (b) Release: the store must make the configuration writes that precede it visible to whoever sees the flag. (c) Acquire: it pairs with the release, so the worker's reads of the configuration happen after the writes. Seq_cst would also be correct for (b) and (c), at some cost on ARM (Appendix A).

**3. Broken mailbox.** With a relaxed load, seeing `ready == true` creates no synchronisation, so the payload write does not happen before the payload read. The reader can see the flag and an old or partly written payload; on ARM this can really happen, on x86 the hardware order usually hides it but the compiler may still reorder the payload read before the flag load. TSan reports a race on `payload` because, by the language rules, the two accesses are not ordered, whatever the hardware did in that run.

**4. Two edges.** For `payload`: written by the producer, read by the consumer. Publication edge: the producer's `full.store(true, release)` after writing `payload`, read by the consumer's `full.load(acquire)` before reading it. Reuse edge: the consumer's `full.store(false, release)` after reading `payload`, read by the producer's `full.load(acquire)` before it writes the next message. For `full`: written by both sides, but never at the same time in a way that matters, because each side writes it only after observing the value the other side wrote; it is atomic, so there is no race. Exercise 3.1's solution (Chapter 3 section of this appendix) has exactly these orders.

### Chapter F8: A Ring Buffer You Can Explain Line by Line

**1. Draw it.** With `N = 3` (capacity 2), writing (read, write) and slot contents: start (0, 0) [–, –, –]; push A → (0, 1) [A, –, –]; push B → (0, 2) [A, B, –]; push C: `next = 0 == read`, fails, full. Pop → returns A, (1, 2) [–, B, –]; push C → (1, 0) [–, B, C]; pop → B, (2, 0); pop → C, (0, 0); pop: `read == write`, fails, empty.

**2. Swap two lines.** If `write_` is released before the slot is written, the consumer can acquire the new `write_`, see the ring non-empty, and copy the slot while the producer is still writing it (or before), reading stale or torn data. TSan reports it: the slot write and the slot read are no longer ordered by the release/acquire pair (the release now precedes the write), so they are conflicting unordered accesses.

**3. Relax the reuse edge.** With a relaxed load of `read_`, nothing in the language orders the consumer's copy out of a slot before the producer's next write to it: the two accesses become a data race, and the producer may in principle overwrite the slot while the consumer is still reading it. On x86 the hardware never reorders a load with a later store, so the bug cannot appear there at the hardware level. On ARM, the producer's store only happens if the comparison with `read_` succeeded, and current cores do not make a store visible before the branch it depends on is resolved, so it is very unlikely to appear either. That is what makes it dangerous: a latent bug that passes every test, which the language does not rule out (compilers are allowed to break control dependencies) and which ThreadSanitizer reports immediately.

**4. Hands-on: Exercise F8.1.** The solution is in `solutions/f08/mod_ring.hpp` and in the chapter. A sum is not enough because errors can cancel: if two values are delivered in the wrong order, or one value is duplicated and another lost such that the differences cancel, the sum is unchanged. Checking each value against its expected position catches loss, duplication and reordering.

### Chapter 1: What Low Latency Means

**1. Fan-out.** A request is fast only if all 50 calls are fast. The probability of that is 0.998⁵⁰ = e^(50 ln 0.998) ≈ e^(−0.1001) ≈ 0.905, so about **9.5%** of requests exceed 1 ms, although each call does so only 0.2% of the time. For 99% of requests to be fast, we need (1 − p)⁵⁰ = 0.99, so 1 − p = 0.99^(1/50) ≈ 0.999799 and p ≈ **0.02%**: each back end must be ten times better than it is. This is the reason fan-out services care about p99.9 and p99.99 of their dependencies, not their medians.

**2. Same mean, different tails.** Take 10,000 samples. Distribution A: 9,980 samples at 10 µs and 20 at 15 µs. Its median is 10 µs, its p99.9 is 15 µs and its mean is 10.01 µs. Distribution B: 5,000 samples at 10 µs, 4,985 at about 5.54 µs and 15 at 1,500 µs. Its median is still 10 µs (the 5,000th sample in sorted order), its mean is (50,000 + 27,600 + 22,500) / 10,000 = 10.01 µs, and its p99.9 is 1,500 µs, a hundred times A's. Mean and median say nothing about the tail. A benchmark that reports only those two numbers cannot distinguish a system that is always fine from one that stalls for 1.5 ms once in every 700 operations.

**3. Little's law.** W = L / λ = 600 / (2 × 10⁶ s^−1^) = **300 µs**. To bring the average below 50 µs at the same arrival rate, L must be at most λW = 2 × 10⁶ × 50 × 10^−6^ = **100 events**. In practice, that means the stage must drain faster, so that its queue stays short; the queue's *capacity* is irrelevant except as a bound.

**4. Budget arithmetic.** Computation is 4 × 300 ns = 1.2 µs and hand-offs 3 × 100 ns = 0.3 µs, so 1.5 µs is spent and **3.5 µs** remain for queueing and jitter. If each stage blocks on a condition variable when idle, each hand-off to an idle stage costs a wake-up of about 20 µs, and three of them cost 60 µs, twelve times the whole budget. At low load, when stages are usually idle, *every* event pays this. The design is impossible with blocking hand-offs; the stages must spin, or at least spin for a while before blocking (Chapters 9 and 17).

**5. Hands-on: your machine's jitter.** Expect CPU 0 to be the noisiest on most machines, since many interrupts and kernel threads default to it, and the highest-numbered CPUs to be quieter. On the reference VM, all CPUs showed maxima in the hundreds of microseconds to milliseconds, from the hypervisor. While a parallel build runs, p99.99 and max typically rise by one or two orders of magnitude, because the jitter thread now competes with compiler processes for its core: that is preemption, the largest source of jitter on a busy machine.

**6. Hands-on: what is in the tail?** Record the gaps in a fixed-size array (no allocation in the loop), with the timestamp of each, and keep the 20 largest. On an untuned machine, you typically find a periodic component at the scheduler tick (1 ms for `CONFIG_HZ=1000`, 4 ms for 250) whose gaps are a few microseconds, plus irregular larger gaps from interrupts, kernel threads and, in a VM, the hypervisor. Comparing `/proc/interrupts` before and after shows which interrupt lines (`LOC` local timer, `RES` rescheduling IPIs, `CAL` function-call IPIs, device interrupts) fired on the measured CPU during the run.

### Chapter 2: The Hardware Beneath the Code

**1. Read the plateau.** The pointer chase shows 1.8 ns up to 32 KiB, 5.7 ns from 64 KiB to 512 KiB, rising through 1 MiB to 25 to 39 ns from 2 to 8 MiB, then 115 ns and above. At 2.1 GHz, that is about 4 cycles for L1 (the published figure for recent Xeon cores is 5 cycles; the difference is within the loop's overlap), about 12 cycles for L2, and about 80 cycles for L3, with L1 at 32 to 48 KiB and L2 at about 2 MiB, consistent with Sapphire Rapids-class cores (48 KiB L1D, 2 MiB L2). The step at 16 MiB is not the end of L3 but the end of the TLB's reach (2,048 entries × 4 KiB = 8 MiB), as the chapter explains; the cloud VM's L3 is far larger than 16 MiB.

**2. MESI walk-through.**

| Step | A | B | Traffic |
|---|---|---|---|
| A reads | E | I | Read from memory |
| B reads | S | S | Line supplied to B |
| A writes | M | I | Invalidate B's copy |
| B reads | S | S | A supplies the line (and writes it back or keeps it as owner) |
| B writes | I | M | Invalidate A's copy |
| A writes | M | I | Read-for-ownership: line transferred from B, B invalidated |

One memory read and five coherence transactions for six accesses. Every write after the first read by another core costs a transaction, which is why alternating writers ("ping-pong") are expensive.

**3. Why 128?** Intel's L2 **spatial (adjacent-line) prefetcher** fetches the other 64-byte half of a 128-byte-aligned pair when one half is missed. Two variables 64 bytes apart in the same pair are on different lines, but a core that writes one tends to pull in the other, and a core writing the other then has to take it back, so the pair behaves partly like one shared line. Apple's M-series processors have genuinely 128-byte cache lines, so variables 64 bytes apart *are* on the same line there. Padding to 128 bytes avoids both effects.

**4. Padding trade-off.** 64 counters of 8 bytes take 512 bytes packed, 4 KiB padded to 64 bytes and 8 KiB padded to 128 bytes. The L1 cost is smaller than it looks: each thread touches only its own counter's line, so its L1 holds one line of the structure, whatever the padding; only the occasional reader that sums them touches all of it. For an 8-thread program, choose 128-byte padding (and size the structure for 8 slots, not 64, if the thread count is fixed). Packing would put eight counters on one line, and all eight threads would write it.

**5. Hands-on: Exercise 2.1, per-thread counters.** The solution (`solutions/ch02/padded_counters.hpp`) gives each counter its own 128-byte slot and replaces the RMW with a load and a store, which is correct because each counter has a single writer:

```cpp
void add(std::size_t slot, std::uint64_t n) noexcept {
    auto& c = slots_[slot].value;
    c.store(c.load(std::memory_order_relaxed) + n, std::memory_order_relaxed);
}

struct alignas(llds::kFalseSharingRange) Slot {
    std::atomic<std::uint64_t> value{0};
};
Slot slots_[N];
```

The counter remains a `std::atomic` so that the reader's concurrent `total()` is not a data race, but no `lock` prefix is emitted. With four threads, expect the starter to run about an order of magnitude slower than the solution: it combines false sharing with a locked RMW on every increment.

**6. Hands-on: hyper-threads.** Two hyper-threads of one core share its L1 and L2, so "false sharing" between them costs little or nothing: the line never leaves the core. Two separate cores must transfer the line through L3 or the interconnect on every write. The packed counters therefore look much better on an SMT pair than on two cores, and the padding matters only for the second pair. (Hyper-threads compete for execution resources instead, which slows both threads in a different way.)

**7. Hands-on: huge pages.** With 2 MiB pages, the TLB covers 2,048 × 2 MiB = 4 GiB, so the 16 MiB to 512 MiB rows drop back towards the L3 and DRAM latencies of the cache hierarchy itself, losing the page-walk component (often 20 to 50 ns per load). What remains above L3 size is the true DRAM latency.

### Chapter 3: The C++ Memory Model

**1. Name the edge.** The synchronises-with edge from the producer's release store of the flag to the consumer's load disappears, and with it the happens-before relation between the producer's write of the data and the consumer's read. The consumer may observe the flag set and stale data; formally, the plain data access is now a data race, so the behaviour is undefined. On x86, the hardware never reorders two loads, so you are unlikely to observe it, but the *compiler* is free to move the data load before the flag load, and at higher optimisation levels or after inlining it sometimes does.

**2. Store buffering by hand.** Thread 1 executes `x = 1`; the store enters store buffer 1. Thread 1 loads `y`; the load reads the cache, which holds 0. Thread 2 executes `y = 1` into store buffer 2 and loads `x` from the cache: 0. Only now do the buffers drain. Both loads returned 0, and each thread's own program order was respected locally. A `seq_cst` store is compiled to `xchg` (or `mov` followed by `mfence`); both drain the store buffer before any later load can execute, so each thread's store is globally visible before its load, and at least one load sees 1.

**3. Is this correct?** No. A relaxed store does not order the earlier writes to `*new_config` before it, so a reader that acquires the new pointer is not guaranteed to see the fields initialised. The minimum fix is to make the store `std::memory_order_release`. (The program also needs a reclamation story for the old configuration: Chapter 12.)

**4. Counting with relaxed.** All modifications of `hits` form a single **modification order**, and the read-read coherence rule says that if one load by a thread sees a value, a later load by the same thread cannot see an earlier value in that order. So the monitor can never print a smaller value than before. Relaxed RMWs are still atomic, and each one reads the value immediately before it in the modification order, so no increment is lost, and after all threads have been joined (which synchronises) the final value is exact.

**5. Dekker's mutual exclusion.** The pattern is a store followed by a load of a different variable on each side: it is the store-buffering test, and the forbidden outcome (both threads read `false`) is exactly both threads entering the critical section. Release/acquire does not forbid it. The flags must be stored and loaded with `seq_cst` (or the thread must execute a `seq_cst` fence between its store and its load). Real implementations also need a turn variable to avoid both threads backing off forever, which is Peterson's algorithm.

**6. Hands-on: Exercise 3.1, a one-slot mailbox.** The solution:

```cpp
bool try_put(const T& v) noexcept {
    if (full_.load(std::memory_order_acquire)) return false;  // pairs with take's release
    value_ = v;
    full_.store(true, std::memory_order_release);             // publish value_
    return true;
}
bool try_take(T& out) noexcept {
    if (!full_.load(std::memory_order_acquire)) return false; // pairs with put's release
    out = value_;
    full_.store(false, std::memory_order_release);            // hand the slot back
    return true;
}
```

Both directions need a release/acquire pair. The second pair (take's release, put's acquire) is easy to miss: it guarantees that the consumer's read of `value_` happens before the producer's next write to it. If you weaken put's final store to relaxed, the test usually still passes on x86, but ThreadSanitizer reports a race on `value_` immediately. Weakening the acquire in put makes TSan report the race between the consumer's read and the producer's overwrite.

**7. Hands-on: litmus tests on ARM.** With relaxed orders, message-passing violations are allowed on ARM and appear on some cores, usually rarely (perhaps a few per million trials, depending on the core and on timing); on others they may not appear at all in a short run, which does not make the code correct. With release/acquire, they must never appear. The store-buffering outcome appears with relaxed and release/acquire, as on x86.

### Chapter 4: Measuring Latency Honestly

**1. Timer floor.** On the reference machine, `rdtsc` costs about 18 ns and `rdtscp` about 23 ns when called back to back, and timing an almost empty operation with the library's `measure_each` gives a median of 6 ns: that is the overhead and resolution floor. An operation must take well over 50 ns or so before an individual measurement says more about it than about the timer. To study a 3 ns operation, time batches (say, 1,000 operations) and record the per-batch time divided by the batch size; that gives a distribution of batch means, which shows interference and tail events but not the variation between individual operations. Alternatively, count cycles and events with hardware performance counters over a long run.

**2. Histogram error.** 1,000,000 lies in [2¹⁹, 2²⁰) = [524,288, 1,048,576). That range is split into 64 sub-buckets of 8,192. The value's sub-bucket is number 58, which spans [999,424, 1,007,615]. A percentile is reported as the bucket's upper edge, so the reported value can exceed the true one by at most one bucket width. The worst case is at the start of a range, where the width is 1/64 of the value: a relative error of 1/64 ≈ **1.6%**.

**3. Coordinated omission arithmetic.** Intended requests: one per 100 µs for 60 s, or 600,000, in twelve 5-second periods of 50,000 each. In each period, one request is caught by the 50 ms stall; the closed-loop generator waits for it, so the 499 requests that should have been sent during the stall are never sent. The naive benchmark records about 49,501 samples per period, one of them 50 ms, so 12 of about 594,000 samples are slow (0.002%): its p99 and p99.9 are both **10 µs**. Measured open-loop from the intended send times, each period has 500 requests affected by the stall, with latencies spread evenly from about 50 ms down to 0.1 ms, which is 1% of all requests. The true p99 is therefore right at the boundary, about 10 µs (the 49,500th of 50,000 samples), with p99.5 already around 25 ms, and the true p99.9 is the 50th-largest sample of each period, about **45 ms**. The naive benchmark omitted 12 × 499 = **5,988** samples.

**4. Spot the mistake.** "100 million messages in 1.5 seconds" is a throughput (67 M/s), and its reciprocal (15 ns) is the *average spacing* of messages, not the latency of any message; with batching and pipelining, each message may have spent microseconds in the queue. The claim reports no distribution, no percentiles and no maximum; it does not say whether producer and consumer were on different cores, whether the queue was full (in which case latency is the queue depth divided by the rate, by Little's law), how big the messages were, or what machine it ran on; and it was probably measured flooded, so coordinated omission is irrelevant only because nothing was measured per message. Ask for the round-trip latency distribution (p50 to p99.99 and max) at several paced rates below capacity, with producer and consumer pinned to named cores, plus the throughput at saturation as a separate figure.

**5. Hands-on: Exercises 4.1 and 4.2.** The solutions:

```cpp
inline std::uint64_t percentile_sorted(const std::vector<std::uint64_t>& sorted, double p) {
    if (sorted.empty()) throw std::invalid_argument("empty sample");
    if (p <= 0.0) return sorted.front();
    if (p >= 100.0) return sorted.back();
    const auto rank = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(sorted.size())));
    return sorted[rank - 1];
}

inline void record_corrected(llds::Histogram& h, std::uint64_t value, std::uint64_t expected_interval) {
    h.record(value);
    if (expected_interval == 0 || value <= expected_interval) return;
    for (std::uint64_t missing = value - expected_interval; missing >= expected_interval;
         missing -= expected_interval)
        h.record(missing);
}
```

The nearest-rank definition returns an actual sample, never an interpolation, which is what latency reports should do. `record_corrected` is HdrHistogram's `recordValueWithExpectedInterval`: a sample of 50 ms with a 100 µs interval also records 49.9 ms, 49.8 ms, and so on, for the requests that would have been waiting behind it.

**6. Hands-on: perf.** In the DRAM-sized rows of the pointer chase, IPC is very low (well below 0.1): each load depends on the previous one, so the core waits for memory with almost nothing to do. `perf c2c report` shows the falsely shared line with a high count of "HITM" (hit in a modified line in another core's cache) events, and lists the offsets within the line accessed by each thread, which identifies the two counters. On many cloud VMs, the required events are not exposed to the guest.

### Chapter 5: Locks and Their Costs

**1. Count the transfers.** With TAS, each waiting thread's `exchange` needs the line in exclusive state, so the line moves from core to core continuously, about one transfer every 50 ns, or **about 20 transfers per microsecond**, regardless of how many waiters there are, and all of them useless. Worse, the holder's unlock must wait its turn for the line. With TTAS, waiters spin on a *load*, so they share the line in S state and generate no traffic while the lock is held. On release, there is one invalidation, followed by a burst in which every waiter reloads and races to `exchange`, which is the thundering herd that backoff and queue locks reduce.

**2. MCS window.** The holder, in `unlock`, reads `next == nullptr`, while a new thread has already exchanged itself into `tail_` but has not yet linked itself into the holder's `next`. The holder's CAS on `tail_` (expecting itself) fails because `tail_` is now the new thread; it must then wait until `next` is set and hand the lock over. If `unlock` simply stored `nullptr` to `tail_`, the new waiter would be dropped from the queue: it would link itself after a node that no longer owns the lock and spin forever, while the next arriving thread would find `tail_ == nullptr` and take the lock. One waiter starves forever, and later waiters queue behind it.

**3. Ticket overflow.** Wrap-around is harmless as long as the comparison is equality on unsigned counters of the same width: tickets and the "now serving" counter wrap together. The comparison stays correct as long as fewer than 2³² threads are waiting at once, since otherwise two waiters would hold the same ticket value. That is not a practical limit; a 16-bit or 8-bit ticket lock (as in some kernels) limits the number of waiters accordingly.

**4. Unfairness, measured.** Give each thread a small array of counters, one per 10 ms window, incremented after each acquisition, and print them afterwards. With backoff, expect some windows in which one thread makes almost all acquisitions: the thread that just released is the one most likely to win again, because the line is still in its cache and the other threads are backing off. The reference run hints at this: TTAS with backoff had by far the best throughput with four threads (15.7 M/s against 1.9 M/s for the ticket lock), the usual signature of unfairness. The ticket lock's windows are nearly equal, because it hands the lock out in arrival order.

**5. Hands-on: bigger machine.** Results vary, but on two-socket machines MCS typically overtakes TTAS with backoff on p99.9 latency at around 8 to 16 threads, and on throughput a little later, when the cost of global spinning on one line across sockets exceeds the cost of MCS's extra atomic operations. On a single socket with few cores, as on the reference VM, it may never overtake.

**6. Hands-on: Exercise 5.1, a reader-writer spinlock.** The solution uses one 32-bit word: bit 0 for the writer, bit 1 for "a writer is waiting", and the remaining bits as a reader count in units of 4. A writer that cannot get the lock sets the waiting bit, which makes `try_lock_shared` refuse new readers, so a stream of readers cannot starve it:

```cpp
void lock() noexcept {
    for (;;) {
        auto s = state_.load(std::memory_order_relaxed);
        if ((s & ~kWaiting) == 0) {
            if (state_.compare_exchange_weak(s, kWriter, std::memory_order_acquire,
                                             std::memory_order_relaxed))
                return;
            continue;
        }
        if (!(s & kWaiting)) state_.fetch_or(kWaiting, std::memory_order_relaxed);
        llds::cpu_relax();
    }
}
bool try_lock_shared() noexcept {
    auto s = state_.load(std::memory_order_relaxed);
    while (!(s & (kWriter | kWaiting))) {
        if (state_.compare_exchange_weak(s, s + kReader, std::memory_order_acquire,
                                         std::memory_order_relaxed))
            return true;
    }
    return false;
}
void unlock() noexcept { state_.fetch_and(~kWriter, std::memory_order_release); }
void unlock_shared() noexcept { state_.fetch_sub(kReader, std::memory_order_release); }
```

The writer clears the waiting bit when it takes the lock (the CAS stores exactly `kWriter`); if several writers wait, the others set it again on their next iteration. Compared with the seqlock in `ch07_seqlock`, expect reader throughput to fall sharply with more readers, since every reader writes the lock word: the reason Chapter 7 exists.

**7. Preemption.** With more spinning threads than cores, the lock holder is sometimes preempted while waiters spin on the other cores, burning their whole time slice. Maximum latencies of the spinlocks rise to several milliseconds (scheduler time slices), and throughput collapses. `std::mutex` degrades gracefully: its waiters sleep in the kernel, freeing the cores for the holder to run and release the lock. This is why user-space spinlocks are only appropriate when threads have cores of their own.

### Chapter 6: Atomics, CAS and Lock-Free Basics

**1. Lock-free or not?** (a) A CAS loop counter is **lock-free** but not wait-free: some thread's CAS always succeeds, but a particular thread can fail indefinitely. (b) `fetch_add` on x86 is a single `lock xadd`, which completes in a bounded number of steps whatever other threads do: **wait-free** (in practice, subject to the hardware's arbitration being fair). (c) The TTAS spinlock is **blocking**: a holder that stops running stops everyone. (d) A seqlock reader is **blocking** too: a writer stalled in the middle of a write makes every reader retry forever. (e) `TaggedStack::push` is **lock-free**: a CAS loop on the head.

**2. ABA with a single popper?** No. For the popper's CAS to suffer ABA, the head would have to change from A and back to A between its read and its CAS, which requires A to be popped (only the popper pops, and it is busy) and pushed again. Pushers can make the head change, but not back to A. Pushers' own CASes cannot suffer harmful ABA either: a push only requires that the head equals the value it stored in `node->next`, and if it does, the push is correct, whatever happened in between. This is why single-consumer stacks and queues can use simpler algorithms.

**3. Tag width.** A 16-bit tag wraps after 65,536 operations, which at 50 million operations a second is **1.3 ms**. A thread preempted between reading the head and its CAS for a multiple of that (with the same node back at the head) would see a matching tag. Preemptions of a millisecond or more are routine, so although each coincidence is unlikely, across a fleet running for months it becomes a real possibility. An 8-bit tag wraps every 256 operations, about **5 µs**, which is far too short. Use 32 or more bits (which the 16-byte CAS or pointer-compression schemes allow), or a reclamation scheme that prevents reuse.

**4. Spurious failure.** With `compare_exchange_strong`, the loop body is the same, but `strong` on an LL/SC architecture such as ARMv8.0 is itself a loop that retries the load-exclusive/store-exclusive pair when the store-exclusive fails spuriously. Inside our own retry loop, that is a redundant inner loop with an extra comparison and branch. On x86, `lock cmpxchg` never fails spuriously, so both compile to the same instruction.

**5. Hands-on: Exercises 6.1 and 6.2.** The solutions:

```cpp
template <class T>
T fetch_max(std::atomic<T>& a, T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    T cur = a.load(std::memory_order_relaxed);
    while (cur < v && !a.compare_exchange_weak(cur, v, order, std::memory_order_relaxed)) {
    }
    return cur;
}

inline bool try_increment_below(std::atomic<std::uint32_t>& count, std::uint32_t limit) noexcept {
    std::uint32_t cur = count.load(std::memory_order_relaxed);
    do {
        if (cur >= limit) return false;
    } while (!count.compare_exchange_weak(cur, cur + 1, std::memory_order_acquire,
                                          std::memory_order_relaxed));
    return true;
}
```

In both, the condition is re-checked against the value the failed CAS loaded into `cur`, so the decision and the update are made on the same value. `fetch_max` exits without writing when the current value is already at least `v`: a CAS that "succeeds" by writing the same value would still take the line exclusively.

**6. Hands-on: break the stack.** Without tags, the test's runs fail intermittently: on a quiet multicore machine perhaps once in tens or hundreds of runs, because the window between reading `next` and the CAS is a few nanoseconds. A `yield()` in that window widens it to a scheduler time slice, and the corruption (lost nodes, or a node appearing twice) then shows up within a few runs. This is the standard technique for confirming a suspected race: widen the window and see whether the failure rate rises.

**7. Contention scaling.** Padded per-thread counters stay at the single-thread cost per increment (a few nanoseconds) at every thread count, while the shared `fetch_add` and CAS versions rise as in the chapter's table (29 ns and 51 ns of wall time per increment at four threads on the reference machine). Plotted, the per-thread version is a flat line and the shared versions rise with each added thread, until the cost of the line transfer dominates.

### Chapter 7: Seqlocks and Publishing State

**1. Why a fence?** A release store orders the operations *before* it; the data stores come *after* the first counter store, so a release store does not stop them from becoming visible first. Execution: the writer's first data store becomes visible while the odd counter value is still invisible. A reader loads the counter (still the old even value `s`), loads the first field (new) and the second field (old, not yet written), then loads the counter again: still `s`, because neither counter store is visible yet. Both loads agree and are even, and the reader returns a torn value. On x86, the hardware keeps stores in order, so only the compiler can produce this; on ARM, the hardware can too.

**2. Odd counter.** A reader could start while a write is in progress: it reads the odd value `s + 1`, copies fields while the writer is changing them, and reads `s + 1` again if the writer has not finished. `s1 == s2` holds, and the reader accepts a value that is half old and half new. The odd check is what rejects copies taken during a write.

**3. Two writers.** Both writers read `s`, both store `s + 1`, and both start writing fields. Writer 1 finishes and stores `s + 2`. A reader now loads `s + 2` (even), copies fields while writer 2 is still storing its values, and loads the counter again: writer 2 has stored `s + 2` (the same value, computed from the same `s`), so the counters match and the reader accepts a mixture of the two writers' data. Concurrent writers must be serialised by a lock, or the counter updated with an RMW and the protocol changed accordingly.

**4. Double buffering.**

```text
data[2]; atomic<int> current = 0
writer:  n = 1 - current; write data[n]; current.store(n, release)
reader:  i = current.load(acquire); copy data[i]
```

If the writer updates twice while a slow reader is copying `data[i]`, the second update writes into `data[i]` (since it alternates), and the reader's copy is torn, with nothing to detect it. Fixing that requires the writer to know whether readers are still using a buffer (reference counts, more buffers, or reclamation), which costs reader-side writes. The seqlock does not prevent the overlap either, but it *detects* it with the counter and retries, so readers never write anything shared.

**5. Hands-on: Exercise 7.1, a seqlock from scratch.**

```cpp
void publish(const Vitals4& v) noexcept {
    const auto s = seq_.load(std::memory_order_relaxed);
    seq_.store(s + 1, std::memory_order_relaxed);         // odd = writing
    std::atomic_thread_fence(std::memory_order_release);  // seq store before data stores
    hr_.store(v.heart_rate, std::memory_order_relaxed);
    spo2_.store(v.spo2, std::memory_order_relaxed);
    rr_.store(v.resp_rate, std::memory_order_relaxed);
    ts_.store(v.timestamp, std::memory_order_relaxed);
    seq_.store(s + 2, std::memory_order_release);         // data stores before seq store
}

Vitals4 read() const noexcept {
    for (;;) {
        const auto s1 = seq_.load(std::memory_order_acquire);
        if (s1 & 1) { llds::cpu_relax(); continue; }
        Vitals4 v{hr_.load(std::memory_order_relaxed), spo2_.load(std::memory_order_relaxed),
                  rr_.load(std::memory_order_relaxed), ts_.load(std::memory_order_relaxed)};
        std::atomic_thread_fence(std::memory_order_acquire);  // data loads before s2
        if (seq_.load(std::memory_order_relaxed) == s1) return v;
    }
}
```

Removing a fence on x86 usually does not make the test fail, because x86 does not reorder stores with stores or loads with loads, and GCC happens not to reorder these accesses. That is the lesson of Chapter 16 in miniature: the test's silence is not evidence. On ARM, removing either fence produces torn reads quickly.

**6. Hands-on: writer pressure.** With the writer updating continuously, a large fraction of reads overlap a write, and readers retry: reader throughput drops, and the retry rate can approach one retry per read or more, depending on how long a copy takes relative to a write. The writer itself is unaffected. A seqlock assumes writes are short and not too frequent compared with reads; under constant writing, readers can in principle starve, which a retry counter makes visible.

### Chapter 8: The SPSC Ring Buffer

**1. Why no RMW?** Each index has a single writer: only the producer writes `tail_`, only the consumer writes `head_`. A single writer can update its own index with a plain store, since no other thread can interleave a write. With two producers: both load `tail_ = t`, both see room, both write slot `t`, and both store `t + 1`. One element overwrote the other, and the index advanced once: an element is lost.

**2. Wasted slot.** Alternatives: keep a separate count of elements (which both sides must update, so it needs an RMW and becomes a contended line); keep a per-slot "full" flag (as Vyukov's queue does with sequence numbers); or use indices that run over twice the capacity, distinguishing full and empty by the extra bit. Free-running 64-bit indices are the simplest: `tail - head` is the number of elements, full is `tail - head == capacity`, empty is `tail == head`, the slot is `index & mask`, and 64-bit counters never wrap in practice.

**3. Memory orders.** Yes to both: a relaxed load followed by an acquire fence is at least as strong as an acquire load, and a release fence followed by a relaxed store is at least as strong as a release store (the fence orders *all* earlier operations against *all* later stores). On x86, all four compile to plain `mov` instructions, so the generated code is identical. On ARM, the fences become `dmb` barriers, which are usually more expensive than the `ldar`/`stlr` instructions the plain acquire and release operations use.

**4. Slot padding.** In the throughput regime (flooded), each element now costs a full cache line transfer instead of an eighth of one, so expect throughput to fall several-fold. In the round-trip regime, only one element is in flight at a time, so each round trip transfers one data line (plus index lines) either way, and latency hardly changes. Padding slots makes sense only for large elements or for designs in which producer and consumer would otherwise write neighbouring slots at the same moment.

**5. Hands-on: Exercise 8.1, batching.**

```cpp
std::size_t push_batch(const T* items, std::size_t n) noexcept {
    const auto t = tail_.load(std::memory_order_relaxed);
    auto free_slots = cap_ - (t - head_cache_);
    if (free_slots < n) {
        head_cache_ = head_.load(std::memory_order_acquire);
        free_slots = cap_ - (t - head_cache_);
    }
    const auto k = std::min(n, static_cast<std::size_t>(free_slots));
    for (std::size_t i = 0; i < k; ++i) buf_[(t + i) & mask_] = items[i];
    if (k) tail_.store(t + k, std::memory_order_release);  // one publish per batch
    return k;
}
```

`pop_batch` is the mirror image, with `tail_cache_`. Batches of 16 typically raise throughput well above v5's (one index store and at most one remote index load per 16 items) and narrow the run-to-run range, because the bimodal behaviour of v5 comes from how often the two sides' index lines bounce. Latency for the *first* item of a batch is unchanged; for later ones it rises by the time the batch takes to fill.

**6. Hands-on: measure the hand-off.** Expect two hyper-threads of one core to show the shortest round trip (the lines never leave the core's L1/L2: tens of nanoseconds), two cores of one socket a few hundred nanoseconds (the reference VM's 475 ns median is typical of a cloud VM), and cores on different sockets roughly 1.5 to 2 times more again. On large chips with several L3 domains (AMD's CCDs, Intel's sub-NUMA clusters), there is an additional step between cores in the same and different domains.

**7. Hands-on: a full ring is a long wait.** Each ping now waits behind 500 earlier elements, so the measured round trip grows by roughly 500 times the per-element service time, from hundreds of nanoseconds to tens of microseconds. By Little's law, the queue holds L = 500 items, and with the consumer serving at rate λ, each item waits W = L / λ. Queue depth, not the data structure's speed, sets the latency.

### Chapter 9: MPMC Queues

**1. Three-way comparison.** For a consumer at position *pos*, `diff = seq − (pos + 1)`. **Zero**: a producer has stored `seq = pos + 1`, so the cell holds this lap's element and the consumer may try to claim it with a CAS on `dequeue_pos`. **Negative**: the cell's sequence is still `pos` (or earlier): no producer has published this position yet, so the queue is empty (or a producer is in the middle of writing it). **Positive**: another consumer has already taken this position and set the cell's sequence for the next lap; our view of `dequeue_pos` is stale, so reload it and retry.

**2. Signed difference.** The positions and sequence numbers are free-running counters that will be compared near each other. Converting their difference to a signed integer gives the right answer even across wrap-around, as long as the two values are less than 2⁶³ apart, because subtraction is exact modulo 2⁶⁴. A direct unsigned comparison such as `seq < pos + 1` would give the wrong answer when one value has wrapped and the other has not.

**3. Stalled producer.** Consumers that reach position *pos* see a negative `diff` and report the queue empty, even if later positions have been filled by other producers: those elements are invisible until the stalled producer publishes. Once producers have gone a full lap and arrive at position *pos + capacity*, they find the cell's sequence still at *pos* instead of *pos + capacity*, see a negative `diff`, and report the queue full. The whole queue is stuck until the stalled producer runs. This is why Vyukov's queue is not, strictly, lock-free, although it behaves as if it were while every thread keeps running.

**4. Helping.** Without helping, an enqueuer that finds `tail->next` non-null must wait for the thread that linked that node to swing `tail`. If that thread is preempted between its two steps (the CAS that links the node, and the CAS that advances `tail`), every other enqueuer spins until it runs again: one stalled thread blocks all others, so the queue is not lock-free. With helping, any thread that sees a lagging tail advances it itself and continues.

**5. Dummy node.** With a dummy, `head` and `tail` always point to a node, and the queue is empty when `head->next` is null. Enqueue only touches `tail` and the last node's `next`; dequeue only touches `head`. Without a dummy, an empty queue has `head == tail == nullptr`, and the transitions between empty and one element must update both pointers at once, which a single-word CAS cannot do. A dequeue that removes the last element and a concurrent enqueue that adds one would race on both pointers and could lose the new element or leave `tail` pointing at freed memory.

**6. Hands-on: Exercise 9.1, a blocking queue.** The solution spins up to 200 times, then sleeps on an **epoch counter** with C++20 `atomic::wait`, re-checking the queue after reading the epoch so that a wake-up between the check and the wait cannot be lost:

```cpp
void push(T v) {
    for (int spin = 0;; ++spin) {
        if (q_.try_push(std::move(v))) break;
        if (spin < kSpins) { llds::cpu_relax(); continue; }
        const auto e = popped_.load(std::memory_order_acquire);  // read the epoch first
        if (q_.try_push(std::move(v))) break;                    // then re-check
        full_waiters_.fetch_add(1, std::memory_order_seq_cst);
        popped_.wait(e, std::memory_order_seq_cst);              // sleeps only if unchanged
        full_waiters_.fetch_sub(1, std::memory_order_relaxed);
    }
    pushed_.fetch_add(1, std::memory_order_seq_cst);
    if (empty_waiters_.load(std::memory_order_seq_cst)) pushed_.notify_all();
}
```

`pop` is symmetric. The waiter counters implement "only notify if someone is waiting": the uncontended path makes no system call. The `seq_cst` orders matter here, because the pattern is store-buffering again: a waiter increments its counter and then checks the epoch, while the notifier bumps the epoch and then checks the counter. With `strace -c`, the version without the waiter check shows a `futex` call per operation (millions in the stress test); with it, `futex` calls occur only when a side actually sleeps.

**7. Hands-on: per-producer rings.**

```cpp
template <class T, std::size_t P>
class MultiSpsc {
public:
    explicit MultiSpsc(std::size_t cap) { for (auto& r : rings_) r = std::make_unique<llds::SpscRing<T>>(cap); }
    llds::SpscRing<T>& producer(std::size_t i) { return *rings_[i]; }
    bool try_pop(T& out) {
        for (std::size_t k = 0; k < P; ++k) {
            auto& r = *rings_[next_];
            next_ = (next_ + 1) % P;
            if (r.try_pop(out)) return true;
        }
        return false;
    }
private:
    std::array<std::unique_ptr<llds::SpscRing<T>>, P> rings_;
    std::size_t next_ = 0;   // consumer-private
};
```

For three producers and one consumer, expect it to beat Vyukov's queue on throughput and tail latency, because producers never contend with each other. The cost is that there is no global FIFO order across producers, only per producer, and an idle consumer polls P rings instead of one.
