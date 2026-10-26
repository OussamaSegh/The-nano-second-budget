# Preface {.unnumbered}

This book is about one question: how do you share data between threads when every microsecond counts, and still get the right answer?

The question shows up in more places than it used to. A trading gateway has a budget of a few microseconds from packet to order. A game engine has 16.7 milliseconds to simulate, animate and render a frame, spread across a dozen cores. An audio plug-in that misses its 5-millisecond deadline produces an audible click. A bedside patient monitor must raise an alarm within a bounded time of the sample that triggers it, every time, for years. A network appliance forwarding millions of packets a second cannot afford a lock on its hot path. These systems look different, but underneath they share the same problem: several threads, some shared state, and a hard limit on how long anyone may wait.

The standard tools, `std::mutex`, `std::queue` and `std::shared_ptr`, are correct and often fast enough. When they are not, the reasons are rarely in the C++ code itself. They are in the cache-coherence protocol, the store buffer, the memory model, the allocator, the scheduler and the measurement method. This book starts there, at the machine, and works up to complete data structures: locks, sequence locks, ring buffers, multi-producer queues, the Disruptor pattern, pools, hazard pointers, epoch-based reclamation, concurrent hash maps and skip lists. Every structure is built from scratch, explained line by line, tested under ThreadSanitizer, and measured.

### Who this book is for

The main part of the book is for a C++ programmer with a couple of years of experience: you know `std::thread`, `std::mutex` and `std::condition_variable`, you can read templates, and you have used `std::atomic` at least for a counter. You may never have written a lock-free algorithm, and you do not need to have studied computer architecture. If you have, the early chapters will go quickly.

If you are rusty, or learned C++ before C++11, start with Part 0. Its eight short Foundations chapters are a compact refresher, not a first course: they assume ordinary C++ and cover what the rest of the book takes for granted: ownership and lifetimes, bytes and integers, threads and data races, mutexes and condition variables, a first look at atomics and memory ordering, and a ring buffer explained line by line.

You work, or want to work, on systems where latency matters more than throughput: finance, games, audio and video, robotics, embedded and medical devices, networking, databases, or the infrastructure underneath them. You care about the 99.9th percentile, not the average.

### What you will build

The book comes with a companion code project, a small header-only library called `llds` (low-latency data structures) plus chapter examples, benchmarks and exercises. By the end you will have written, or at least read and modified, all of it:

- a benchmark harness with a latency histogram that does not lie about tails;
- five spinlocks, a reader-writer spinlock and a sequence lock;
- a lock-free stack and an ABA-proof tagged pointer;
- a single-producer single-consumer ring buffer that moves a message between cores in a few dozen nanoseconds;
- Vyukov's bounded multi-producer multi-consumer queue and the Michael-Scott unbounded queue;
- a Disruptor-style broadcast ring with dependent pipeline stages;
- lock-free object pools and arenas;
- hazard pointers and epoch-based reclamation;
- three concurrent hash maps and a concurrent skip list;
- two order books and a real-time patient-monitoring pipeline that ties everything together;
- a simulated PET data-acquisition pipeline, with wire-format decoding, loss detection, watermark merging of several sources, streaming coincidence sorting checked against an oracle, and versioned calibration.

### How the book is organised

**Part 0, Foundations** (Chapters F1 to F8), is the on-ramp described above. Skip it if you already know its material; each chapter ends with a summary that tells you quickly.

**Part I, The Machine** (Chapters 1 to 4), explains why latency is hard: where jitter comes from, how caches and coherence really work, what the C++ memory model promises, and how to measure latency without fooling yourself. Do not skip Chapter 4. Most performance claims you will ever read are wrong because of the mistakes it describes.

**Part II, Building Blocks** (Chapters 5 to 7), covers locks done well, atomics as a design tool, and sequence locks. These are the parts every later structure is assembled from.

**Part III, Queues and Buffers** (Chapters 8 to 11), is the heart of the book: the SPSC ring, MPMC queues, the Disruptor, and memory management without `malloc`.

**Part IV, Shared State at Scale** (Chapters 12 to 15), tackles the hardest problem in lock-free programming, safe memory reclamation, and then hash maps, ordered structures and data-oriented design.

