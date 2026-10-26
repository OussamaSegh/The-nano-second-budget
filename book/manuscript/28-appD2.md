### Chapter 10: The Disruptor

**1. Gating.** `process` declares `parse` as its dependency, so it only reads entries up to the value of `parse`'s sequence it has observed, and it sets its own sequence to at most that value. `parse`'s sequence only increases. So at every moment, `process_seq ≤ parse_seq`, and therefore min(`process_seq`, `journal_seq`) ≤ min(`parse_seq`, `process_seq`, `journal_seq`). Gating on the two leaves already guarantees that `parse` has finished every entry the producer is about to overwrite.

**2. Wrap point.** Claiming sequence 5,001 reuses slot 5,001 mod 1,024, which last held sequence 5,001 − 1,024 = **3,977**. Every gating consumer must have finished 3,977, so the condition for claiming n more entries is `claimed_ + n − capacity ≤ min_gating`.

**3. Multi-producer.** With `fetch_add` on a shared claim counter, producers get distinct sequences but finish writing in any order. If producer A claims 10 and producer B claims 11, and B finishes first and advances the cursor to 11, consumers will read entry 10 before A has written it. A single cursor cannot express "11 is ready but 10 is not". The fix is per-entry state: each entry carries the sequence it was last published with (or LMAX's per-slot "available" flag, holding the lap number). A consumer reads entry *s* only after checking that its published sequence equals *s*, and the highest contiguous available sequence is found by scanning forward from the consumer's position. This is the same idea as the per-cell sequence numbers in Vyukov's queue.

**4. Work splitting.** Give each of the three workers a consumer sequence that depends only on the producer, have each read every entry but process only those with `s % 3 == i`, and advance its sequence over all of them. The gating set is all three workers. If one worker is slow, the others continue until the ring is about to wrap, and then the producer waits for the slow one: every item is delayed, not just the slow worker's third. (LMAX's `WorkerPool` instead lets workers claim entries from a shared sequence with a CAS, which balances load at the cost of a contended counter.)

**5. Hands-on: Exercise 10.1, a batching consumer step.**

```cpp
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
```

The end-of-batch flag lets handlers defer expensive work (flushing a journal, sending a network packet) to the end of a batch, which is how the Disruptor's natural batching turns into fewer system calls.

**6. Hands-on: a slow journal.** At one event per microsecond, a 1 ms sleep lets 1,000 events accumulate. With a ring of 1,024 entries, that is almost a full lap, so the producer gates on the journal, and the process stage's latency spikes to about a millisecond once every 10,000 events (1% of events see some of the delay). Removing `journal_seq` from the gating set removes the spikes, but the producer may then overwrite entries the journal has not yet read: the journal silently records wrong data, or reads an entry while it is being written (a data race). A consumer outside the gating set must detect overruns, for example by checking a sequence number stored in each entry.

### Chapter 11: Memory Without malloc

**1. Intrusive list.** In `FixedPool`, only one thread ever touches the pool, so a slot's `next` pointer is read only while the slot is free, by the thread that owns the pool. In `LockFreePool`, a thread popping the free list can read `head->next` after another thread has popped the same slot and the user has already constructed an object over it; if `next` shared storage with the object, that read would return part of the user's object (and is a data race). That is why `LockFreePool` keeps its link beside the storage. The union in `FixedPool` relies on the object's lifetime ending (`destroy` runs the destructor) before the slot is reused as a link, on the slot being at least as large and as aligned as both members, and on nobody using a pointer to the object after `destroy`.

**2. Exhaustion policy.** (a) **Drop** the packet and count the drop: latency stays bounded, but data is lost, and the handler must detect the gap (sequence numbers) and recover, for example by requesting a snapshot. (b) **Fall back to the heap**: correctness is kept, but an allocation, possibly with a page fault, lands on the hot path at exactly the moment the system is busiest. (c) **Wait** until a buffer is returned: correct, and it applies back-pressure, but latency is unbounded, and if the consumer that returns buffers is itself waiting for the producer, it deadlocks. Pools should be sized so that exhaustion indicates a fault, and the policy chosen should be the one whose failure is safest for the application.

**3. Return ring capacity.** At most all 2,048 buffers can be waiting in the return ring at once (if the producer stops taking them back), so a return ring of **2,048** entries guarantees that the consumer's push never fails. The forward ring needs no such guarantee: if it is full, the producer waits or drops, which is back-pressure. In the example it holds 512. To make the producer's push never fail as well, it would also need 2,048 entries.

**4. Arena and destructors.** `reset()` discards the arena's memory without running destructors, so each `std::string` longer than its small-string buffer leaks the heap block it allocated, which was never in the arena. Fixes: use `std::pmr::string` with the arena as its memory resource, so that the strings' storage also comes from the arena and is discarded with it; or keep a list of destructors registered at construction and run them in `reset()`; or avoid owning types entirely and store `std::string_view`s into the message buffer, or fixed-size character arrays.

**5. Hands-on: Exercise 11.1, a fixed pool.**

```cpp
explicit FixedPool(std::size_t capacity) : slots_(new Slot[capacity]), capacity_(capacity) {
    for (std::size_t i = 0; i < capacity; ++i) slots_[i].next = i + 1 < capacity ? &slots_[i + 1] : nullptr;
    free_ = capacity ? &slots_[0] : nullptr;
}
void* allocate() noexcept {
    Slot* s = free_;
    if (!s) return nullptr;
    free_ = s->next;
    ++in_use_;
    return s->storage;
}
void deallocate(void* p) noexcept {
    Slot* s = reinterpret_cast<Slot*>(p);
    s->next = free_;
    free_ = s;
    --in_use_;
}
template <class... Args>
T* create(Args&&... args) {
    void* m = allocate();
    if (!m) return nullptr;
    try {
        return ::new (m) T(std::forward<Args>(args)...);
    } catch (...) {
        deallocate(m);   // a throwing constructor must not leak the slot
        throw;
    }
}
void destroy(T* p) noexcept {
    if (!p) return;
    p->~T();
    deallocate(p);
}
```

In the same-thread benchmark, expect this pool to be the fastest allocator after the bump arena: a few nanoseconds per allocate-free pair, with no atomic operations at all.

**6. Hands-on: per-thread caches.** With a 32-slot cache refilled and flushed 16 at a time, the shared stack's head is touched once per 16 operations, so the contended-line cost falls by about 16 times, and the median comes close to the return-ring design. It does not match it: the producer's cache runs dry and the consumer's fills up at a steady rate, so every 16th operation on each side still performs a contended exchange with the shared stack, which shows up in the p99 and p99.9; and objects still move between the two threads' caches in a pattern the hardware cannot predict. The return ring makes the whole round trip a pair of SPSC transfers with no shared RMW at all.

