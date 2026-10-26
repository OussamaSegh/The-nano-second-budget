## Appendix C. Glossary {#appC}

**ABA problem.** A CAS succeeds because a location holds the value it expected, although the location was changed and changed back in between, so the state the CAS assumed no longer holds. Fixed with tagged pointers or safe memory reclamation (Chapters 6 and 12).

**Acquire.** A memory order for loads (and RMWs): no later memory operation of the same thread may be reordered before it. Paired with a release store, it creates a happens-before edge (Chapter 3).

**Alarm fatigue.** Desensitisation of clinical staff to alarms caused by frequent non-actionable alarms; reduced by persistence and hysteresis in alarm logic (Chapter 18).

**Arena (bump allocator).** An allocator that hands out memory by advancing a pointer through a block and frees everything at once (Chapter 11).

**Array of structs (AoS) / struct of arrays (SoA).** Two layouts for a collection of records: each record's fields together, or each field's values together (Chapter 15).

**Asymmetric fence.** A technique in which the frequent side of a synchronisation pattern uses only a compiler barrier and the rare side forces a barrier on all cores, for example with Linux's `membarrier()` (Chapter 12).

**Atomic operation.** An operation on a `std::atomic` object that is indivisible and participates in the memory model; concurrent atomic accesses are never data races (Chapter 3).

**Backoff.** Waiting for an increasing time after a failed attempt to acquire a contended resource, to reduce contention (Chapter 5).

**Backpressure.** A slow consumer causing upstream stages to wait rather than drop, so that loss, if any, happens at one stage that is designed for it (Chapters 19 and 21).

**Busy-spin.** Waiting by repeatedly checking a condition, without giving up the CPU (Chapters 9 and 17).

**C-state / P-state.** Processor idle states (deeper states save more power and take longer to exit) and performance states (clock frequency and voltage) (Chapter 17).

**Cache coherence.** The hardware protocol (such as MESI) that keeps the copies of a memory location in different caches consistent (Chapter 2).

**Cache line.** The unit of data transfer between memory and caches, and between caches: 64 bytes on most x86 and ARM processors, 128 bytes on Apple M-series (Chapter 2).

**CAS (compare-and-swap).** An atomic RMW that writes a new value only if the location holds an expected value; `compare_exchange_weak` and `compare_exchange_strong` in C++ (Chapter 6).

**Coincidence (PET).** Two hits on different detector modules within the coincidence window, taken to come from one positron annihilation. *Prompt* coincidences are trues, scatters and randoms (Chapter 20).

**Coincidence window.** The maximum time difference, typically a few nanoseconds, between two hits that may form a coincidence (Chapter 20).

**Condition variable.** A primitive that lets a thread sleep, with a mutex released, until another thread notifies it that a condition may have changed; always used with a predicate (Chapter F6).

**Coordinated omission.** A benchmarking error in which a closed-loop load generator stops sending requests while the system is stalled, so the stall is under-represented in the measured latencies (Chapter 4).

**Critical section.** Code that must not run concurrently with certain other code, usually protected by a lock (Chapter 5); also, in epoch-based reclamation, the region in which a reader may hold pointers (Chapter 12).

**Data race.** Two conflicting accesses to the same memory location from different threads, at least one a write and at least one non-atomic, not ordered by happens-before. Undefined behaviour in C++ (Chapter 3).

**Dead time.** Loss of counts at high rates because a detector or acquisition chain is busy; corrected for in quantitative imaging, which is why the software must report its own losses (Chapter 21).

**Delayed window.** A randoms estimate that pairs hits with hits arriving a fixed long delay later, where no true coincidence can occur (Chapter 20).

**Disruptor.** A ring-buffer design from LMAX in which entries stay in place and multiple consumers track their progress with sequence counters (Chapter 10).

**DMA (direct memory access).** A device writing into or reading from host memory without the CPU; governed by driver and platform rules, not by C++ atomics (Chapter 19).

