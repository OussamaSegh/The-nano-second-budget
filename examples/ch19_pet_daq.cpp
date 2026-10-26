// Chapters 19 to 22: a simulated PET data-acquisition (DAQ) pipeline.
//
//   source 0 (modules 0-3) --SPSC of wire batches--+
//                                                   +-> sorter --SPSC--> analysis
//   source 1 (modules 4-7) --SPSC of wire batches--+    decode, seq check,
//                                                      watermark merge,
//                                                      coincidence sorting
//
// The "detector" is a ring of 8 modules. Each simulated annihilation emits
// two back-to-back 511 keV photons; module m's opposite is m + 4, so every
// true coincidence crosses the two sources and the merge must be right for
// the pairs to be found. Some events lose one photon (singles), some photons
// scatter and lose energy, and the timing has jitter, so the output contains
// true, scattered and random coincidences, as a real scanner's does.
//
// Design rules this program follows (Chapter 19):
//   * Loss can happen in exactly one place, the sources, which like hardware
//     cannot wait: a full ring drops the batch and counts it. Everything
//     downstream applies backpressure instead of dropping.
//   * Loss is detectable downstream without trusting the source's counters:
//     per-module sequence numbers expose every missing hit.
//   * Calibration is an immutable versioned snapshot, taken once per batch
//     of coincidences; the control thread swaps it mid-run.
//   * At the end, the books must balance, and with --check (implied by
//     --smoke) the streaming output is compared with the batch oracle.
//
// This is a teaching simulation. Its rates, energies, resolutions and
// windows are plausible orders of magnitude, not any scanner's values.
//
// Options: --smoke, --check (compare with the oracle; implied by --smoke),
//          --rate=N   decays per second of wall time (default 1,000,000;
//                     0 = as fast as the sources can go, which overloads
//                     the sorter on most machines),
//          --slow-analysis (a slow consumer: backpressure, then loss),
//          --corrupt (injects bad records the decoder must reject).
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/pet/calibration.hpp>
#include <llds/pet/coincidence.hpp>
#include <llds/pet/hit.hpp>
#include <llds/pet/merge.hpp>
#include <llds/queue/spsc_ring.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

#include "example_util.hpp"

namespace daq {

using namespace llds::pet;

constexpr std::uint16_t kModules = 8;
constexpr std::size_t kSources = 2;
constexpr std::size_t kBatchHits = 64;
constexpr std::uint64_t kWindowPs = 4'000;  // 4 ns coincidence window

struct WireBatch {
    std::uint64_t promise = 0;  // no later hit from this source is earlier
    std::uint64_t t_push_ns = 0;
    std::uint32_t n = 0;
    bool last = false;
    std::array<std::array<std::byte, kHitWireSize>, kBatchHits> rec;
};

constexpr std::size_t source_of(std::uint16_t module) { return module < kModules / 2 ? 0 : 1; }

// One simulated photon detection before digitisation.
struct Photon {
    std::uint64_t tick;
    std::uint16_t module;
    double energy_kev;
};

// Per-module gain errors that calibration v2 corrects (v1 assumes nominal).
constexpr std::array<double, kModules> kTrueGain{1.00, 0.82, 1.21, 0.95, 1.12, 0.85, 1.25, 0.90};

// The event generator. Both source threads run identical copies (same seed),
// so they agree on every event, and each keeps only its own modules' hits.
class EventGenerator {
public:
    explicit EventGenerator(std::uint64_t seed) : g_(seed) {}