**7. Hands-on: count the faults.** The result is a useful surprise: the fault count barely changes. `LockFreePool`'s constructor builds its free list by pushing every slot, which writes each slot's hook and therefore touches every page of the pool anyway, so all its faults are taken in the constructor whether or not `prefault` is set. The `memset` is redundant for this pool. It matters for structures that do *not* touch their memory at construction: an arena, a ring buffer allocated with `new T[n]` for a trivial `T`, or a pool whose free list is built lazily. With a copy of `Arena` whose constructor skips its pre-faulting pass, its first trip through the buffer takes one fault per 4 KiB page on the hot path, which `perf stat -e page-faults` shows directly and the first pass's p99.99 shows as microsecond outliers.

### Chapter 12: Safe Memory Reclamation

**1. The validation step.** The reader loads `p = src` (node A). Before the reader stores A in its slot, the writer exchanges `src` to B, retires A, and scans: the reader's slot is still empty, so the scan frees A. The reader now stores A in its slot, returns it, and dereferences freed memory. The writer's unlink, retire, scan and free must all happen between the reader's load of `src` and its store to the slot; the re-check closes exactly that window, because after the store the reader reloads `src`, sees B, and retries.

**2. Why `seq_cst`?** Reader: store `slot = A`, then load `src`. Writer: store `src = B` (the unlink), then load `slot` (the scan). The two stores and two loads form the store-buffering test. The bad outcome is that the reader's load returns A (the writer's store is not yet visible) *and* the writer's load returns null (the reader's store is not yet visible). The reader then believes A is protected, and the writer frees it. With release stores and no fences, both stores can sit in store buffers, which Chapter 3 observed in about 7.5% of trials. The fix needs both sides. The writer puts a `seq_cst` fence between the unlink and the scan. The reader makes *both* its announcement and its re-read `seq_cst` (or puts a `seq_cst` fence between them). Making only the store `seq_cst` and leaving the re-read as acquire happens to work on x86, where the store is a full barrier, but not on ARMv8.3 or POWER: there the acquire load may be satisfied before the store is visible, and the bad outcome comes back.

**3. The memory bound.** There are 16 × 2 = 32 hazard slots, and the threshold is 64. Just after a scan, a thread's retire list contains only nodes that were protected during the scan, so at most **32** (if every slot in the program protected one of this thread's nodes). Each thread's list never reaches 64 without a scan, so the whole program holds fewer than **16 × 64 = 1,024** retired but unfreed nodes (plus any orphans from exited threads), whatever the readers do.

**4. The stalled reader.** In 500 ms, the other threads retire 1 million nodes, or **64 MB** of nodes, plus the limbo lists' own bookkeeping (16 bytes per entry in our implementation, another 16 MB). None of it can be freed until the reader resumes and leaves its guard. With hazard pointers, the stalled reader holds at most its two hazard slots' worth of nodes, and everything else is freed by normal scans.

**5. Nesting.** If every guard announced, an inner guard would overwrite the thread's announcement with the current, possibly newer, epoch, while the outer region still holds pointers loaded in the older one. The epoch could then advance past the outer region's epoch, and nodes it is still using could be freed. If an inner guard's destructor stored `kInactive`, the rest of the outer region would run unprotected, with the same result. Counting nesting makes only the outermost guard announce and retract.

**6. Hands-on: Exercise 12.1, a hot-swappable configuration.**

```cpp
template <class F>
decltype(auto) read(F&& f) const {
    llds::HazardDomain::Guard hp;
    const T* p = hp.protect(current_);
    return std::forward<F>(f)(*p);
}
void update(T next) {
    std::lock_guard g(writer_);
    swap_in(std::move(next));
}
template <class F>
void modify(F&& f) {
    std::lock_guard g(writer_);
    T next = *current_.load(std::memory_order_acquire);  // safe: only writer_ holders retire
    f(next);
    swap_in(std::move(next));
}
private:
void swap_in(T next) {
    T* old = current_.exchange(new T(std::move(next)), std::memory_order_acq_rel);
    llds::HazardDomain::global().retire(old);
}
```

Readers are lock-free and never block on writers; writers are serialised by a mutex, which is fine because they are rare, and which also fixes the lost-update race in `modify`. Under ASan, the starter's `delete old` produces a heap-use-after-free report within the first few thousand updates; the solution produces none.

**7. Hands-on: measure the fence.** The "unsafe" scheme (acquire load, leaked objects) costs about as much as a plain load plus the two field reads, a few nanoseconds, so the gap between it and the epoch scheme (33 to 38 ns) is almost entirely the `seq_cst` fence. With `membarrier`, the reader's guard becomes a relaxed store of the epoch followed by `std::atomic_signal_fence(std::memory_order_seq_cst)` (a compiler barrier only), and the thread that tries to advance the epoch calls `syscall(SYS_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0)` before reading the announcements (the process must register once at start-up with `MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED`). Reader cost then falls to within a few nanoseconds of the unsafe scheme. Because `collect` runs every 64 retirements, the system call is amortised; make sure it never runs on a latency-critical thread.

### Chapter 13: Concurrent Hash Maps

**1. Mutex collapse.** 10.8 M ops/s on four threads is about 93 ns of wall time per operation, against 25 ns on one thread. On one thread, the mutex's line stays in L1, and the operation costs a lookup plus two uncontended atomics. With four threads, each acquisition usually finds the line in another core's cache, and the acquire and release each need a cache-line transfer of 50 to 100 ns (Chapter 2); under contention, waiters also sleep in the kernel and must be woken. The critical section is serialised in both cases, but with four threads every pass through it pays for moving the lock between cores.

**2. Tombstones.** Let keys a, b and c all hash to slot i. Insert a (slot i), b (slot i+1) and c (slot i+2). If erasing b stored `kEmptyKey` into slot i+1, a later `find(c)` would probe slot i (a), then slot i+1 (empty), conclude that the probe chain ends there, and report c absent, although it is in slot i+2. Leaving the key and marking the value as a tombstone keeps the chain intact.

**3. Linearization points.** B can return: (a) **absent**, because its key load ran before A's key CAS (B is linearized before A's insert); (b) **absent**, because its key load saw 7 but its value load ran before A's value store, which is the insert's linearization point, so B is still before A; (c) **70**, because its value load ran after A's value store (B after A). B can never return any other value, because the value word is written once, atomically, and read atomically.

