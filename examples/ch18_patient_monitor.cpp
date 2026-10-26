// Chapter 18 case study: a real-time patient-monitoring pipeline.
//
//   sensor threads            fusion/alarm thread            consumers
//   ECG  (simulated) --SPSC-->                    --SeqLock--> display (latest vitals)
//   SpO2 (simulated) --SPSC-->  AlarmEvaluator    --Disruptor-> audit log + alarm annunciator
//
// Requirements we design against (in the spirit of IEC 60601-1-8 alarm
// systems; the numbers here are illustrative, not regulatory):
//   * an alarm condition must be raised within 10 ms of the sample that
//     triggers it (our internal budget, far inside the clinical limit);
//   * the display must never show a torn record;
//   * the audit log must record every alarm transition, in order;
//   * nothing on the sample path may allocate, lock, or block.
//
// The program injects a desaturation episode (SpO2 below 88% for 1.5 s) and a
// tachycardia episode, measures sample-to-alarm latency, and verifies the log.
#include <llds/common/bench.hpp>
#include <llds/common/cpu.hpp>
#include <llds/queue/disruptor.hpp>
#include <llds/queue/spsc_ring.hpp>
#include <llds/sync/seqlock.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

#include "example_util.hpp"

namespace pm {

enum class Channel : std::uint8_t { HeartRate, SpO2 };

struct Sample {
    std::uint64_t t_ns;   // acquisition time (monotonic clock)
    std::int32_t value;   // bpm or permille
    Channel ch;
};

struct VitalsView {  // what the bedside display reads
    std::uint64_t t_ns = 0;
    std::int32_t heart_rate = 0;
    std::int32_t spo2 = 0;
    std::uint32_t active_alarms = 0;  // bit mask
    std::uint32_t generation = 0;
};

enum AlarmBit : std::uint32_t { kTachy = 1, kDesat = 2 };

struct AlarmEvent {
    std::uint64_t t_sample_ns = 0;  // sample that caused the transition
    std::uint64_t t_raised_ns = 0;  // when the evaluator published it
    std::uint32_t alarm = 0;
    bool active = false;
};

// Threshold alarm with hysteresis and a minimum persistence time: the
// condition must hold for `persist_ns` before the alarm is raised, and the
// value must recover past `clear_at` before it clears. This is what keeps a
// single noisy sample from causing alarm fatigue.
class ThresholdAlarm {
public:
    ThresholdAlarm(bool above, std::int32_t trigger, std::int32_t clear_at, std::uint64_t persist_ns)
        : above_(above), trigger_(trigger), clear_(clear_at), persist_(persist_ns) {}

    // Returns +1 when the alarm becomes active, -1 when it clears, 0 otherwise.
    int update(std::int32_t v, std::uint64_t t) {
        const bool bad = above_ ? v > trigger_ : v < trigger_;
        const bool good = above_ ? v < clear_ : v > clear_;
        if (!active_) {
            if (bad) {
                if (!pending_) { pending_ = true; since_ = t; }
                if (t - since_ >= persist_) { active_ = true; pending_ = false; return +1; }
            } else {
                pending_ = false;
            }
        } else if (good) {
            active_ = false;
            return -1;
        }
        return 0;
    }
    bool active() const { return active_; }

private:
    bool above_;
    std::int32_t trigger_, clear_;
    std::uint64_t persist_;
    bool active_ = false, pending_ = false;
    std::uint64_t since_ = 0;
};

}  // namespace pm