    // Next event time and its photons (0, 1 or 2 detected).
    std::uint64_t next(std::array<Photon, 2>& out, int& n) {
        t_ += static_cast<std::uint64_t>(-std::log(1.0 - u()) * 25'000.0);  // mean 25 ns between decays
        const auto m = static_cast<std::uint16_t>(g_() % kModules);
        auto partner = static_cast<std::uint16_t>((m + kModules / 2) % kModules);
        if (u() < 0.15) partner = static_cast<std::uint16_t>((partner + (u() < 0.5 ? 1 : kModules - 1)) % kModules);
        n = 0;
        if (u() < 0.75) out[n++] = photon(m);       // each photon detected with some probability
        if (u() < 0.75) out[n++] = photon(partner);
        return t_;
    }

private:
    double u() { return std::uniform_real_distribution<double>(0.0, 1.0)(g_); }
    Photon photon(std::uint16_t m) {
        // Time: non-negative jitter up to ~1 ns (front-end delay + resolution).
        const double jitter = (u() + u() + u() + u()) * 250.0;
        // Energy: photopeak with ~11% FWHM, or Compton-scattered below it.
        double e = 511.0 * (1.0 + 0.047 * ((u() + u() + u() + u()) - 2.0) * 1.73);
        if (u() < 0.35) e = 150.0 + u() * 330.0;
        return {t_ + static_cast<std::uint64_t>(jitter), m, e};
    }
    std::mt19937_64 g_;
    std::uint64_t t_ = 1'000'000;
};

std::uint16_t digitise(const Photon& p) {
    // raw = E / (true gain), with the true gain unknown to calibration v1.
    const double raw = std::round(p.energy_kev / kTrueGain[p.module]);
    return static_cast<std::uint16_t>(std::clamp(raw, 0.0, 65535.0));
}

struct SourceStats {
    std::uint64_t hits_generated = 0, hits_sent = 0, hits_dropped = 0, batches_dropped = 0, corrupted = 0;
};

}  // namespace daq

int main(int argc, char** argv) {
    using namespace daq;
    ex::Args args(argc, argv);
    bool check = args.smoke, slow = false, corrupt = false;
    double rate = 1'000'000.0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--rate=", 7)) rate = std::atof(argv[i] + 7);
        if (!std::strcmp(argv[i], "--check")) check = true;
        if (!std::strcmp(argv[i], "--slow-analysis")) slow = true;
        if (!std::strcmp(argv[i], "--corrupt")) corrupt = true;
    }
    llds::print_environment();
    const std::uint64_t events = args.n(10'000'000);
    std::uint64_t t_start = 0;  // set before the sources start

    std::array<llds::SpscRing<WireBatch>*, kSources> rings{};
    std::vector<std::unique_ptr<llds::SpscRing<WireBatch>>> ring_storage;
    for (auto& r : rings) {
        ring_storage.push_back(std::make_unique<llds::SpscRing<WireBatch>>(256));
        r = ring_storage.back().get();
    }
    llds::SpscRing<Coincidence> to_analysis(1 << 14);

    std::vector<ModuleCalibration> nominal(kModules, {1.0, 0.0}), corrected;
    for (double g : kTrueGain) corrected.push_back({g, 0.0});
    CalibrationHolder calib(std::make_shared<const CalibrationTable>(1, nominal));

    std::array<SourceStats, kSources> src_stats{};
    std::array<std::uint32_t, kModules> final_seq{};  // written by each source for its own modules
    std::array<std::vector<Hit>, kSources> sent_log;  // --check only
    std::atomic<std::uint64_t> events_done{0};

    // ---- sources: simulate front-end electronics -------------------------
    auto source = [&](std::size_t s) {
        return std::thread([&, s] {
            EventGenerator gen(2026);
            SourceStats& st = src_stats[s];
            std::array<std::uint32_t, kModules> seq{};
            // Reorder heap: photons from overlapping events can complete out
            // of order; a hit is final once the event clock has passed it.
            std::vector<Hit> heap;
            heap.reserve(4096);
            auto later = [](const Hit& a, const Hit& b) { return a.tick > b.tick; };
            WireBatch b;
            auto send = [&](std::uint64_t promise, bool last) {
                b.promise = promise;
                b.last = last;
                b.t_push_ns = llds::now_ns();
                if (last) {
                    while (!rings[s]->try_push(b)) llds::cpu_relax();  // end-of-run marker must arrive
                } else if (!rings[s]->try_push(b)) {
                    ++st.batches_dropped;  // hardware cannot wait: drop and count
                    st.hits_dropped += b.n;
                    st.hits_sent -= b.n;
                }
                b.n = 0;
            };
            std::array<Photon, 2> ph;
            int n = 0;
            std::mt19937 fault_rng(static_cast<unsigned>(s) + 99);
            for (std::uint64_t e = 0; e < events; ++e) {
                if (rate > 0 && (e & 63) == 0) {  // pace the simulated detector against the wall clock
                    const auto due = t_start + static_cast<std::uint64_t>(static_cast<double>(e) * 1e9 / rate);
                    while (llds::now_ns() < due) llds::cpu_relax();
                }
                const auto t = gen.next(ph, n);
                for (int i = 0; i < n; ++i) {
                    if (source_of(ph[i].module) != s) continue;
                    heap.push_back({ph[i].tick, 0, ph[i].module, digitise(ph[i])});
                    std::push_heap(heap.begin(), heap.end(), later);
                    ++st.hits_generated;
                }
                while (!heap.empty() && heap.front().tick < t) {
                    std::pop_heap(heap.begin(), heap.end(), later);
                    Hit h = heap.back();
                    heap.pop_back();
                    h.seq = seq[h.module]++;  // numbered in emission order, as hardware would
                    if (check) sent_log[s].push_back(h);
                    b.rec[b.n] = encode_hit(h);
                    if (corrupt && fault_rng() % 100'000 == 0) {
                        b.rec[b.n][12] = std::byte{200};  // module 200: must be rejected
                        ++st.corrupted;
                    }
                    ++b.n;
                    ++st.hits_sent;
                    // The promise covers what is still in the heap, not just
                    // the event clock: those hits will follow in later batches.
                    if (b.n == kBatchHits) send(heap.empty() ? t : std::min(t, heap.front().tick), false);
                }
                if (s == 0 && (e & 1023) == 0) events_done.store(e, std::memory_order_relaxed);
            }
            while (!heap.empty()) {
                std::pop_heap(heap.begin(), heap.end(), later);
                Hit h = heap.back();
                heap.pop_back();
                h.seq = seq[h.module]++;
                if (check) sent_log[s].push_back(h);
                b.rec[b.n++] = encode_hit(h);
                ++st.hits_sent;
                if (b.n == kBatchHits) send(heap.empty() ? h.tick : heap.front().tick, false);
            }
            for (std::uint16_t m = 0; m < kModules; ++m)
                if (source_of(m) == s) final_seq[m] = seq[m];
            send(WatermarkMerger::kClosed, true);
        });
    };