**4. Capacity.** Keys are never removed from cells during the day, so what matters is the number of distinct keys inserted, not the number present at once. If only the day's active instruments are inserted (at most 30,000), a load factor of 0.5 requires 60,000 cells: round up to **65,536** (1 MiB at 16 bytes per cell). If the whole universe of 120,000 might be inserted, for example when loading reference data, use 262,144. Either way, check the count at start-up and refuse to run if the expected key count exceeds half the capacity.

**5. RCU write cost.** On one thread with 0.01% writes, 2 million operations took 2 × 10⁶ / 13.8 × 10⁶ ≈ 145 ms, against 28 ms with no writes, so the 200 writes cost about 117 ms, or **about 0.6 ms per write**. The four-thread figures give the same: 800 writes add 442 ms. Writes are serialised, so the map can absorb at most about 1,800 writes per second in total. With four threads, the RCU map matches the striped map's 65 M ops/s when the time spent on writes equals the difference in read time: f × 0.55 ms = 1/65 M − 1/274 M per operation, so f ≈ 2 × 10^−5^, which is about **1,400 writes per second** at that throughput. Above that, use the striped (or lock-free) map.

**6. Hands-on: Exercise 13.1, a sharded counter.**

```cpp
void add(std::uint64_t n = 1) noexcept {
    // Several threads may map to the same shard, so this must be an RMW.
    shards_[my_shard()].value.fetch_add(n, std::memory_order_relaxed);
}
std::uint64_t read() const noexcept {
    std::uint64_t sum = 0;
    for (const auto& s : shards_) sum += s.value.load(std::memory_order_relaxed);
    return sum;
}
static std::size_t my_shard() noexcept {
    static std::atomic<std::size_t> next{0};
    thread_local const std::size_t id = next.fetch_add(1, std::memory_order_relaxed) & (Shards - 1);
    return id;
}
struct alignas(llds::kFalseSharingRange) Shard { std::atomic<std::uint64_t> value{0}; };
std::array<Shard, Shards> shards_{};
```

`add()` keeps the atomic RMW because more threads than shards can exist, and shard IDs wrap: two threads may share a shard. If the design guaranteed one thread per shard, a relaxed load and store would suffice, as in Exercise 2.1. In `ch06_atomics`, the sharded counter's cost per increment stays flat as threads are added, while the single counter's rises. Note that `read()` is not a snapshot: it sums shards at slightly different times, which is fine for statistics.

**7. Hands-on: a value pool.** Allocate an array of N records, each a `SeqLock<Record>`, and a `ConcurrentIntMap` from key to index. Readers look up the index (wait-free) and `load()` the seqlock (a copy with no shared writes). Writers update the record in place through the seqlock, which requires a single writer per record, or a per-record lock around the store. At 0.01% writes, expect this design to scale like the lock-free map, far ahead of `StripedHashMap<uint64_t, Record>`; at 5% writes, it still wins, but readers retry more often on hot keys.

### Chapter 14: Ordered Structures

**1. Expected levels.** Each node reaches level 3 or above with probability 1/8, so about **12,500** of 100,000 nodes do. The expected height of the tallest node is about log₂ n ≈ 16.6. Our `random_level` caps levels at 15 (0 to 15, so 16 levels), which leaves about n/2¹⁵ ≈ 3 nodes at the top level for n = 100,000: no measurable cost. For much larger n, say 10⁸, the top level would hold about 3,000 nodes, and searches would walk a long list at the top; then `MaxLevel` should be about log₂ n.

**2. Why validate?** Suppose insert locked its predecessors and linked without checking. A concurrent erase of predecessor P marks P and unlinks it, setting P's own predecessor's `next` to P's successor. Our insert then sets `P->next = new`. P is no longer reachable from the head, so neither is the new node: the key is lost, although insert returned true. Similarly, if another insert placed a node between P and our successor S, writing `P->next = new` (with `new->next = S`) would cut that node out. Validation (P unmarked, S unmarked, `P->next == S`, all checked under P's lock) rules out both.

**3. Linearization points.** A successful insert: the store `fully_linked = true`. A successful erase: the store `marked = true`. A `contains` that returns false: if no node with the key was found, the moment the level-0 search passed the key's position; if a node was found but was unmarked-and-not-fully-linked or marked, the moment the flag was read. Insert waits for a found node to become fully linked because returning "already present" before the other insert's linearization point would let a later `contains` still return false, which no sequential order can explain.

**4. Range-scan guarantees.** Yes to both. If the quantity moves from a price the scan has not reached to one it has already passed, the scan misses it; if it moves from a price already passed to one not yet reached, the scan counts it twice. A snapshot fixes it, because both changes would appear in one published version or neither would. With RCU, the feed handler copies the affected part of the book (or the whole book, if small) after each update, or each batch of updates, and readers scan an immutable version; the cost is the copy and the reclamation of old versions.

**5. Tick sizes.** (105.000 − 95.000) / 0.005 = 2,000 ticks, so **2,001** levels, indices 0 to 2,000. Index = (price − 95.000) / 0.005. Do not compute it with `double` on the hot path: 0.005 has no exact binary representation, so a price such as 95.015 can convert to index 2.9999999 and truncate to 2. Exchanges send prices as integers with an implied number of decimals; keep them as integers (ticks, or fixed-point) from the decoder onwards, so the index is an integer subtraction.

**6. Hands-on: Exercise 14.1, a sequential skip list.** Everything rests on `find`:

```cpp
Node* find(const K& k, Node** preds) {
    Node* x = head_;
    for (int l = MaxLevel - 1; l >= 0; --l) {
        while (x->next[l] && x->next[l]->key < k) x = x->next[l];
        preds[l] = x;
    }
    return x->next[0];   // first node with key >= k, or nullptr
}

bool insert(const K& k) {
    Node* preds[MaxLevel];
    Node* n = find(k, preds);
    if (n && !(k < n->key)) return false;   // already present
    const int top = random_level();
    Node* node = new Node{k, top};
    for (int l = 0; l <= top; ++l) {
        node->next[l] = preds[l]->next[l];
        preds[l]->next[l] = node;
    }
    ++size_;
    return true;
}
```

`erase` unlinks the found node from `preds[l]` for each of its levels; `lower_bound` returns `find`'s result. Against `std::set`, expect the skip list to be somewhat slower for random lookups: it visits more nodes (about 2 log₂ n instead of log₂ n) and each node carries a 16-pointer array, so fewer nodes fit in the cache. Sequential inserts are fast in both.

