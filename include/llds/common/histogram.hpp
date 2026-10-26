// llds/common/histogram.hpp
// A small log-linear latency histogram in the spirit of HdrHistogram.
//
// Values are recorded in integer units (we use nanoseconds). Each power-of-two
// range [2^k, 2^(k+1)) is split into kSubBuckets linear sub-buckets, so the
// relative error of any reported percentile is at most 1/kSubBuckets
// (about 1.6% with 64 sub-buckets). Recording is a few instructions and never
// allocates, which matters: a histogram that allocates on the hot path
// measures the allocator, not your code.
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <string>

namespace llds {

class Histogram {
public:
    static constexpr int kSubBits = 6;                     // 64 sub-buckets
    static constexpr int kSubBuckets = 1 << kSubBits;
    static constexpr int kMagnitudes = 64 - kSubBits + 1;  // covers all of uint64
    static constexpr int kBuckets = kMagnitudes * kSubBuckets;

    void record(std::uint64_t v) noexcept {
        counts_[index_of(v)]++;
        total_++;
        min_ = std::min(min_, v);
        max_ = std::max(max_, v);
        sum_ += static_cast<double>(v);
    }

    void record_n(std::uint64_t v, std::uint64_t n) noexcept {
        if (!n) return;
        counts_[index_of(v)] += n;
        total_ += n;
        min_ = std::min(min_, v);
        max_ = std::max(max_, v);
        sum_ += static_cast<double>(v) * static_cast<double>(n);
    }

    void merge(const Histogram& o) noexcept {
        for (int i = 0; i < kBuckets; ++i) counts_[i] += o.counts_[i];
        total_ += o.total_;
        min_ = std::min(min_, o.min_);
        max_ = std::max(max_, o.max_);
        sum_ += o.sum_;
    }

    void reset() noexcept { *this = Histogram{}; }

    std::uint64_t count() const noexcept { return total_; }
    std::uint64_t min() const noexcept { return total_ ? min_ : 0; }
    std::uint64_t max() const noexcept { return max_; }
    double mean() const noexcept { return total_ ? sum_ / static_cast<double>(total_) : 0.0; }

    // Value at percentile p in [0, 100]. Returns the upper edge of the bucket
    // that contains the p-th percentile sample, clamped to the observed max.
    std::uint64_t percentile(double p) const noexcept {
        if (!total_) return 0;
        if (p >= 100.0) return max_;
        const auto rank = static_cast<std::uint64_t>(
            (p / 100.0) * static_cast<double>(total_) + 0.5);
        std::uint64_t seen = 0;
        for (int i = 0; i < kBuckets; ++i) {
            seen += counts_[i];
            if (seen >= std::max<std::uint64_t>(rank, 1))
                return std::min(upper_edge(i), max_);
        }
        return max_;
    }

    std::string summary(const char* unit = "ns") const {
        char buf[256];
        std::snprintf(buf, sizeof buf,
                      "n=%llu min=%llu p50=%llu p90=%llu p99=%llu p99.9=%llu "
                      "p99.99=%llu max=%llu mean=%.1f (%s)",
                      static_cast<unsigned long long>(count()),
                      static_cast<unsigned long long>(min()),
                      static_cast<unsigned long long>(percentile(50)),
                      static_cast<unsigned long long>(percentile(90)),
                      static_cast<unsigned long long>(percentile(99)),
                      static_cast<unsigned long long>(percentile(99.9)),
                      static_cast<unsigned long long>(percentile(99.99)),
                      static_cast<unsigned long long>(max()), mean(), unit);
        return buf;
    }

    static constexpr int index_of(std::uint64_t v) noexcept {
        if (v < static_cast<std::uint64_t>(kSubBuckets)) return static_cast<int>(v);
        const int msb = 63 - std::countl_zero(v);          // v >= 64, so msb >= 6
        const int mag = msb - kSubBits + 1;                // 1..58
        // The top bit is implicit, so the next kSubBits bits pick the
        // sub-bucket. Each bucket in magnitude m is 2^(m-1) values wide.
        const int sub = static_cast<int>((v >> (msb - kSubBits)) & (kSubBuckets - 1));
        return mag * kSubBuckets + sub;
    }

    static constexpr std::uint64_t upper_edge(int idx) noexcept {
        const int mag = idx / kSubBuckets;
        const int sub = idx % kSubBuckets;
        if (mag == 0) return static_cast<std::uint64_t>(sub);
        const int shift = mag - 1;
        const std::uint64_t base = (static_cast<std::uint64_t>(kSubBuckets) | sub) << shift;
        return base + ((std::uint64_t{1} << shift) - 1);
    }

private:
    std::array<std::uint64_t, kBuckets> counts_{};
    std::uint64_t total_ = 0;
    std::uint64_t min_ = ~std::uint64_t{0};
    std::uint64_t max_ = 0;
    double sum_ = 0.0;
};

}  // namespace llds