    // ---- sorter: decode, check sequence numbers, merge, pair -------------
    std::uint64_t decoded = 0, decode_errors = 0, seq_gaps = 0, coincidences = 0, merge_rejects = 0;
    llds::Histogram queue_delay;  // batch push -> sorter pop, ns
    StreamingSorter sorter(kWindowPs, 1024);
    std::vector<Coincidence> streamed;  // --check only
    std::array<std::uint32_t, kModules> expect{};  // next sequence number per module
    std::thread sorter_thread([&] {
        WatermarkMerger merger(kSources, 1 << 14);
        auto to_sorter = [&](const Hit& h) {
            // Unsigned subtraction: correct across the 2^32 wrap as long as
            // fewer than 2^32 hits go missing in a row.
            seq_gaps += static_cast<std::uint32_t>(h.seq - expect[h.module]);
            expect[h.module] = h.seq + 1;
            sorter.push(h, [&](const Coincidence& c) {
                if (check) streamed.push_back(c);
                ++coincidences;
                while (!to_analysis.try_push(c)) llds::cpu_relax();  // backpressure, never drop
            });
        };
        std::size_t open = kSources;
        while (open) {
            bool any = false;
            for (std::size_t s = 0; s < kSources; ++s) {
                if (merger.buffered(s) + kBatchHits > (1u << 14)) continue;  // no room: leave it in the ring
                WireBatch* b = rings[s]->front();
                if (!b) continue;
                any = true;
                queue_delay.record(llds::now_ns() - b->t_push_ns);
                for (std::uint32_t i = 0; i < b->n; ++i) {
                    Hit h;
                    if (decode_hit(b->rec[i], kModules, h) != DecodeStatus::ok || source_of(h.module) != s) {
                        ++decode_errors;
                        continue;
                    }
                    ++decoded;
                    if (!merger.add(s, h)) ++merge_rejects;  // broke its source's promise: a source bug
                }
                if (b->last) {
                    merger.close(s);
                    --open;
                } else {
                    merger.promise(s, b->promise);
                }
                rings[s]->pop_front();
            }
            merger.drain(to_sorter);
            if (!any) llds::cpu_relax();
        }
        merger.drain(to_sorter);
    });