**7. Hands-on: top N levels.** Keep a `struct Depth5 { std::int64_t bid_px[5], bid_qty[5], ask_px[5], ask_qty[5]; std::uint64_t seq; }` (168 bytes), fill it after each update by iterating the first five entries of each map, and publish it with `SeqLock<Depth5>`. The reader thread checks `bid_px[i] > bid_px[i+1]` and `ask_px[i] < ask_px[i+1]` for the populated levels. Because the seqlock copies the whole struct consistently, the check never fails; TSan stays silent because the seqlock uses atomic words. Expect `apply` to slow by the cost of walking ten map nodes and copying three cache lines per update, perhaps doubling it; publishing only when the top five actually change recovers most of it.

### Chapter 15: Data-Oriented Design and SIMD

**1. Bandwidth.** AoS: 64 bytes per 5.67 ns is **11.3 GB/s**. SoA: 4 bytes per 0.27 ns is **14.8 GB/s**. The vectorised compare-and-add could process eight values per cycle or so, about 0.06 ns per value, so the SoA loop is not limited by the core: it is limited by memory bandwidth (here, a mixture of L3 and DRAM bandwidth for a 16 MB array), just like the AoS loop, which reads 16 times as much.

**2. Padding.** `S` is laid out as `a` at 0, 7 bytes of padding, `b` at 8, `c` at 16, 3 bytes of padding, `d` at 20, `e` at 24 and 7 bytes of tail padding: `sizeof(S) == 32`, `alignof(S) == 8`. Reordered as `double b; int d; char a, c, e;` it is 8 + 4 + 3 = 15 bytes, padded to **16**, the minimum for these types with 8-byte alignment.

**3. Hot/cold.** Keep three parallel arrays indexed by instrument: an array of published top-of-book seqlocks, each padded to its own cache line so that readers of one instrument are never disturbed by writes to its neighbour; an array of 64-byte statistics blocks, aligned to 64 bytes and written only by the handler thread; and a cold array of static descriptions and symbols. Each update touches exactly two lines: the instrument's statistics line and its top-of-book line. The symbol and the description are never touched on the hot path.

**4. Ladder bounds.** Keep a second, pre-allocated ladder. When the best price comes within some margin of an edge (say, 1,000 ticks), choose a new base centred on the current price, copy the occupied range into the spare ladder (at most 128 KiB, about 10 µs), move levels that fall outside the new band into the overflow map, and swap. Do it when the feed is quiet if possible, or incrementally over several updates. A 3,000-tick move needs at most one or two recentrings a day. Readers of the published top of book are unaffected: they read the seqlock, not the ladder.

**5. Branch or not?** (a) **Branchy**: the branch is almost always taken the same way, so it is predicted correctly and costs almost nothing; the branchless version gains little (although the compiler may vectorise either). (b) **Branchless**: clamping is `std::min(std::max(x, -1.0f), 1.0f)`, which compiles to `maxss`/`minss` and vectorises, and audio code wants constant time anyway. (c) **Branchless**, or better, restructure: an unpredictable side choice should index an array of two sides (`books[side]`) rather than branch.

**6. Hands-on: separate the effects.** After sorting, the branch is taken for one contiguous third of the records and not taken for the rest, so it is predicted almost perfectly; the branchy loop speeds up dramatically, but still cannot vectorise because of the compiler barrier, so it stays slower than the branchless version. The difference between the sorted and unsorted branchy loops is the cost of mispredictions; the remaining difference to the branchless loop is vectorisation. Without the barrier, GCC typically if-converts the loop and vectorises it, producing nearly the same code as the branchless version.

**7. Hands-on: Exercise 15.1, particles.**

```cpp
void integrate(float dt) noexcept {
    const std::size_t n = size();
    float* __restrict px = x.data();
    float* __restrict py = y.data();
    float* __restrict pz = z.data();
    const float* __restrict qx = vx.data();
    const float* __restrict qy = vy.data();
    const float* __restrict qz = vz.data();
    for (std::size_t i = 0; i < n; ++i) {
        px[i] += qx[i] * dt;
        py[i] += qy[i] * dt;
        pz[i] += qz[i] * dt;
    }
}

double active_mass() const noexcept {
    double sum = 0.0;
    for (std::size_t i = 0; i < size(); ++i) {
        const float m = (flags[i] & 1u) ? mass[i] : 0.0f;   // a select, not a jump
        sum += m;
    }
    return sum;
}
```

The `__restrict` qualifiers tell the compiler the six arrays do not overlap, which it cannot otherwise prove, and which is needed to vectorise `integrate`. The `double` accumulation in `active_mass` is not vectorised at `-O2` without `-ffast-math`, because reordering floating-point additions changes the result; the select is still branch-free. For one million particles, expect SoA `integrate` to be several times faster than the AoS loop, which drags `mass` and `flags` through the cache as well.

### Chapter 16: Testing and Verifying Concurrent Code

**1. Why zero?** A bad read needs either the producer's slot store to become visible after its tail store (store-store reordering) or the consumer's slot load to be performed before its tail load (load-load reordering). x86-TSO forbids both in hardware. The compiler is allowed to perform either, because the code asked for no ordering, but GCC happened not to. So no component that actually ran reordered anything.

**2. TSan's blind spot.**

```cpp
std::atomic<int> data{0}, flag{0};
void producer() { data.store(42, std::memory_order_relaxed); flag.store(1, std::memory_order_relaxed); }
void consumer() { if (flag.load(std::memory_order_relaxed)) assert(data.load(std::memory_order_relaxed) == 42); }
```

All accesses are atomic, so there is no data race and TSan reports nothing. The assertion can fail on ARM, or after compiler reordering. A model checker (GenMC, CDSChecker, Relacy) finds the failing execution immediately.

