# The Nanosecond Budget

**Fast, Correct Concurrent Data Structures in Modern C++**, *from cache lines to real-time medical imaging.*

[![Code](https://github.com/OussamaSegh/The-nano-second-budget/actions/workflows/code.yml/badge.svg)](https://github.com/OussamaSegh/The-nano-second-budget/actions/workflows/code.yml)
[![Book](https://github.com/OussamaSegh/The-nano-second-budget/actions/workflows/book.yml/badge.svg)](https://github.com/OussamaSegh/The-nano-second-budget/actions/workflows/book.yml)

**Read online, download the PDF or EPUB:** https://oussamasegh.github.io/The-nano-second-budget/

This repository holds the book and its companion code:

- the **`llds`** library, examples, exercises and solutions, at the top level (everything below is about them);
- the **book**: the manuscript in [`book/manuscript`](book/manuscript), the build that turns it into the print interior, cover, EPUB and PDF in [`book/build-book`](book/build-book), and the website in [`book/site`](book/site). See [`book/README.md`](book/README.md).

Every push to `main` rebuilds and tests the code with GCC, Clang, ThreadSanitizer and AddressSanitizer, rebuilds the book and republishes the website. Errata and questions are welcome as [issues](https://github.com/OussamaSegh/The-nano-second-budget/issues).

## The companion code: llds

`llds` is the hands-on project for the book. It contains:

- **`include/llds/`**: a small header-only library with every data structure the book builds;
- **`examples/`**: one or more runnable programs per chapter, which produce the measurements quoted in the text;
- **`exercises/`**: starter code for the 25 hands-on exercises, 4 in `fNN/` for Part 0 (Foundations) and 21 in `chNN/` (each fails its test until you complete it);
- **`solutions/`**: reference solutions to the same exercises, discussed in Appendix D of the book;
- **`tests/`**: unit and stress tests for the library, and the tests for the exercises;
- **`sample-output/`**: the full output of every example on the book's reference machine.

## Requirements

- Linux on x86-64 or AArch64 (the code is portable C++20, but the examples use Linux thread pinning and the x86 TSC where available);
- GCC 13 or later, or Clang 17 or later with libstdc++ (libc++ does not yet provide `std::atomic<std::shared_ptr>`, used by `pet/calibration.hpp`); the whole project, sanitizer builds included, is tested with GCC 13.3 and Clang 18.1;
- CMake 3.20 or later, and optionally Ninja.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build -LE exercise        # library, solutions, examples: all pass
ctest --test-dir build -L exercise         # your exercises: all fail until you solve them
```

Sanitizer builds (use GCC for these if your Clang lacks the sanitizer runtimes):

```sh
cmake -S . -B build-tsan -DLLDS_SANITIZE=thread -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-tsan -j && ctest --test-dir build-tsan -LE exercise

cmake -S . -B build-asan -DLLDS_SANITIZE=address,undefined -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-asan -j && ctest --test-dir build-asan -LE exercise
```

Under ThreadSanitizer, `smoke_ch16_stress` is expected to report a race: the example contains a deliberately buggy ring (Chapter 16), and the test is marked `WILL_FAIL` in that configuration.

Pass `-DLLDS_NATIVE=ON` to compile with `-march=native`.

## Test labels

| Label | What it runs |
|---|---|
| `library` | `tests/test_*.cpp`: unit and stress tests of `include/llds` |
| `smoke` | every example with `--smoke` (tiny iteration counts) |
| `solution` | each exercise's tests against `solutions/chNN/` |
| `exercise` | each exercise's tests against your code in `exercises/chNN/` |

Run one exercise with, for example, `ctest --test-dir build -R ex_ch12` or directly with `./build/ex_ch12_config_holder`. The Foundations exercises are `ex_f04` to `ex_f08` (`ctest --test-dir build -R ex_f`).

## Library layout

| Header | Contents | Chapter |
|---|---|---|
| `common/cacheline.hpp` | `kCacheLine`, `kFalseSharingRange` (128), `Padded<T>`, power-of-two helpers | 2 |
| `common/cpu.hpp` | `pin_this_thread`, `cpu_relax`, `hardware_threads` | 2, 17 |
| `common/timer.hpp` | `now_ns`, TSC reading and calibration (`TscClock`), `do_not_optimize`, `clobber_memory` | 4 |
| `common/histogram.hpp` | log-linear latency histogram with mergeable per-thread instances | 4 |
| `common/bench.hpp` | `measure_each`, environment printing | 4 |
| `sync/spinlock.hpp` | TAS, TTAS, TTAS with backoff, ticket and MCS locks | 5 |
| `sync/seqlock.hpp` | `SeqLock<T>` (relaxed atomic payload words plus fences) | 7 |
| `stack/treiber_stack.hpp` | Treiber stack, tagged-pointer variant | 6 |
| `queue/spsc_ring.hpp` | `SpscRing<T>`: the final ring of Chapter 8 | 8 |
| `queue/mpmc_bounded.hpp` | Vyukov's bounded MPMC queue | 9 |
| `queue/ms_queue.hpp` | Michael-Scott queue with hazard pointers | 9 |
| `queue/disruptor.hpp` | single-producer Disruptor with sequences and dependencies | 10 |
| `mem/object_pool.hpp` | `LockFreePool<T>`, `Arena` | 11 |
| `reclaim/hazard_pointers.hpp` | `HazardDomain` | 12 |
| `reclaim/epoch.hpp` | `EpochDomain` | 12 |
| `map/striped_hash_map.hpp` | lock-striped hash map | 13 |
| `map/concurrent_int_map.hpp` | lock-free open-addressing map for 64-bit keys and values | 13 |
| `map/rcu_map.hpp` | read-copy-update map on epochs | 13 |
| `ordered/lazy_skiplist.hpp` | the lazy concurrent skip list | 14 |
| `ordered/order_book.hpp` | `MapBook` and `LadderBook` with seqlock-published top of book | 14, 15 |
| `pet/hit.hpp` | synthetic detector hit, 16-byte wire format, checked decoder | 19 |
| `pet/coincidence.hpp` | coincidence oracle, batch sliding window, `StreamingSorter` | 20 |
| `pet/merge.hpp` | `WatermarkMerger`: time-ordered merge of several sources | 20 |
| `pet/calibration.hpp` | immutable versioned `CalibrationTable`, `CalibrationHolder`, energy window | 21 |

`#include <llds/llds.hpp>` includes everything.

## Examples

Every example accepts `--smoke` (tiny iteration counts) and `--cpus=A,B` to choose the logical CPUs its main threads are pinned to. Most take from a few seconds to a minute at full length.

| Program | Chapter | Measures |
|---|---|---|
| `ch01_jitter` | 1 | gaps seen by an idle spinning thread |
| `ch02_cache_latency`, `ch02_false_sharing` | 2 | pointer-chase latency by working-set size; false sharing |
| `ch03_litmus` | 3 | store-buffering and message-passing litmus tests |
| `ch04_timers`, `ch04_coordinated_omission` | 4 | timer costs; naive vs corrected latency measurement |
| `ch05_locks` | 5 | lock throughput and acquisition latency |
| `ch06_atomics` | 6 | atomic operation costs, contended counters, ABA demonstration |
| `ch07_seqlock` | 7 | seqlock vs mutex vs reader-writer lock |
| `ch08_spsc_steps` | 8 | five versions of the SPSC ring: throughput and round trip |
| `ch09_mpmc` | 9 | MPMC queues, flooded and paced |
| `ch10_pipeline` | 10 | chained SPSC rings vs Disruptor pipeline |
| `ch11_allocators` | 11 | `new`/`delete`, pools, arena, pmr, return ring |
| `ch12_reclamation` | 12 | read cost of mutex, `atomic<shared_ptr>`, hazard pointers, epochs |
| `ch13_maps` | 13 | five concurrent maps at three write rates |
| `ch14_skiplist` | 14 | `std::map` with locks vs lazy skip list; range scans |
| `ch15_data_oriented` | 15 | AoS vs SoA, branchy vs branchless, two order books |
| `ch16_stress` | 16 | a correct and a buggy ring under stress (run it under TSan too) |
| `ch17_tuning` | 17 | a pipeline under four wait and pinning strategies |
| `ch18_patient_monitor` | 18 | the patient-monitor case study, with self-checks |
| `ch19_pet_daq` | 19 to 22 | a simulated PET acquisition pipeline: loss accounting, watermark merge, streaming coincidences checked against the oracle, calibration swap; `--rate=N`, `--slow-analysis`, `--corrupt`, `--check` |

## Exercises

| Exercise | Starter | Topic |
|---|---|---|
| F4.1 | `exercises/f04/le_parse.hpp` | little-endian fields without casts; a checked calibration |
| F5.1 | `exercises/f05/parallel_sum.hpp` | a parallel sum with private partials; an exact atomic counter |
| F6.1 | `exercises/f06/closable_queue.hpp` | a bounded blocking queue with a close-and-drain contract |
| F8.1 | `exercises/f08/mod_ring.hpp` | the first SPSC ring, with modulo indices |
| 2.1 | `exercises/ch02/padded_counters.hpp` | per-thread counters without false sharing |
| 3.1 | `exercises/ch03/mailbox.hpp` | a one-slot mailbox with release/acquire |
| 4.1, 4.2 | `exercises/ch04/percentiles.hpp` | nearest-rank percentiles; coordinated-omission correction |
| 5.1 | `exercises/ch05/rw_spinlock.hpp` | a writer-preferring reader-writer spinlock |
| 6.1, 6.2 | `exercises/ch06/atomic_ops.hpp` | `fetch_max` and bounded increment with CAS |
| 7.1 | `exercises/ch07/vitals_board.hpp` | a seqlock from scratch |
| 8.1 | `exercises/ch08/batch_ring.hpp` | batched SPSC push and pop |
| 9.1 | `exercises/ch09/blocking_queue.hpp` | spin-then-wait blocking queue with `atomic::wait` |
| 10.1 | `exercises/ch10/drain.hpp` | a batching Disruptor consumer step |
| 11.1 | `exercises/ch11/fixed_pool.hpp` | a fixed-size object pool |
| 12.1 | `exercises/ch12/config_holder.hpp` | a hot-swappable configuration with hazard pointers |
| 13.1 | `exercises/ch13/sharded_counter.hpp` | a sharded counter |
| 14.1 | `exercises/ch14/simple_skiplist.hpp` | a sequential skip list |
| 15.1 | `exercises/ch15/particles.hpp` | AoS to SoA, vectorisable and branch-free |
| 16.1 | `exercises/ch16/spsc_find_the_bugs.hpp` | find two bugs in an SPSC ring |
| 17.1 | `exercises/ch17/cpu_list.hpp` | parse a Linux CPU list |
| 18.1 | `exercises/ch18/threshold_alarm.hpp` | a threshold alarm with persistence and hysteresis |
| 19.1 | `exercises/ch19/sequence_tracker.hpp` | lost, late and duplicate records from wrapping sequence numbers |
| 20.1 | `exercises/ch20/single_window.hpp` | single-window coincidence grouping, two multiples policies |
| 20.2 | `exercises/ch20/delayed_window.hpp` | a delayed-window randoms estimate in O(n + C) |
| 21.1 | `exercises/ch21/pair_histogram.hpp` | a parallel histogram with private bins |

Each starter compiles and runs, but fails its tests. Work in `exercises/`; compare with `solutions/` only when you are done.

## A note on the numbers

The sample output in the book came from a 4-vCPU cloud VM (Intel Xeon at 2.1 GHz, Linux 6.18, GCC 13.3, `-O2`), deliberately an ordinary, noisy machine. Expect your numbers to differ; the relative results are what carry over. Appendix B of the book describes how to set up a quiet lab machine.

## Safety note

The patient-monitor example (Chapter 18) and the PET acquisition example (Chapters 19 to 22) are teaching examples. Their thresholds, rates, energies, windows and budgets are illustrative, not clinical or regulatory values; the PET simulation is generic and is not any manufacturer's design. Neither is suitable for use in any medical device.

## Licence

The code is provided for learning, under the MIT licence (see `LICENSE`): use it, change it and ship it, with no warranty of any kind. The text of the book is © 2026 Oussama Seghaier, all rights reserved (see [`book/LICENSE.md`](book/LICENSE.md)).