    // ---- analysis: calibrate, energy-qualify, histogram ------------------
    std::atomic<bool> sorter_done{false};
    std::uint64_t analysed = 0;
    std::array<std::uint64_t, 3> accepted_by_version{}, seen_by_version{};
    std::array<std::uint64_t, kModules / 2 + 1> by_separation{};  // modules apart, accepted
    std::thread analysis([&] {
        const EnergyWindow win{425.0, 650.0};
        auto snap = calib.snapshot();
        Coincidence c;
        for (;;) {
            if (!to_analysis.try_pop(c)) {
                if (sorter_done.load(std::memory_order_acquire) && to_analysis.empty_approx()) break;
                llds::cpu_relax();
                continue;
            }
            if ((analysed & 1023) == 0) snap = calib.snapshot();  // one snapshot per batch of 1024
            ++analysed;
            ++seen_by_version[snap->version()];
            if (win.contains(snap->energy_kev(c.first)) && win.contains(snap->energy_kev(c.second))) {
                ++accepted_by_version[snap->version()];
                const int d = std::abs(int(c.first.module) - int(c.second.module));
                ++by_separation[static_cast<std::size_t>(std::min(d, kModules - d))];
            }
            if (slow && (analysed & 63) == 0) std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    const auto t0 = llds::now_ns();
    t_start = t0;
    std::thread s0 = source(0), s1 = source(1);
    // Control thread: publish the corrected calibration halfway through.
    while (events_done.load(std::memory_order_relaxed) < events / 2 && events > 2048) std::this_thread::yield();
    calib.publish(std::make_shared<const CalibrationTable>(2, corrected));
    s0.join();
    s1.join();
    sorter_thread.join();
    sorter_done.store(true, std::memory_order_release);
    analysis.join();
    const double secs = static_cast<double>(llds::now_ns() - t0) / 1e9;

    // ---- the books must balance -------------------------------------------
    std::uint64_t generated = 0, sent = 0, dropped = 0, batches_dropped = 0, corrupted = 0;
    for (auto& st : src_stats) {
        generated += st.hits_generated;
        sent += st.hits_sent;
        dropped += st.hits_dropped;
        batches_dropped += st.batches_dropped;
        corrupted += st.corrupted;
    }
    // Hits missing at the very end of a module's stream leave no gap behind
    // them; the sources' final counts reveal those too.
    for (std::uint16_t m = 0; m < kModules; ++m) seq_gaps += static_cast<std::uint32_t>(final_seq[m] - expect[m]);
    const auto& sc = sorter.counters();
    std::printf("simulated %llu decays in %.2f s: %llu hits (%.1f M hits/s through the pipeline)\n",
                (unsigned long long)events, secs, (unsigned long long)generated, static_cast<double>(decoded) / secs / 1e6);
    std::printf("accounting: generated %llu = sent %llu + dropped at source %llu (%llu batches)\n",
                (unsigned long long)generated, (unsigned long long)sent, (unsigned long long)dropped,
                (unsigned long long)batches_dropped);
    std::printf("            sent %llu = decoded %llu + rejected by decoder %llu (injected %llu)\n",
                (unsigned long long)sent, (unsigned long long)decoded, (unsigned long long)decode_errors,
                (unsigned long long)corrupted);
    std::printf("            sequence gaps seen downstream: %llu (dropped + rejected = %llu)\n",
                (unsigned long long)seq_gaps, (unsigned long long)(dropped + decode_errors));
    std::printf("sorter: %llu hits in, %llu coincidences, out-of-order %llu, forced evictions %llu, peak retained %zu\n",
                (unsigned long long)sc.hits_in, (unsigned long long)coincidences, (unsigned long long)sc.out_of_order,
                (unsigned long long)sc.forced_evictions, sc.retained_peak);
    std::printf("queue delay (source push -> sorter pop): %s\n", queue_delay.summary().c_str());
    for (std::uint32_t v = 1; v <= 2; ++v)
        if (seen_by_version[v])
            std::printf("calibration v%u: %llu coincidences, %llu in the 425-650 keV window (%.1f%%)\n", v,
                        (unsigned long long)seen_by_version[v], (unsigned long long)accepted_by_version[v],
                        100.0 * static_cast<double>(accepted_by_version[v]) / static_cast<double>(seen_by_version[v]));
    std::printf("accepted coincidences by module separation:");
    for (std::size_t d = 0; d < by_separation.size(); ++d) std::printf(" %zu:%llu", d, (unsigned long long)by_separation[d]);
    std::printf("   (4 = opposite modules: mostly trues)\n");

    bool ok = generated == sent + dropped && sent == decoded + decode_errors &&
              seq_gaps == dropped + decode_errors && analysed == coincidences && sc.out_of_order == 0 && merge_rejects == 0;
    if (check) {
        // Oracle: the source logs merged in the merger's order, then the
        // batch sliding window. Only valid if nothing was lost on the way.
        std::vector<Hit> all;
        for (auto& v : sent_log) all.insert(all.end(), v.begin(), v.end());
        std::stable_sort(all.begin(), all.end(), [](const Hit& a, const Hit& b) {
            return a.tick != b.tick ? a.tick < b.tick : source_of(a.module) < source_of(b.module);
        });
        if (dropped == 0 && decode_errors == 0 && sc.forced_evictions == 0) {
            const bool same = window_pairs(all, kWindowPs) == streamed;
            std::printf("check: streaming output %s the batch oracle (%zu pairs)\n", same ? "equals" : "DIFFERS FROM",
                        streamed.size());
            ok = ok && same;
        } else {
            std::printf("check: skipped oracle comparison (data was lost, as accounted above)\n");
        }
    }
    std::printf("%s\n", ok ? "books balance: yes" : "books balance: NO");
    return ok ? 0 : 1;
}