**3. Stress-test design.** Use four producers and four consumers (more threads than cores, to perturb scheduling), a capacity of 4 or 8 to exercise full and empty constantly, and payloads of {producer ID, sequence number, checksum of both}. Each consumer verifies the checksum and sets a bit for (producer, sequence) in a shared bitmap with `fetch_or`, reporting a duplicate if the bit was already set; at the end, every bit must be set, or an item was lost. Per-producer FIFO order can be checked per consumer (a consumer must never see a producer's sequence numbers go backwards). Perturb with random `yield()`s in a test build, run under `taskset` on fewer cores, under `stress-ng` noise, and on ARM. For the blocking wait strategy, add phases in which producers stop for long periods and then burst, and check that every consumer wakes within a timeout (lost wake-ups show as timeouts) and that idle consumers use little CPU time.

**4. Pool and ASan.** ASan only tracks `malloc` and `new`. Pool slots are part of one large live allocation, so using a slot after returning it is, to ASan, an access to valid memory. Poison the slot's storage when it is returned and unpoison it when it is handed out:

```cpp
#include <sanitizer/asan_interface.h>   // the macros are no-ops without -fsanitize=address

T* create(...)  { Slot* s = pop_free_list(); ASAN_UNPOISON_MEMORY_REGION(s->storage, sizeof(T)); /* construct */ }
void destroy(T* p) { p->~T(); ASAN_POISON_MEMORY_REGION(storage_of(p), sizeof(T)); push_free_list(slot_of(p)); }
```

Poison only the storage, not the free-list link beside it, which the pool itself must still read. Poison every slot in the constructor too.

**5. Review.** For `SpscRing`: `tail_.load(relaxed)` in `try_emplace` ("only the producer writes `tail_`"); `head_.load(acquire)` on a cache miss ("pairs with the consumer's release in `pop_front`: the consumer has finished with slots before `head`, so we may overwrite them"); `tail_.store(release)` ("pairs with the consumer's acquire in `front`: publishes the element constructed in slot `t`"); `head_.load(relaxed)` in `front` ("only the consumer writes `head_`"); `tail_.load(acquire)` on a cache miss ("pairs with the producer's release: makes elements before `tail` visible"); `head_.store(release)` in `pop_front` ("pairs with the producer's acquire: the element's destruction happens before the producer reuses the slot"). Ownership: `tail_` and the head cache belong to the producer; `head_` and the tail cache to the consumer; slots belong to whichever side the indices say.

**6. Hands-on: Exercise 16.1, find the bugs.** Bug 1, the logic error: the full check is `t - head > cap_` instead of `== cap_`, so the producer writes one element past capacity, overwriting the oldest unread one. The single-threaded "capacity is respected" test catches it: the fifth push into a ring of four succeeds, and the first pop returns the wrong element. Bug 2, the ordering error: the tail is published with a relaxed store and read with a relaxed load. The stress test usually passes on x86, but the TSan build reports a data race between the slot write in `try_push` and the slot read in `try_pop`. The fixes:

```cpp
if (t - head_.load(std::memory_order_acquire) == cap_) return false;  // fixed: ==
tail_.store(t + 1, std::memory_order_release);                         // fixed: release
if (h == tail_.load(std::memory_order_acquire)) return false;          // fixed: acquire
```

**7. Hands-on: a model-checking test.** The test has two threads: T1 stores `slot = 1` and then loads `src`; T2 stores `src = 1` and then loads `slot`; after both finish, assert that not both loads returned 0. With `seq_cst` on all four operations, the tool explores every execution and reports none violating the assertion. Weakening T1 to a release store and acquire load (with T2 still `seq_cst`) produces a reported violation, as does weakening T2 instead, and the tool prints the execution in which both stores are invisible to both loads. Adding a `seq_cst` fence between the store and the load on each weakened side restores correctness, which is the structure of the hazard-pointer code.

### Chapter 17: Tuning the Platform

**1. Timer slack.** Linux adds a default timer slack of 50 µs to sleeps of ordinary threads, so a 50 µs sleep ends 50 to 100 µs later, and the thread must then be scheduled. The pipeline has two sleeping consumers in series, so an event can wait for both. With `prctl(PR_SET_TIMERSLACK, 1)`, each sleep ends close to 50 µs plus the wake-up latency, and the median should fall roughly by half, to some tens of microseconds. It remains far above the spinning configurations, because sleeping itself is the problem.

**2. Queueing.** During a 100 µs sleep, 50 events arrive, one every 2 µs. When the consumer drains the ring, the first has waited about 100 µs and the last about 0, so the latencies are spread evenly between 0 and 100 µs: median about 50 µs, p99 about 99 µs. Two sleeping stages in series, each with timer slack and wake-up latency, roughly double that, which is consistent with the measured median of 117 µs and p99 of 260 µs.

**3. Why pinning hurt.** Experiment 1: rerun D pinned to cores 1, 2 and 3 instead of 0, 1 and 2. If CPU 0's extra interrupts and kernel threads were the cause, the tail should improve. Experiment 2: record `perf sched record` during C and D and compare with `perf sched timehist`: in D, expect to see other tasks (kernel threads, other processes) running on the pinned CPUs, with the pinned thread waiting; in C, expect migrations of the spinning threads to the idle CPU instead. Diffing `/proc/interrupts` during each run shows more interrupts on CPU 0 than on the others.

**4. Core layout.** Put the critical path on socket 1, local to the network card: isolate, say, physical cores 17 to 20 of socket 1, and leave their SMT siblings idle. Feed handler on 17, strategy on 18, order sender on 19, sharing socket 1's L3. Put the logger on a non-isolated core of socket 1 (16, say), close to the rings it reads, and the monitoring agent on socket 0. Route the network card's interrupts to a housekeeping core on socket 1 (or, with kernel bypass and polling, disable them). All other interrupts and kernel housekeeping go to socket 0's cores and socket 1's housekeeping core.

**5. RT starvation.** A busy-spinning `SCHED_FIFO` thread at priority 99 never yields its core. Kernel threads bound to that core (per-CPU workqueues, `ksoftirqd`, RCU, migration) cannot run, and other cores that wait for them (for an RCU grace period, an IPI acknowledgement or a workqueue flush) stall too. RT throttling gives the core back for 5% of each second, which is why the hangs last seconds rather than forever. Fixes: run the spinning thread on an isolated core whose kernel housekeeping has been moved away (Chapter 17's isolation steps), or run it with an ordinary policy (`SCHED_OTHER`), since a thread alone on an isolated core gains nothing from real-time priority.

**6. Hands-on: Exercise 17.1, parse a CPU list.** The solution (`solutions/ch17/cpu_list.hpp`) trims a trailing newline, splits on commas, parses each item as a number or `lo-hi` range with explicit checks (non-empty, digits only, at most 4,095, `lo ≤ hi`), and sorts and de-duplicates the result. A start-up function then reads `/sys/devices/system/cpu/isolated`, parses it, and pins each named thread:

```cpp
void pin_onto_isolated(const std::vector<std::pair<std::string, std::thread*>>& threads) {
    std::ifstream f("/sys/devices/system/cpu/isolated");
    std::string line;
    std::getline(f, line);
    const auto cpus = parse_cpu_list(line);
    if (cpus.size() < threads.size())
        throw std::runtime_error("need " + std::to_string(threads.size()) + " isolated CPUs, have " +
                                 std::to_string(cpus.size()));
    for (std::size_t i = 0; i < threads.size(); ++i) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpus[i], &set);
        if (pthread_setaffinity_np(threads[i].second->native_handle(), sizeof set, &set) != 0)
            throw std::runtime_error("cannot pin " + threads[i].first);
    }
}
```

Failing loudly matters: a latency-critical service that silently runs unpinned because its configuration changed is a bug that appears only as a worse tail, weeks later.

**7. Hands-on: tune your machine.** Results depend on the machine, but on most bare-metal servers the biggest single improvement to p99.9 comes from isolation (`isolcpus` or cpusets plus `nohz_full` and IRQ affinity), followed by limiting C-states. Pinning alone, as the chapter showed, may make things worse; pinning after isolation is what makes configuration D's tail collapse to within a few microseconds of its median.

### Chapter 18: Case Studies

**1. Gating.** If the logger stops (blocked on a full disk, a stalled file system, or simply descheduled), the evaluator can publish 256 more alarm events, and then its `claim` waits for the logger: the alarm path is blocked by the log. At a few transitions per minute, that takes hours; in an alarm storm of ten transitions a second, 25 seconds. As a non-gating consumer, the logger instead compares the producer's cursor with its own position on every pass. If the producer is more than a ring's capacity ahead, the logger has been overrun: it must record the gap (the range of lost sequence numbers, the time and the count), raise a technical alarm ("audit log incomplete"), and skip to the oldest entry still in the ring. To read entries safely while they may be overwritten, each entry stores its sequence number, and the logger checks it after copying the entry, as a seqlock reader would.

**2. Sensor loss.** On every loop iteration (or a 10 ms tick, whichever is sooner), the evaluator compares the time of each channel's last sample with the current time and raises a technical alarm if it exceeds a timeout, for example a few sample periods for a "no data" condition, with a longer clinical timeout before escalation (the values are illustrative). The sensors must stamp each sample with a monotonic timestamp and a sequence number, so the evaluator can detect both silence and gaps, and should forward hardware status such as "lead off" or "probe disconnected" as samples of their own.

**3. One queue or two?** First, contention: both sensor threads would write the same tail line, so each push would pay for cache-line transfers that two SPSC rings avoid. Second, and more important for a safety system, fault isolation: in an MPSC queue of the Vyukov type, a producer stalled between claiming a position and publishing it blocks every later sample, so a stalled sensor thread would hide the other sensor's samples, and a babbling sensor could fill the shared queue and starve the other. With separate rings, each sensor's failure affects only its own channel. An MPSC queue is the right choice when there are many low-rate sources (dozens of channels, where polling dozens of rings costs more than occasional contention), or when the consumer needs the samples in one global arrival order.

**4. Trading budget.** The two largest items are network-to-user (1.2 µs) and order encode and send (1.1 µs). Attack the first with kernel bypass (receive directly from the card in user space with a polling thread) and the second with pre-built order templates in which only a few fields change, sent through the same bypass path, with the template's cache lines kept warm. Next is the strategy (300 ns), where data layout (Chapter 15) and keeping its working set in L1 and L2 help. Fusing the book and strategy threads removes one SPSC hand-off, about half of the ring's round trip (on the reference VM, roughly 240 ns), plus the cache misses the strategy takes when it reads book data written by another core.

**5. Audio reclamation.** `delete` may take an allocator lock, return memory to the kernel (a system call and TLB shootdowns), or run a destructor that does arbitrary work, all with unbounded duration: a glitch. The return path: the UI thread builds the new impulse response and exchanges it into a `pending` atomic pointer. If the exchange returns a non-null pointer, the callback never picked that one up, so the UI thread can delete it directly. The callback, at the top of each period, exchanges `pending` with null; if it gets a pointer, it swaps it into `current` and pushes the old `current` into an SPSC "trash" ring that the UI thread drains and deletes. Three swaps in one period therefore cost the callback at most one swap, and the UI thread deletes the two intermediate versions itself. If the trash ring is ever full, the callback keeps the old pointer in a small fixed array and retries next period; it never blocks.

**6. Hands-on: Exercise 18.1, a threshold alarm.**

```cpp
Change update(std::int32_t value, std::uint64_t t_ns) noexcept {
    const bool beyond = dir_ == Direction::Above ? value > trigger_ : value < trigger_;
    const bool recovered = dir_ == Direction::Above ? value < clear_ : value > clear_;
    if (!active_) {
        if (!beyond) { pending_ = false; return Change::None; }   // any good sample restarts the timer
        if (!pending_) { pending_ = true; since_ = t_ns; }
        if (t_ns - since_ >= persist_) { active_ = true; pending_ = false; return Change::Raised; }
        return Change::None;
    }
    if (recovered) { active_ = false; return Change::Cleared; }
    return Change::None;
}
```

For the boundary tests: a value exactly at the trigger is not beyond it (the comparisons are strict), so it does not start the timer; a value exactly at the clear threshold has not recovered, so the alarm stays active; and a persistence period that ends exactly on a sample raises the alarm on that sample, because the check is `>=`. Write the tests so that each of these decisions is explicit; in a real device, each would be a documented requirement.

**7. Hands-on: extend the monitor.** Add a third `SpscRing<Sample>`, a respiration sensor thread at 50 Hz (a 20 ms period), a `ThresholdAlarm` for low respiration rate with its own bit in `active_alarms`, and the new field in `VitalsView`; the evaluator's loop drains the third ring like the others. For the supervisor, give each stage a padded `std::atomic<std::uint64_t>` heartbeat that it increments on every loop iteration, and have a supervisor thread read all of them every 10 ms and report any that has not changed for 100 ms. Under TSan, the extended program should report nothing; if it reports a race, it is almost certainly a field added to a shared struct without going through the seqlock or a ring.

### Chapter 19: A Detector Acquisition Pipeline

**1. Ownership table.** Two rows change and one is added. Decoded hits are no longer owned by one thread: sorter 1 allocates a `HitBlock` from the pool and owns it while filling it; ownership passes to sorter 2 with the release in the SPSC ring's `try_push` of the pointer, and sorter 2 owns it from `try_pop` until it has processed the block. The new release event is the *return* of the block to the pool: sorter 2 must hand it back, either by deallocating into a pool that supports frees from another thread (the `LockFreePool` of Chapter 11) or by pushing the pointer onto a return ring back to sorter 1 (Chapter 11's return-ring design, which keeps the pool single-threaded). The table must also say what sorter 1 does when the pool is empty because sorter 2 is slow: wait (backpressure) or drop and count, and it must be the same answer as for the source rings, or the "one point of loss" rule is broken.

**2. Loss in two places.** The first three identities still hold, because they are about hits, and hits are still lost only at the sources. The fourth, coincidences sorted = coincidences analysed, fails; it must become sorted = analysed + dropped in the analysis ring, with a new counter. The design is harder to verify because the second loss point has no independent check: coincidences carry no sequence numbers, so the only evidence for the new counter is the counter itself. With one point of loss, every lost item is counted by the stage that dropped it *and* detected by a mechanism that does not trust that stage.

**3. Silent loss.** Examples: a FIFO overflow in the front-end electronics before the point where the source counts records; a DMA buffer overwritten by the device before the host read it; a bug in the source that skips a record while packing a batch. In each case the source's own counters say nothing was dropped. The next record from that module arrives with a sequence number more than one above the last, so the gap is counted downstream, and the identity "gaps = dropped + rejected" fails, pointing at a loss nobody accounted for.

**4. Wrap arithmetic.** After 0xfffffffd the expected number is 0xfffffffe. The gap is `0x00000002 - 0xfffffffe` in 32-bit unsigned arithmetic: 2 − (2^32 − 2) = 4 − 2^32, which is 4 modulo 2^32. Four records are missing: 0xfffffffe, 0xffffffff, 0x00000000 and 0x00000001.

**5. Hands-on: Exercise 19.1.** The reference solution is in `solutions/ch19/sequence_tracker.hpp`. Its core is one modular subtraction:

```cpp
const std::uint32_t ahead = seq - st.expect;
if (ahead >= 0x8000'0000u) { ++st.late; return 0; }   // behind: late or duplicate
st.missing += ahead;
st.expect = seq + 1;
```

Splitting the 2^32 range in half is the standard way to compare wrapping sequence numbers (it is how TCP compares them): anything less than half the range ahead is "ahead", anything else is "behind". It works as long as fewer than 2^31 records are lost in a row and no record is more than 2^31 late, both of which are true by many orders of magnitude here. Two design decisions are visible in the tests. The first record from a source *synchronises* the tracker rather than counting the numbers before it as lost, because a tracker can attach to a running stream; and `finish` is needed because loss at the very end of a stream leaves no later record to reveal the gap. To use it in the example, replace the `expect` array and the two places that update `seq_gaps` with `tracker.observe(h.module, h.seq)` and, after the join, `tracker.finish(m, final_seq[m])`.

**6. Hands-on: break the books.** Skipping a batch without decoding it breaks two identities: *sent = decoded + rejected* (the source counted the 64 hits as sent, the decoder never saw them) and *gaps = dropped + rejected* (the sequence numbers show 64 more missing hits than anyone dropped or rejected). With sequence checking also removed, the first identity still fails. That is the value of having two independent identities: the sorter's bug is caught by the count *and* by the sequence numbers, and disabling one check does not hide it.

### Chapter 20: Coincidence Sorting

**1. Policies by hand.** Number the hits 0 to 5. *Multiple windows, take all pairs:* (0, 1) at 3 apart, (0, 2) at 4, (1, 2) at 1, and (2, 3) at 5 (the window is inclusive); (4, 5) are on the same module. Four coincidences. *Single window, take all pairs:* hit 0 opens a window to time 5, which hits 1 and 2 join; hit 3 at time 9 is outside it. The group {0, 1, 2} is a multiple, giving (0, 1), (0, 2), (1, 2). Hit 3 opens the next window alone: a single. Hit 4 opens a window that hit 5 joins: two hits on one module, counted as same-module. Three coincidences, one multiple, one single. The pair (2, 3) is lost because hit 2 was already in a group. *Single window, kill multiples:* no coincidences at all; the same multiple, single and same-module counts.

**2. Boundary.** With `>=`, `first` moves past a hit that is exactly `window` older than hit *j*, so pairs at exactly the window distance are lost. The oracle-comparison test fails within the first few trials: with gaps of 0 to 6 ticks and windows of 0 to 19, exact-boundary pairs occur in almost every input. With realistic gaps of thousands of picoseconds, a difference exactly equal to the window would be rare, and the bug could survive thousands of tests. Choosing test data so that boundary cases are *common* is the point.

**3. Memory bound.** Hits in the 4 ns before a given hit, at 20 million a second, are Poisson with mean λ = 20×10⁶ × 4×10^−9^ = 0.08. The probability that more than 16 are retained is about e^−0.08 × 0.08^17 / 17!, roughly 10^−33: it does not happen. A 1,024-hit ring is generous, by design, for a different reason: retention depends on the *timestamps*, not on when hits arrive, so a stall that delays hits does not increase it. What can fill it is a fault that produces many hits with the same or nearly the same timestamp (a stuck clock, a noisy channel firing continuously). The capacity, and the `forced_evictions` counter, are there to make such a fault visible and bounded, not to absorb normal traffic.

**4. Watermark liveness.** *Heartbeat:* when a source has sent nothing for, say, 10 µs, it sends an empty batch whose promise is the earliest time it could still send (the minimum of its clock and anything in its reorder buffer). *Timeout:* if the merger has received neither data nor a heartbeat from a source for, say, 10 ms, it closes that source. During the timeout, the watermark is stuck at source 1's last promise, so the merger buffers source 0's hits; when that buffer is full, the sorter stops taking source 0's batches, its ring fills, and source 0 starts dropping, counted. Size the per-source buffer for the timeout (here 10 ms at source 0's rate) if that loss is unacceptable. The fault record names source 1, the time of its last promise and the time of exclusion, and states that its data after the last promise is missing; any later data from source 1 is rejected until an explicit re-admission.