**Part V, Correctness, Tuning and Practice** (Chapters 16 to 18), is about getting it right and keeping it fast: testing and verification, tuning the whole system, and case studies.

**Part VI, Applying It: PET Data Acquisition** (Chapters 19 to 22), follows one system all the way down: the acquisition software of a positron emission tomography scanner. It builds a detector pipeline with explicit ownership and loss rules, specifies and implements coincidence sorting and multi-source merging, handles overload and calibration changes, and ends with traceability, review and a staged capstone project.

The appendices contain a memory-ordering cheat sheet for x86 and ARM, a benchmarking lab set-up guide, a glossary, and solutions to every exercise.

### Conventions

Code in the text is taken from the companion project, sometimes shortened. When a listing is abridged the full version is named, for example `include/llds/queue/spsc_ring.hpp`. All code is C++20. It needs GCC 13 or later, or Clang 17 or later used with GCC's standard library, libstdc++ (Clang's own libc++ does not yet provide `std::atomic<std::shared_ptr>`, which Part VI uses), on Linux; it is tested with GCC 13.3 and Clang 18.1. C++23 and C++26 features are pointed out where they help, but never required.

Numbers matter in this book, so every measurement says where it came from. Unless stated otherwise, results were produced by the companion code on a 4-vCPU cloud virtual machine (Intel Xeon at 2.1 GHz, Linux 6.18, GCC 13.3, `-O2`). That is deliberately an ordinary, noisy machine, not a tuned lab server. Your numbers will differ, sometimes by a factor of two or more; the *shape* of the results (which design wins, by roughly how much, and why) is what carries over. Chapter 17 and Appendix B show how to build a quiet machine where the tails shrink dramatically.

Each chapter ends with exercises. Written exercises check your understanding. Hands-on exercises live in `exercises/chNN/` (and `exercises/fNN/` for Part 0) in the code project; each has a test that fails until you complete it, and a reference solution in `solutions/chNN/`. Solutions to all of them, with discussion, are in Appendix D.

### Getting the code running

You need a Linux machine (a virtual machine or WSL 2 is fine for everything except the tuning chapter), CMake 3.20 or later, and a C++20 compiler. The code is published at **@CODE_URL@**, which also lists errata. Clone the repository, or download the release archive for this edition (version @VERSION@) and unpack it:

```sh
git clone @CODE_URL@ llds-book-code && cd llds-book-code
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build -LE exercise        # library, solutions, examples: all pass
ctest --test-dir build -L exercise         # your exercises: all fail until you solve them
./build/ch08_spsc_steps                    # run any chapter's example
```

For the concurrency chapters, also build with ThreadSanitizer:

```sh
cmake -S . -B build-tsan -DLLDS_SANITIZE=thread -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-tsan -j && ctest --test-dir build-tsan -LE exercise
```

Every example accepts `--smoke` for a quick run and `--cpus=A,B` to choose the cores its threads are pinned to.

### A note on safety-critical software

One of the case studies in Chapter 18 is a patient-monitoring pipeline, Part VI is about the acquisition software of a PET scanner, and examples throughout the book borrow vital signs as data. That is because medical devices are a good illustration of hard latency requirements, not because the code in this book is fit for a medical device. Software for regulated devices is developed under a process (IEC 62304 and related standards) that covers requirements, risk analysis, verification and traceability far beyond the scope of this book. Treat the case studies as engineering sketches of the data-structure problems such systems face. The PET simulation in Part VI is generic and physically plausible in its orders of magnitude; it is not the design of any manufacturer's scanner, and its rates, windows and energies are not clinical or regulatory values.

### Acknowledgements

This book stands on the work of the people who invented and explained the algorithms in it: Maurice Herlihy, Nir Shavit, Maged Michael, Michael Scott, Dmitry Vyukov, Paul McKenney, Keir Fraser, Hans Boehm, Martin Thompson and the LMAX team, Jeff Preshing, Cliff Click, Anthony Williams, Fedor Pikus, and many others. Their papers, talks and blog posts are listed in the references. Any errors in retelling them are mine.