**Drain.** Stop accepting new work but finish the work already accepted; contrasted with abort (Chapters F6 and 22).

**Energy window.** The range of calibrated photon energies, around 511 keV in PET, accepted for a coincidence; rejects many scattered photons (Chapter 21).

**Epoch-based reclamation (EBR).** A safe memory reclamation scheme in which readers announce a global epoch and retired nodes are freed after the epoch has advanced twice (Chapter 12).

**False sharing.** Performance loss caused by threads writing different variables that happen to share a cache line (Chapter 2).

**Fence.** An `std::atomic_thread_fence`, which orders memory operations around it without itself accessing memory (Chapter 3).

**Futex.** A Linux system call for waiting on and waking threads by memory address; the basis of `std::mutex` and `std::atomic::wait` on Linux (Chapters 5 and 9).

**Happens-before.** The partial order on operations, built from program order and synchronises-with edges, that determines which writes a read may observe (Chapter 3).

**Hazard pointer.** A per-thread published pointer that prevents the node it points to from being freed; the basis of a safe memory reclamation scheme with bounded memory (Chapter 12).

**Hit.** This book's name for one digitised photon detection: a timestamp, module, energy and sequence number. Called a *single* in PET terminology (Chapter 19).

**Hysteresis.** Using different thresholds for entering and leaving a state, so that a value near a threshold does not cause rapid switching (Chapter 18).

**Isolated core.** A CPU core from which the scheduler, interrupts and kernel housekeeping have been removed, so that it runs only explicitly placed threads (Chapter 17).

**Jitter.** Variation in latency, especially the occasional large delays caused by the platform (Chapter 1).

**Linearizability.** A correctness condition for concurrent objects: each operation appears to take effect at a single instant (its linearization point) between its invocation and its response (Chapters 6 and 14).

**List mode.** Storage of coincidences as a list of individual events rather than as histograms (Chapter 19).

**Little's law.** In a stable system, the average number of items in the system equals the arrival rate times the average time each spends in it: L = λW (Chapter 1).

**Lock striping.** Splitting a data structure into independent parts, each with its own lock, selected by hash (Chapter 13).

**Lock-free.** A progress guarantee: some thread always completes its operation in a finite number of steps, whatever the scheduling of the others (Chapter 6).

**MCS lock.** A queue lock in which each waiter spins on its own node, so a release wakes exactly one waiter with one cache-line transfer (Chapter 5).

**Memory order.** The parameter of an atomic operation (`relaxed`, `acquire`, `release`, `acq_rel`, `seq_cst`) that specifies what ordering it imposes (Chapter 3).

**MESI.** A cache-coherence protocol whose states are Modified, Exclusive, Shared and Invalid (Chapter 2).

**Model checker.** A tool that explores all executions of a small concurrent program permitted by a memory model, to find assertion failures and races (Chapter 16).

**MPMC / MPSC / SPMC / SPSC.** Multi- or single-producer, multi- or single-consumer: the classes of queues by how many threads may push and pop concurrently (Chapters 8 and 9).

**Multiple (coincidence).** Three or more hits within one coincidence window; handled by a policy such as take all pairs, kill multiples or take the winner (Chapter 20).

**NUMA.** Non-uniform memory access: on multi-socket machines, memory attached to the local socket is faster to reach than memory attached to another (Chapter 17).

**Oracle.** A slow, obviously correct implementation of a specification, used to check a fast one on many inputs (Chapters F4 and 20).

**Order book.** The set of outstanding buy and sell orders for an instrument, ordered by price (Chapter 14).

**Ownership.** Responsibility for an object's lifetime and, at a hand-off, the right to modify or reuse its storage (Chapters F2 and 19).

**Percentile (p99, p99.9).** The value below which a given percentage of samples fall; the standard way to describe latency tails (Chapter 4).

**PET (positron emission tomography).** A medical imaging method that reconstructs a tracer's distribution from coincidences of the two 511 keV photons produced by each positron annihilation (Part VI).

**Pool.** An allocator that pre-allocates fixed-size slots and recycles them (Chapter 11).

