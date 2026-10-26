## References {#references}

### Books

- Herlihy, M., Shavit, N., Luchangco, V. and Spear, M. *The Art of Multiprocessor Programming*, 2nd edition. Morgan Kaufmann, 2020.
- McKenney, P. E. *Is Parallel Programming Hard, And, If So, What Can You Do About It?* Free online book, kernel.org, continuously updated.
- Williams, A. *C++ Concurrency in Action*, 2nd edition. Manning, 2019.
- Drepper, U. *What Every Programmer Should Know About Memory*. Red Hat, 2007.
- Hennessy, J. L. and Patterson, D. A. *Computer Architecture: A Quantitative Approach*, 6th edition. Morgan Kaufmann, 2017.
- Gregg, B. *Systems Performance: Enterprise and the Cloud*, 2nd edition. Addison-Wesley, 2020.
- Sorin, D. J., Hill, M. D. and Wood, D. A. *A Primer on Memory Consistency and Cache Coherence*, 2nd edition. Morgan & Claypool, 2020.
- Stroustrup, B. *A Tour of C++*, 3rd edition. Addison-Wesley, 2022 (a compact companion to Part 0).

### Memory models and verification

- Boehm, H.-J. and Adve, S. V. "Foundations of the C++ Concurrency Memory Model". *PLDI*, 2008.
- Batty, M., Owens, S., Sarkar, S., Sewell, P. and Weber, T. "Mathematizing C++ Concurrency". *POPL*, 2011.
- Owens, S., Sarkar, S. and Sewell, P. "A Better x86 Memory Model: x86-TSO". *TPHOLs*, 2009.
- Pulte, C., Flur, S., Deacon, W., French, J., Sarkar, S. and Sewell, P. "Simplifying ARM Concurrency: Multicopy-Atomic Axiomatic and Operational Models for ARMv8". *POPL*, 2018.
- Boehm, H.-J. "Can Seqlocks Get Along With Programming Language Memory Models?" *MSPC*, 2012.
- Norris, B. and Demsky, B. "CDSChecker: Checking Concurrent Data Structures Written with C/C++ Atomics". *OOPSLA*, 2013.
- Kokologiannakis, M. and Vafeiadis, V. "GenMC: A Model Checker for Weak Memory Models". *CAV*, 2021.
- Serebryany, K. and Iskhodzhanov, T. "ThreadSanitizer: Data Race Detection in Practice". *WBIA*, 2009.
- Herlihy, M. and Wing, J. "Linearizability: A Correctness Condition for Concurrent Objects". *ACM TOPLAS*, 1990.

### Locks and synchronisation

- Mellor-Crummey, J. M. and Scott, M. L. "Algorithms for Scalable Synchronization on Shared-Memory Multiprocessors". *ACM TOCS*, 1991.
- Anderson, T. E. "The Performance of Spin Lock Alternatives for Shared-Memory Multiprocessors". *IEEE TPDS*, 1990.
- Drepper, U. "Futexes Are Tricky". Red Hat, 2004 (revised 2011).

### Lock-free structures and queues

- Treiber, R. K. "Systems Programming: Coping with Parallelism". IBM Research Report RJ 5118, 1986.
- Michael, M. M. and Scott, M. L. "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms". *PODC*, 1996.
- Vyukov, D. "Bounded MPMC Queue". 1024cores.net, 2010.
- Lamport, L. "Specifying Concurrent Program Modules". *ACM TOPLAS*, 1983 (the single-producer single-consumer ring).
- Thompson, M., Farley, D., Barker, M., Gee, P. and Stewart, A. "Disruptor: High Performance Alternative to Bounded Queues for Exchanging Data Between Concurrent Threads". LMAX technical paper, 2011.
- Chase, D. and Lev, Y. "Dynamic Circular Work-Stealing Deque". *SPAA*, 2005.
- Preshing, J. "The World's Simplest Lock-Free Hash Table". preshing.com, 2013.
- Click, C. "A Lock-Free Hash Table". JavaOne, 2007.
- Shalev, O. and Shavit, N. "Split-Ordered Lists: Lock-Free Extensible Hash Tables". *Journal of the ACM*, 2006.
- Li, X., Andersen, D. G., Kaminsky, M. and Freedman, M. J. "Algorithmic Improvements for Fast Concurrent Cuckoo Hashing". *EuroSys*, 2014.
- Pugh, W. "Skip Lists: A Probabilistic Alternative to Balanced Trees". *Communications of the ACM*, 1990.
- Herlihy, M., Lev, Y., Luchangco, V. and Shavit, N. "A Simple Optimistic Skiplist Algorithm". *SIROCCO*, 2007.
- Fraser, K. *Practical Lock-Freedom*. PhD thesis, University of Cambridge, 2004.
- Ellen, F., Fatourou, P., Ruppert, E. and van Breugel, F. "Non-Blocking Binary Search Trees". *PODC*, 2010.
- Natarajan, A. and Mittal, N. "Fast Concurrent Lock-Free Binary Search Trees". *PPoPP*, 2014.
- Bronson, N. G., Casper, J., Chafi, H. and Olukotun, K. "A Practical Concurrent Binary Search Tree". *PPoPP*, 2010.