**5. Hands-on: Exercise 20.1.** The reference solution is in `solutions/ch20/single_window.hpp`; its key line is the window test against the *opening* hit, `hits[end].tick - hits[i].tick <= window`, not against the previous hit, which would make the window slide. In the test, every group's hits lie within 5 ticks of one another and groups are 100 ticks apart. Then each group is exactly one single window, every pair in it is within the window, and no pair crosses groups, so take-all-pairs and the multiple-window sorter produce the same pairs in the same order. In general they differ: a chain of hits each within the window of the next but not of the first is one group under multiple windows' pairwise rule but several single windows, and pairs that straddle two single windows, like (2, 3) in Exercise 1, are lost.

**6. Hands-on: Exercise 20.2.** The reference solution, in `solutions/ch20/delayed_window.hpp`, keeps two indices `lo` and `hi` that bracket the hits between `delay` and `delay + window` after hit *i*; both only move forward, so the work is O(n + C). The window shape matters: it must be the same as the prompt sorter's, which pairs each hit with later hits at most `window` after it. A delayed window of [delay − window, delay + window] is twice as wide and counts twice as many randoms. Added to the example (recording all sent hits, as `--check` does) with a delay of 100 ns, it reports about 3.15 million delayed coincidences in a run with 8.56 million prompts before the energy window. A delay of 200 ns gives the same figure to within 0.05%, as it should if no true coincidences reach that far. As a cross-check, the prompts between modules one and two apart, which are almost all randoms in this simulation, are about 110,000 per module pair; randoms spread evenly over the 28 pairs of different modules, giving about 3.07 million, consistent with the delayed estimate.

