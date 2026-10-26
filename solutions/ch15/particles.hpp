// Exercise 15.1 (solution): from array-of-structs to struct-of-arrays.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct ParticleAoS {
    float x, y, z;
    float vx, vy, vz;
    float mass;
    std::uint32_t flags;  // bit 0: active
};

struct ParticlesSoA {
    std::vector<float> x, y, z, vx, vy, vz, mass;
    std::vector<std::uint32_t> flags;

    static ParticlesSoA from_aos(const std::vector<ParticleAoS>& in) {
        ParticlesSoA s;
        const std::size_t n = in.size();
        for (auto* v : {&s.x, &s.y, &s.z, &s.vx, &s.vy, &s.vz, &s.mass}) v->resize(n);
        s.flags.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            s.x[i] = in[i].x;   s.y[i] = in[i].y;   s.z[i] = in[i].z;
            s.vx[i] = in[i].vx; s.vy[i] = in[i].vy; s.vz[i] = in[i].vz;
            s.mass[i] = in[i].mass;
            s.flags[i] = in[i].flags;
        }
        return s;
    }

    std::size_t size() const noexcept { return x.size(); }

    // Advance every particle by dt. Touches only position and velocity
    // arrays: the loop vectorizes and streams 24 bytes per particle
    // instead of the full 32-byte record.
    void integrate(float dt) noexcept {
        const std::size_t n = size();
        float* __restrict px = x.data();
        float* __restrict py = y.data();
        float* __restrict pz = z.data();
        const float* __restrict qx = vx.data();
        const float* __restrict qy = vy.data();
        const float* __restrict qz = vz.data();
        for (std::size_t i = 0; i < n; ++i) {
            px[i] += qx[i] * dt;
            py[i] += qy[i] * dt;
            pz[i] += qz[i] * dt;
        }
    }

    // Total mass of active particles, without a data-dependent branch.
    double active_mass() const noexcept {
        double sum = 0.0;
        const std::size_t n = size();
        for (std::size_t i = 0; i < n; ++i) {
            const float m = (flags[i] & 1u) ? mass[i] : 0.0f;  // compiles to a select, not a jump
            sum += m;
        }
        return sum;
    }
};