### Memory reclamation

- Michael, M. M. "Hazard Pointers: Safe Memory Reclamation for Lock-Free Objects". *IEEE TPDS*, 2004.
- McKenney, P. E. and Slingwine, J. D. "Read-Copy Update: Using Execution History to Solve Concurrency Problems". *PDCS*, 1998.
- Desnoyers, M., McKenney, P. E., Stern, A. S., Dagenais, M. R. and Walpole, J. "User-Level Implementations of Read-Copy Update". *IEEE TPDS*, 2012.
- Hart, T. E., McKenney, P. E., Demke Brown, A. and Walpole, J. "Performance of Memory Reclamation for Lockless Synchronization". *Journal of Parallel and Distributed Computing*, 2007.
- Michael, M. M. and others. "Hazard Pointers for C++26". WG21 paper P2530.
- McKenney, P. E. and others. "Read-Copy Update (RCU)". WG21 paper P2545.

### Measurement and latency

- Tene, G. "How NOT to Measure Latency". Conference talk, 2013 onwards; and the HdrHistogram project.
- Dean, J. and Barroso, L. A. "The Tail at Scale". *Communications of the ACM*, 2013.
- Little, J. D. C. "A Proof for the Queuing Formula: L = λW". *Operations Research*, 1961.
- Paoloni, G. "How to Benchmark Code Execution Times on Intel IA-32 and IA-64 Instruction Set Architectures". Intel white paper, 2010.

### Data layout and hardware

- Intel Corporation. *Intel 64 and IA-32 Architectures Optimization Reference Manual*. Current edition.
- Arm Limited. *Arm Architecture Reference Manual for A-profile Architecture*. Current edition.
- Fog, A. *Optimizing Software in C++* and *The Microarchitecture of Intel, AMD and VIA CPUs*. agner.org, continuously updated.
- Acton, M. "Data-Oriented Design and C++". CppCon, 2014.
- Kulukundis, M. "Designing a Fast, Efficient, Cache-Friendly Hash Table, Step by Step". CppCon, 2017 (the Swiss table).
- Varghese, G. and Lauck, A. "Hashed and Hierarchical Timing Wheels". *SOSP*, 1987.

### Operating system and platform

- The Linux kernel documentation: "CPU Isolation", "NO_HZ: Reducing Scheduling-Clock Ticks", "Real-Time Preemption", the `membarrier(2)` and `sched(7)` manual pages.
- Red Hat. *Real Time Tuning Guide* and the `tuned` profiles documentation.
- The `rt-tests` project (`cyclictest`, `hwlatdetect`) and the `rtla` tools in the Linux kernel tree.

### PET physics, reconstruction and stream processing

- Cherry, S. R., Sorenson, J. A. and Phelps, M. E. *Physics in Nuclear Medicine*, 4th edition. Saunders/Elsevier, 2012.
- Bailey, D. L., Townsend, D. W., Valk, P. E. and Maisey, M. N. (eds.) *Positron Emission Tomography: Basic Sciences*. Springer, 2005.
- Shepp, L. A. and Vardi, Y. "Maximum Likelihood Reconstruction for Emission Tomography". *IEEE Transactions on Medical Imaging*, 1982 (MLEM).
- Hudson, H. M. and Larkin, R. S. "Accelerated Image Reconstruction Using Ordered Subsets of Projection Data". *IEEE Transactions on Medical Imaging*, 1994 (OSEM).
- Akidau, T. and others. "The Dataflow Model: A Practical Approach to Balancing Correctness, Latency, and Cost in Massive-Scale, Unbounded, Out-of-Order Data Processing". *Proceedings of the VLDB Endowment*, 2015 (watermarks).
- The Linux kernel documentation: "Dynamic DMA mapping Guide" (`core-api/dma-api-howto`).

### Standards cited

- ISO/IEC 14882:2020, *Programming Languages — C++* (C++20), and the C++26 working draft.
- IEC 62304, *Medical Device Software — Software Life Cycle Processes*.
- ISO 14971, *Medical Devices — Application of Risk Management to Medical Devices*.
- ISO 13485, *Medical Devices — Quality Management Systems — Requirements for Regulatory Purposes*.
- IEC 60601-1-8, *Medical Electrical Equipment — General Requirements, Tests and Guidance for Alarm Systems*.