**PREEMPT_RT.** The Linux real-time patch set, merged in Linux 6.12, that makes almost all kernel code preemptible for bounded scheduling latency (Chapter 17).

**Priority inversion.** A high-priority thread waits for a lock held by a low-priority thread, which is itself preempted by medium-priority work (Chapter 5).

**RAII.** Resource acquisition is initialisation: tying a resource's release to an object's destructor, so leaving a scope by any path releases it (Chapter F2).

**Random coincidence.** A coincidence between photons from two different annihilations; its rate is about 2τ·S₁·S₂ for a window of width 2τ and singles rates S₁ and S₂ (Chapter 20).

**RCU (read-copy-update).** A pattern in which readers access data without locks, writers publish modified copies, and old versions are freed after a grace period (Chapters 12 and 13).

**Relaxed.** The weakest memory order: atomicity and a single modification order per variable, but no ordering of other memory operations (Chapter 3).

**Release.** A memory order for stores (and RMWs): no earlier memory operation of the same thread may be reordered after it (Chapter 3).

**Retire.** To hand a removed node to a reclamation scheme, which frees it when no reader can still reach it (Chapter 12).

**RMW (read-modify-write).** An atomic operation that reads and writes a location indivisibly: `fetch_add`, `exchange`, CAS (Chapter 6).

**Safe memory reclamation (SMR).** The family of schemes that decide when a node removed from a lock-free structure can be freed (Chapter 12).

**Scattered coincidence.** A coincidence from one annihilation in which a photon was deflected, so the line between the detectors misses the true origin (Chapter 20).

**Seqlock.** A single-writer synchronisation scheme in which readers copy the data and retry if a sequence counter shows that a write overlapped the copy (Chapter 7).

**Sequence number.** A per-source counter carried in each record, so that a receiver can detect loss, duplication and reordering independently of the sender's own counters (Chapter 19).

**Sequential consistency (`seq_cst`).** The memory order under which all `seq_cst` operations appear in a single total order consistent with each thread's program order (Chapter 3).

**Single-writer principle.** Designing so that each piece of mutable data is written by exactly one thread (Chapters 2, 10 and 14).

**Sinogram.** A histogram of coincidence counts per line of response, the classic input to PET reconstruction (Chapter 21).

**Skip list.** A probabilistic ordered structure made of sorted linked lists at several levels (Chapter 14).

**SMT (hyper-threading).** Running two or more hardware threads on one physical core, sharing its execution resources and private caches (Chapters 2 and 17).

**Store buffer.** A per-core queue of stores that have executed but are not yet visible to other cores; the cause of store-load reordering on x86 (Chapter 3).

**Tagged pointer.** A pointer combined with a version counter, so that a CAS fails if the pointer was changed and changed back (Chapter 6).

**Tail latency.** The high percentiles of a latency distribution (Chapter 1).

**Time-of-flight (TOF).** Measuring the arrival-time difference within a coincidence precisely enough, a few hundred picoseconds, to locate the annihilation along the line (Chapter 20).

**TLB (translation lookaside buffer).** A cache of virtual-to-physical address translations; a miss costs a page-table walk (Chapter 2).

**Treiber stack.** A lock-free stack using CAS on the head pointer (Chapter 6).

**True coincidence.** A coincidence of the two photons of a single annihilation, neither scattered (Chapter 20).

**TSC (time-stamp counter).** A per-core cycle counter read with `rdtsc`, the cheapest high-resolution clock on x86 (Chapter 4).

**TSO (total store order).** The x86 memory model, which allows only store-load reordering (Chapter 3, Appendix A).

**Wait-free.** A progress guarantee: every thread completes its operation in a bounded number of its own steps (Chapter 6).

**Watermark.** A time below which a merger knows no further input can arrive, computed from per-source promises; a silent source stalls it (Chapter 20).

**Work stealing.** A scheduling scheme in which idle workers take tasks from the deques of busy workers (Chapter 18).
