// Chapter 4: coordinated omission, demonstrated with a simulation.
//
// A service normally answers in 2 us, but once every 20,000 requests it
// stalls for 20 ms (a GC pause, a page-cache flush, a log rotation...).
// Clients are supposed to send one request every 50 us.
//
// The naive closed-loop benchmark sends the next request only when the
// previous one returns and records the service time. During a stall it
// simply stops sending, so it records ONE slow sample where a real client
// population would have seen 400 queued requests, all of them slow.
//
// The corrected measurement charges each request from its *intended* send
// time, exactly as HdrHistogram's recordValueWithExpectedInterval does.
#include <llds/common/histogram.hpp>

#include <cstdio>

#include "example_util.hpp"

int main(int argc, char** argv) {
    ex::Args args(argc, argv);
    const std::uint64_t requests = args.smoke ? 200'000 : 2'000'000;
    const std::uint64_t interval_ns = 50'000;
    const std::uint64_t fast_ns = 2'000;
    const std::uint64_t stall_ns = 20'000'000;
    const std::uint64_t stall_every = 20'000;

    llds::Histogram naive, corrected, hdr_style;
    std::uint64_t clock = 0;  // simulated time when the service becomes free

    for (std::uint64_t i = 0; i < requests; ++i) {
        const std::uint64_t service = (i % stall_every == stall_every - 1) ? stall_ns : fast_ns;
        const std::uint64_t intended = i * interval_ns;  // open-loop schedule
        const std::uint64_t start = std::max(clock, intended);
        const std::uint64_t done = start + service;
        clock = done;

        naive.record(service);               // what a closed-loop tool reports
        corrected.record(done - intended);   // what users actually experience

        // HdrHistogram-style back-filling applied to the naive samples:
        // for a sample longer than the interval, add the samples that
        // "should" have been taken while we were stuck.
        hdr_style.record(service);
        for (std::uint64_t missing = service; missing > interval_ns; missing -= interval_ns)
            hdr_style.record(missing - interval_ns);
    }

    std::printf("naive (service time)        %s\n", naive.summary().c_str());
    std::printf("corrected (from intended)   %s\n", corrected.summary().c_str());
    std::printf("naive + expected-interval   %s\n", hdr_style.summary().c_str());
    std::printf("\nThe naive p99.9 is %.0fx too optimistic.\n",
                static_cast<double>(corrected.percentile(99.9)) /
                    static_cast<double>(naive.percentile(99.9)));
    return 0;
}
