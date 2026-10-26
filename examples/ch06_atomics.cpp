// Chapter 6: the cost of atomic operations, uncontended and contended, and
// the ABA problem in a naive lock-free stack.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/stack/treiber_stack.hpp>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "example_util.hpp"

// ---------------------------------------------------------------------------
// Part 1: what does each operation cost on one core?
static void single_thread_costs(const ex::Args& args) {
    const auto n = args.n(50'000'000);
    std::atomic<std::uint64_t> a{0};
    std::uint64_t plain = 0;
    std::printf("single thread, per operation:\n");
    std::printf("  plain increment        %6.2f ns\n", llds::measure_batch([&] { llds::do_not_optimize(++plain); }, n));
    std::printf("  load(relaxed)          %6.2f ns\n", llds::measure_batch([&] { llds::do_not_optimize(a.load(std::memory_order_relaxed)); }, n));
    std::printf("  load(seq_cst)          %6.2f ns\n", llds::measure_batch([&] { llds::do_not_optimize(a.load()); }, n));
    std::printf("  store(release)         %6.2f ns\n", llds::measure_batch([&] { a.store(1, std::memory_order_release); }, n));
    std::printf("  store(seq_cst)         %6.2f ns\n", llds::measure_batch([&] { a.store(1); }, n));
    std::printf("  fetch_add(relaxed)     %6.2f ns\n", llds::measure_batch([&] { a.fetch_add(1, std::memory_order_relaxed); }, n));
    std::printf("  compare_exchange       %6.2f ns\n", llds::measure_batch([&] {
                    auto e = a.load(std::memory_order_relaxed);
                    a.compare_exchange_strong(e, e + 1);
                }, n));
}

// ---------------------------------------------------------------------------
// Part 2: fetch_add vs a CAS loop when T threads hammer one counter.
static void contended_counter(const ex::Args& args) {
    const auto n = args.n(5'000'000);
    std::printf("\ncontended counter, ns per increment (wall time / total ops):\n");
    for (unsigned t = 1; t <= std::min(8u, llds::hardware_threads()); t *= 2) {
        for (int use_cas = 0; use_cas < 2; ++use_cas) {
            std::atomic<std::uint64_t> c{0};
            std::vector<std::thread> ts;
            const auto t0 = llds::now_ns();
            for (unsigned i = 0; i < t; ++i)
                ts.emplace_back([&] {
                    for (std::uint64_t k = 0; k < n; ++k) {
                        if (use_cas) {
                            auto e = c.load(std::memory_order_relaxed);
                            while (!c.compare_exchange_weak(e, e + 1, std::memory_order_relaxed)) {}
                        } else {
                            c.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                });
            for (auto& th : ts) th.join();
            const double ns = static_cast<double>(llds::now_ns() - t0) / static_cast<double>(t * n);
            std::printf("  T=%u %-10s %6.2f ns\n", t, use_cas ? "CAS loop" : "fetch_add", ns);
        }
    }
}

// ---------------------------------------------------------------------------
// Part 3: ABA, made deterministic.
//
// The naive pop is:   top = head; next = top->next; CAS(head, top, next)
// We pause thread A between reading `next` and the CAS. Meanwhile thread B
// pops A and B, then pushes A back. head == A again, so A's CAS succeeds and
// installs its stale `next` (= B, which is no longer in the stack).
struct Node {
    int value;
    Node* next;
};

static void aba_demo() {
    std::printf("\nABA demonstration with a naive stack (A -> B -> C):\n");
    Node c{3, nullptr}, b{2, &c}, a{1, &b};
    std::atomic<Node*> head{&a};

    // Thread A starts pop(): reads top = A and next = B, then is "preempted".
    Node* top = head.load();
    Node* next = top->next;

    // Thread B runs pop(), pop(), push(A).
    Node* x = head.load();
    head.store(x->next);  // popped A
    Node* y = head.load();
    head.store(y->next);  // popped B; the stack is now just C
    x->next = head.load();
    head.store(x);        // pushed A back: A -> C

    // Thread A resumes. head is A, so the CAS succeeds...
    bool ok = head.compare_exchange_strong(top, next);
    std::printf("  naive CAS succeeded: %s; head is now node %d\n", ok ? "yes" : "no", head.load()->value);
    std::printf("  but node %d was popped by thread B: the stack is corrupted.\n", head.load()->value);

    // The same sequence against a tagged head fails the CAS, as it should.
    llds::TaggedPtr tagged_head(&a, 0);
    llds::TaggedPtr seen = tagged_head;           // thread A's snapshot (tag 0)
    tagged_head = llds::TaggedPtr(&b, 1);         // B pops A
    tagged_head = llds::TaggedPtr(&c, 2);         // B pops B
    tagged_head = llds::TaggedPtr(&a, 3);         // B pushes A
    std::atomic<llds::TaggedPtr> th{tagged_head};
    bool ok2 = th.compare_exchange_strong(seen, llds::TaggedPtr(&b, 1));
    std::printf("  tagged CAS succeeded: %s (tag was %u, expected 0)\n", ok2 ? "yes" : "no",
                static_cast<unsigned>(th.load().tag()));
}

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    llds::print_environment();
    single_thread_costs(args);
    contended_counter(args);
    aba_demo();
    return 0;
}
