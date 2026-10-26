#include <particles.hpp>

#include <cmath>
#include <random>

#include "check.hpp"

static std::vector<ParticleAoS> make(std::size_t n) {
    std::mt19937 rng(5);
    std::uniform_real_distribution<float> d(-1.0f, 1.0f);
    std::vector<ParticleAoS> v(n);
    for (auto& p : v) p = {d(rng), d(rng), d(rng), d(rng), d(rng), d(rng), 1.0f + d(rng), static_cast<std::uint32_t>(rng() & 1u)};
    return v;
}

TEST("ex15.1: SoA matches AoS reference") {
    auto aos = make(10007);
    auto soa = ParticlesSoA::from_aos(aos);
    REQUIRE(soa.size() == aos.size());
    for (int step = 0; step < 10; ++step) {
        for (auto& p : aos) {
            p.x += p.vx * 0.01f;
            p.y += p.vy * 0.01f;
            p.z += p.vz * 0.01f;
        }
        soa.integrate(0.01f);
    }
    bool same = true;
    for (std::size_t i = 0; i < aos.size(); ++i)
        same = same && std::abs(aos[i].x - soa.x[i]) < 1e-5f && std::abs(aos[i].y - soa.y[i]) < 1e-5f &&
               std::abs(aos[i].z - soa.z[i]) < 1e-5f;
    CHECK(same);
    double ref = 0;
    for (auto& p : aos) ref += (p.flags & 1u) ? p.mass : 0.0f;
    CHECK(std::abs(ref - soa.active_mass()) < 1e-6 * std::abs(ref) + 1e-9);
}

TEST_MAIN()
