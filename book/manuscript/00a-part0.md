# Part 0. Foundations {.part}

The rest of this book assumes that you are comfortable with modern C++ and have written a few multithreaded programs. This part is for readers who are not there yet, or who learned C++ before C++11 and want the gaps filled in the order the later chapters need them. It is short on purpose: eight chapters, each ending in exercises, that take you from values, ownership and bytes to your first threads, locks, atomics and a ring buffer you can explain line by line.

If you already know `std::unique_ptr`, `std::span`, `std::condition_variable` and release/acquire, skim the summaries and go to Chapter 1. If you do not, work through all eight. Each one tells you which later chapter takes its topic further.

One question runs through every chapter of this part, and through the rest of the book. For each piece of data you share, you should be able to answer three things: **who owns it**, **which operation lets another thread see it**, and **what happens when that operation fails or the system stops**.