### Chapter 21: Backpressure, Calibration and Parallel Analysis

**1. Size a buffer.** During a 5 ms pause, 8 million × 0.005 = 40,000 hits arrive and none are processed, so the buffer must hold 40,000 hits plus whatever was already queued, plus a margin. Afterwards the backlog drains at the net rate of 10 − 8 = 2 million hits a second, which takes 20 ms. Another pause within those 20 ms adds to the backlog, so the margin should be based on the measured distribution of pauses, not on one.

**2. Which policy?** (a) A live count-rate display wants fresh data: drop the oldest, or sample, and show a "data incomplete" indicator; losing display updates is harmless. (b) Quantitative list-mode data must not lose anything silently: apply backpressure up to the source, count every drop there so that dead-time correction can account for it, and declare a fault if loss persists. (c) An audit log of operator commands is low rate and must be complete: block (the rate makes this cheap), or if the log is stalled, record a gap and raise a technical alarm as Chapter 18's non-gating logger does. Never drop silently.

**3. Calibration race.** First, a batch can see a mixture: some modules' old gains and others' new ones, and within one module a new gain with an old offset, since gain and offset are separate atomics. Nothing in the design can prevent it or even detect it. Second, there is no version: results cannot say which calibration produced them, and a table cannot be validated as a whole before it goes live, so a half-written or invalid set of coefficients is used immediately. The immutable snapshot solves both by making the whole table the unit of change.

