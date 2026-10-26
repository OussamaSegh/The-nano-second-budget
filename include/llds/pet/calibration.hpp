// llds/pet/calibration.hpp
// Versioned, immutable energy calibration (Chapter 21).
//
// A calibration table is validated once, when it is built, and never changed
// afterwards. To change calibration, the control path builds a new table and
// publishes it; the data path takes one snapshot per batch, so a batch is
// never processed half with old and half with new coefficients, and every
// result can say which version produced it.
//
// The snapshot is a std::shared_ptr published through
// std::atomic<std::shared_ptr> (Chapter 12). That costs a reference-count
// update or two per batch, which is negligible at batch frequency and would
// not be at per-hit frequency. Put the cost where the frequency allows it.
#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "hit.hpp"

namespace llds::pet {

struct ModuleCalibration {
    double gain_kev = 1.0;    // keV per ADC unit; must be finite and > 0
    double offset_kev = 0.0;  // must be finite
};

class CalibrationTable {
public:
    // Throws std::invalid_argument on any invalid coefficient: a table is
    // either entirely valid or never published.
    CalibrationTable(std::uint32_t version, std::vector<ModuleCalibration> modules)
        : version_(version), m_(std::move(modules)) {
        if (m_.empty()) throw std::invalid_argument("calibration: no modules");
        for (const auto& c : m_)
            if (!std::isfinite(c.gain_kev) || !std::isfinite(c.offset_kev) || c.gain_kev <= 0)
                throw std::invalid_argument("calibration: invalid coefficient");
    }

    std::uint32_t version() const noexcept { return version_; }
    std::size_t modules() const noexcept { return m_.size(); }

    // Calibrated energy in keV. The module index was validated by the decoder
    // against the same module count; an index outside the table is a
    // configuration mismatch, reported as NaN so that no energy window
    // accepts it.
    double energy_kev(const Hit& h) const noexcept {
        if (h.module < m_.size()) {
            const ModuleCalibration c = m_[h.module];
            return static_cast<double>(h.energy) * c.gain_kev + c.offset_kev;
        }
        return std::nan("");
    }

private:
    std::uint32_t version_;
    std::vector<ModuleCalibration> m_;
};

// Inclusive energy acceptance window. NaN compares false with everything, so
// it is rejected without a separate check.
struct EnergyWindow {
    double lo_kev;
    double hi_kev;
    bool contains(double e) const noexcept { return e >= lo_kev && e <= hi_kev; }
};

class CalibrationHolder {
public:
    explicit CalibrationHolder(std::shared_ptr<const CalibrationTable> initial) : cur_(std::move(initial)) {
        if (!cur_.load()) throw std::invalid_argument("calibration: null table");
    }
    void publish(std::shared_ptr<const CalibrationTable> t) {
        if (!t) throw std::invalid_argument("calibration: null table");
        cur_.store(std::move(t), std::memory_order_release);
    }
    std::shared_ptr<const CalibrationTable> snapshot() const { return cur_.load(std::memory_order_acquire); }

private:
    std::atomic<std::shared_ptr<const CalibrationTable>> cur_;
};

}  // namespace llds::pet
