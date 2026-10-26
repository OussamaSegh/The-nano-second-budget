// Exercise 15.1: from array-of-structs to struct-of-arrays.
//
// Implement ParticlesSoA::from_aos, integrate and active_mass so that they
// give the same results as the AoS reference functions in the test, but
// with a layout that lets integrate() stream only the arrays it uses and
// lets the compiler vectorize it. active_mass() must not contain a
// data-dependent branch (use arithmetic or a conditional select).
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
        (void)in;
        return {};  // TODO
    }

    std::size_t size() const noexcept { return x.size(); }

    void integrate(float dt) noexcept { (void)dt; /* TODO */ }

    double active_mass() const noexcept { return 0.0; /* TODO */ }
};
