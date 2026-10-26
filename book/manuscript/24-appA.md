# Appendices {.part}

## Appendix A. Memory Ordering on x86-64 and ARMv8 {#appA}

This appendix summarises how C++ atomic operations map to machine instructions on the two architectures that matter most for low-latency work, and what each hardware memory model allows. It is a reference for reading generated code, not a substitute for the C++ rules of Chapter 3: correct C++ is correct on every architecture, and the mappings below are what compilers emit for it. The mappings are those used by GCC and Clang; other compilers may differ in detail.

### Hardware memory models in one table

| Reordering of | x86-64 (TSO) | ARMv8 (weak, multi-copy atomic) |
|---|---|---|
| Load after load | Never | Allowed |
| Store after store | Never | Allowed |
| Load after store (to a different address) | **Allowed** (store buffer) | Allowed |
| Store after load | Never | Allowed |
| Dependent loads (address depends on an earlier load) | Never | Never |
| Atomic RMW reordered with anything | Never (RMWs are full barriers) | Depends on the variant |
| Stores visible to all other cores at the same time | Yes | Yes (ARMv8 is "other-multi-copy atomic") |

x86-64's model, **total store order** (TSO), allows only one reordering: a store followed by a load of a different location may appear reordered, because the store waits in the store buffer. This is why the store-buffering litmus test of Chapter 3 fails without `seq_cst`, and why release/acquire cost nothing on x86. ARMv8 allows every reordering except those prevented by dependencies or by explicit barriers or ordered instructions, which is why missing release/acquire orders produce wrong results there and not on x86 (Chapter 16).

### x86-64 mapping

| C++ operation | x86-64 instruction(s) |
|---|---|
| `load(relaxed)`, `load(acquire)`, `load(seq_cst)` | `mov` |
| `store(relaxed)`, `store(release)` | `mov` |
| `store(seq_cst)` | `xchg` (or `mov` + `mfence`) |
| `exchange` (any order) | `xchg` (implicitly locked) |
| `fetch_add`, `fetch_sub` (any order) | `lock xadd` (or `lock add` if the result is unused) |
| `fetch_and`, `fetch_or`, `fetch_xor` (any order) | `lock and`/`or`/`xor` if the result is unused; otherwise a `lock cmpxchg` loop |
| `compare_exchange_weak`/`strong` (any order) | `lock cmpxchg` |
| 16-byte `compare_exchange` | `lock cmpxchg16b` (requires `-mcx16`; may go through libatomic) |
| `atomic_thread_fence(acquire)`, `(release)`, `(acq_rel)` | nothing (compiler barrier only) |
| `atomic_thread_fence(seq_cst)` | `mfence` (or a locked no-op such as `lock or [rsp], 0`) |
| `atomic_signal_fence(any)` | nothing (compiler barrier only) |

Every locked instruction is a full barrier on x86, so the memory order passed to an RMW makes no difference to the generated code; it still matters to the compiler, which may reorder surrounding code according to it. On the reference machine, a `seq_cst` store cost about 7 ns against 0.4 ns for a release store (Chapter 6), which is the cost of draining the store buffer.

### ARMv8 mapping

| C++ operation | ARMv8.0 | ARMv8.1+ (LSE atomics) |
|---|---|---|
| `load(relaxed)` | `ldr` | `ldr` |
| `load(acquire)` | `ldar` (or `ldapr` with RCpc, ARMv8.3) | same |
| `load(seq_cst)` | `ldar` | `ldar` |
| `store(relaxed)` | `str` | `str` |
| `store(release)`, `store(seq_cst)` | `stlr` | `stlr` |
| `fetch_add` (relaxed / acquire / release / acq_rel) | `ldxr`/`stxr` loop (with `ldaxr`/`stlxr` as needed) | `ldadd` / `ldadda` / `ldaddl` / `ldaddal` |
| `exchange` | `ldxr`/`stxr` loop | `swp` / `swpa` / `swpl` / `swpal` |
| `compare_exchange` | `ldxr`/`stxr` loop | `cas` / `casa` / `casl` / `casal` |
| `atomic_thread_fence(acquire)` | `dmb ishld` | same |
| `atomic_thread_fence(release)`, `(acq_rel)`, `(seq_cst)` | `dmb ish` | same |

Three ARM details matter in practice.

**`seq_cst` loads and stores do not need fences.** ARMv8 defines `ldar` and `stlr` so that a `stlr` followed by an `ldar` are never reordered, which is exactly what `seq_cst` requires; so on ARMv8, unlike ARMv7, `seq_cst` loads and stores cost the same as acquire and release.

**Load-linked/store-conditional loops can fail spuriously.** On ARMv8.0, every RMW is a loop around a load-exclusive and a store-exclusive that fails if the line was touched in between, including by unrelated events. This is why `compare_exchange_weak` exists and why it is the right choice inside a loop (Chapter 6). Under heavy contention, these loops scale poorly; the LSE instructions of ARMv8.1 (`ldadd`, `cas`, `swp`) are performed in the cache or interconnect and are far better. Build with `-march=armv8.1-a` or later (or `-moutline-atomics`, the default on recent GCC for AArch64 Linux, which selects LSE at run time) to get them.

**Dependencies give ordering for free.** If a load's address depends on the value of an earlier load (as in following a pointer just loaded), ARM will not reorder them. C++ tried to expose this as `memory_order_consume`, but no compiler implements it as intended, and all promote it to `acquire`. Use `acquire`.

### Cache-line and false-sharing sizes

| Platform | Cache line | Recommended padding (`kFalseSharingRange`) |
|---|---|---|
| Intel x86-64 | 64 B | 128 B (the adjacent-line prefetcher fetches pairs of lines) |
| AMD x86-64 (Zen) | 64 B | 64 to 128 B (128 B is safe) |
| Apple M-series | 128 B | 128 B |
| ARM Neoverse (Graviton, Ampere) | 64 B | 64 to 128 B (128 B is safe) |
| IBM POWER | 128 B | 128 B |

`std::hardware_destructive_interference_size` reports a value for the target the compiler was told about, but it is a compile-time constant that can be wrong for the machine the code actually runs on, and GCC warns when it is used in a header. The `llds` library uses a fixed 128 bytes.

### Quick reference: which order do I need?

| Pattern | Writer | Reader | Chapter |
|---|---|---|---|
| Counter, statistic | `relaxed` RMW (or single-writer `relaxed` store) | `relaxed` | 6, 13 |
| Publish data through a flag or index | `release` store | `acquire` load | 3, 8 |
| Seqlock | `relaxed` counter, `release` fence, `relaxed` data, `release` counter | `acquire` counter, `relaxed` data, `acquire` fence, `relaxed` counter | 7 |
| Lock acquire / release | `acquire` RMW / `release` store | | 5 |
| Store then load of a different variable (Dekker, hazard pointers, wait/notify flags) | `seq_cst` on both sides, or `seq_cst` fences | | 3, 12 |
| CAS loop | `compare_exchange_weak` with the success order the pattern needs, `relaxed` on failure | | 6 |