int main(int argc, char** argv) {
    using namespace pm;
    ex::Args args(argc, argv);
    llds::print_environment();

    const std::uint64_t seconds = args.smoke ? 3 : 20;
    const std::uint64_t hr_period_ns = 2'000'000;    // 500 Hz derived heart-rate stream
    const std::uint64_t spo2_period_ns = 4'000'000;  // 250 Hz pleth-derived SpO2 stream
    const std::uint64_t budget_ns = 10'000'000;      // 10 ms sample-to-alarm budget

    llds::SpscRing<Sample> hr_q(1024), spo2_q(1024);
    llds::SeqLock<VitalsView> display;
    llds::Disruptor<AlarmEvent> alarms(256);
    llds::Sequence& log_seq = alarms.add_consumer();
    llds::Sequence& annunciator_seq = alarms.add_consumer();
    std::atomic<bool> sensors_done{false}, evaluator_done{false};
    const auto start = llds::now_ns();
    const auto end = start + seconds * 1'000'000'000ull;

    // Scenario: tachycardia from 1.0 s to 2.0 s, desaturation 1.5 s to 3.0 s
    // (shifted later in the long run so the alarms clear and re-arm).
    auto hr_at = [&](std::uint64_t t) {
        const double s = static_cast<double>(t - start) / 1e9;
        const double phase = std::fmod(s, 3.0);
        return phase >= 1.0 && phase < 2.0 ? 145 : 72 + static_cast<int>(t / 1'000'000 % 5);
    };
    auto spo2_at = [&](std::uint64_t t) {
        const double s = static_cast<double>(t - start) / 1e9;
        const double phase = std::fmod(s + 1.5, 3.0);
        return phase < 1.5 ? 850 : 970;  // permille: 85.0% vs 97.0%
    };

    auto sensor = [&](llds::SpscRing<Sample>& q, Channel ch, std::uint64_t period, auto value_at, int core) {
        return std::thread([&, ch, period, value_at, core] {
            llds::pin_this_thread(core);
            for (std::uint64_t due = start; due < end; due += period) {
                while (llds::now_ns() < due) llds::cpu_relax();
                const auto t = llds::now_ns();
                while (!q.try_push(Sample{t, value_at(t), ch})) {}  // never drops
            }
        });
    };
    const int hw = static_cast<int>(llds::hardware_threads());
    std::thread hr_thread = sensor(hr_q, Channel::HeartRate, hr_period_ns, hr_at, 1 % hw);
    std::thread spo2_thread = sensor(spo2_q, Channel::SpO2, spo2_period_ns, spo2_at, 1 % hw);

    llds::Histogram alarm_latency;
    std::uint64_t budget_misses = 0, transitions = 0;

    std::thread evaluator([&] {
        llds::pin_this_thread(2 % hw);
        ThresholdAlarm tachy(true, 130, 120, 100'000'000);  // HR > 130 for 100 ms; clears < 120
        ThresholdAlarm desat(false, 880, 900, 200'000'000); // SpO2 < 88.0% for 200 ms; clears > 90.0%
        VitalsView view;
        auto handle = [&](const Sample& s) {
            int change = 0;
            std::uint32_t bit = 0;
            if (s.ch == Channel::HeartRate) {
                view.heart_rate = s.value;
                change = tachy.update(s.value, s.t_ns);
                bit = kTachy;
            } else {
                view.spo2 = s.value;
                change = desat.update(s.value, s.t_ns);
                bit = kDesat;
            }
            view.t_ns = s.t_ns;
            if (change) {
                view.active_alarms = change > 0 ? (view.active_alarms | bit) : (view.active_alarms & ~bit);
                const auto seq = alarms.claim();
                AlarmEvent& ev = alarms[seq];
                ev.t_sample_ns = s.t_ns;
                ev.alarm = bit;
                ev.active = change > 0;
                ev.t_raised_ns = llds::now_ns();
                alarms.publish(seq);
                const auto lat = ev.t_raised_ns - s.t_ns;
                alarm_latency.record(lat);
                if (lat > budget_ns) ++budget_misses;
                ++transitions;
            }
            ++view.generation;
            display.store(view);
        };
        Sample s;
        for (;;) {
            bool any = false;
            while (hr_q.try_pop(s)) { handle(s); any = true; }
            while (spo2_q.try_pop(s)) { handle(s); any = true; }
            if (!any) {
                if (sensors_done.load(std::memory_order_acquire) && hr_q.empty_approx() && spo2_q.empty_approx()) break;
                llds::cpu_relax();
            }
        }
        evaluator_done.store(true, std::memory_order_release);
    });

    // Audit log: records every transition in order.
    std::vector<AlarmEvent> log;
    std::thread logger([&] {
        std::int64_t next = 0;
        for (;;) {
            const auto avail = alarms.available({});
            for (; next <= avail; ++next) log.push_back(alarms[next]);
            log_seq.set(avail);
            if (evaluator_done.load(std::memory_order_acquire) && alarms.available({}) < next) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));  // logging is not latency critical
        }
    });
    // Annunciator: would drive the speaker/LED; here it just counts activations.
    std::uint64_t annunciations = 0;
    std::thread annunciator([&] {
        std::int64_t next = 0;
        for (;;) {
            const auto avail = alarms.available({});
            for (; next <= avail; ++next) annunciations += alarms[next].active;
            annunciator_seq.set(avail);
            if (evaluator_done.load(std::memory_order_acquire) && alarms.available({}) < next) break;
            llds::cpu_relax();
        }
    });

    // Display: reads at 60 Hz from the main thread, checks for torn reads.
    std::uint64_t frames = 0, last_gen = 0, stale = 0;
    while (llds::now_ns() < end) {
        VitalsView v = display.load();
        if (v.generation < last_gen) ++stale;
        last_gen = v.generation;
        ++frames;
        std::this_thread::sleep_for(std::chrono::microseconds(16'667));
    }

    hr_thread.join();
    spo2_thread.join();
    sensors_done.store(true, std::memory_order_release);
    evaluator.join();
    logger.join();
    annunciator.join();

    bool ordered = true;
    for (std::size_t i = 1; i < log.size(); ++i) ordered = ordered && log[i].t_raised_ns >= log[i - 1].t_raised_ns;

    std::printf("ran %llu s: %llu display frames, %llu alarm transitions, %llu annunciations\n",
                static_cast<unsigned long long>(seconds), static_cast<unsigned long long>(frames),
                static_cast<unsigned long long>(transitions), static_cast<unsigned long long>(annunciations));
    std::printf("sample-to-alarm latency: %s\n", alarm_latency.summary().c_str());
    std::printf("  (excludes the deliberate persistence delay; measures pipeline cost only)\n");
    std::printf("budget (10 ms) misses: %llu   audit log entries: %zu, in order: %s   display regressions: %llu\n",
                static_cast<unsigned long long>(budget_misses), log.size(), ordered ? "yes" : "NO",
                static_cast<unsigned long long>(stale));
    for (std::size_t i = 0; i < log.size() && i < 8; ++i)
        std::printf("  t=%7.3f s  %-6s %s\n", static_cast<double>(log[i].t_sample_ns - start) / 1e9,
                    log[i].alarm == kTachy ? "TACHY" : "DESAT", log[i].active ? "RAISED" : "cleared");
    // The latency budget is a property of a tuned, pinned machine (Chapter 17).
    // A --smoke run shares the CPUs with other tests, so there a miss is
    // reported but only the functional checks decide the exit status.
    const bool functional_ok = ordered && log.size() == transitions;
    if (args.smoke && budget_misses != 0)
        std::printf("note: budget misses ignored in --smoke mode (shared, untuned CPUs)\n");
    return (functional_ok && (args.smoke || budget_misses == 0)) ? 0 : 1;
}