**4. Snapshot frequency.** On the reference machine, `ch12_reclamation` measured 133 ns per `atomic<shared_ptr>` load with one reader and 959 ns with three. 1% of a 100 ns per-coincidence budget is 1 ns per coincidence, so one snapshot per 133 coincidences with one reader, or per about 1,000 with three contending readers. The example's 1,024 is about at that limit for a contended case, and far inside it for one reader.

**5. Hands-on: Exercise 21.1.** The reference solution is in `solutions/ch21/pair_histogram.hpp`. The validation happens before any thread starts, because an exception thrown inside a `std::thread` terminates the program. Each private histogram is a `Padded<std::vector<std::uint64_t>>`; the padding keeps the vector *objects* on separate lines, and the bins themselves are separate heap allocations, so no two workers write the same line. Compared with one shared array of `std::atomic<std::uint64_t>` incremented with relaxed `fetch_add`, the private version scales with threads while the shared version gets slower as threads are added, because the opposite-module bins receive most of the counts and every increment fights over those few cache lines. Expect the shared version to stop scaling, and quite possibly to get slower, as threads are added; the size of the effect depends on the machine, so measure it.

**6. Hands-on: reproducibility.** With a fixed partition of the input and a fixed merge order, each configuration is deterministic: running serial twice, or 4-thread twice, gives identical bits. The configurations differ from each other in the last bits (and, with a range of six orders of magnitude, sometimes by more), because each groups the additions differently and floating-point addition is not associative. If the merge used the order in which threads finished, or every thread added into a shared `std::atomic<double>`, even repeated runs of one configuration would differ, which is the property a regulated product most needs to avoid.

### Chapter 22: Engineering for a Regulated Environment

**1. Bound a claim.** For example: "Every display frame shows a complete `VitalsView` (no field from a different update than another), and its newest sample is no older than 100 ms when the frame is drawn, while the acquisition rate is at its specified value." Mechanism: the `SeqLock` publication and the generation counter. Verification: the example's display-regression count (must be zero) and a histogram of frame staleness (`now − t_ns` at draw time) whose maximum must be below 100 ms over a specified run length. The boundary is explicit: torn frames are excluded entirely, staleness is bounded under stated conditions.

**2. Extend the table.** *DAQ-008, requirement:* on abort, every pipeline thread stops within 100 ms, and the abort report gives, per stage, the number of accepted items discarded, such that accepted = processed + discarded. *Design:* an abort flag checked in every wait and spin loop, closeable queues, a discard count per buffer taken when the flag is observed, join of every thread before destruction. *Verification:* a test that aborts at random points of 1,000 runs under ThreadSanitizer, checks the time to stop against the limit and checks the accounting identity in every run.

**3. First fault.** Source 1's promise stops advancing, so the watermark stops; the merger keeps buffering source 0's hits until its per-source buffer is full (16,384 hits, about 20 ms at source 0's rate); the sorter then stops taking source 0's batches; source 0's ring fills (256 batches, about another 20 ms); source 0 starts dropping, and `batches_dropped` and `hits_dropped` rise; downstream, sequence gaps appear for source 0's modules. When source 1 resumes, the merge restarts and the queue delay shows a long tail. The fault report should name source 1's stall, with its start time, as the cause; the drops at source 0 are a consequence. A report that names only the drops sends the investigation to the wrong component.

**4. Evidence and its limits.** DAQ-003: the tests show that the implementations agree with the oracle on random inputs of limited size and density and on every chunking of small inputs; they do not show that the pairing rule is the one the physics requires, and they do not exercise production-scale inputs. DAQ-007: ThreadSanitizer shows that no race occurred in the executions it observed; it does not cover interleavings that did not happen, and it does not model `std::atomic_thread_fence` (Chapter 16), which matters for the seqlock but not for these components.

**5. Rehearse.** There is no single answer. Common findings: claims of "lock-free" for code that waits (Chapter F7); "no loss" without saying where it is measured (this chapter); latency figures without the machine, percentile and run length (Chapter 4); and a limitation stated defensively instead of as the next experiment. Fix each by naming the evidence, or by changing the claim to what the evidence supports.
