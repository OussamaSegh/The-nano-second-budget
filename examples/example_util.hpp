// examples/example_util.hpp -- tiny helpers shared by the chapter examples.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace ex {

// Every example accepts --smoke (tiny iteration counts, used by ctest) and
// --cpus=A,B to choose which logical CPUs threads are pinned to.
struct Args {
    bool smoke = false;
    int cpu_a = 0;
    int cpu_b = 1;

    Args(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            if (!std::strcmp(argv[i], "--smoke")) smoke = true;
            else if (!std::strncmp(argv[i], "--cpus=", 7)) std::sscanf(argv[i] + 7, "%d,%d", &cpu_a, &cpu_b);
        }
    }

    // Scale an iteration count down in smoke mode.
    std::uint64_t n(std::uint64_t full) const { return smoke ? (full / 1000 > 0 ? full / 1000 : 1) : full; }
};

}  // namespace ex
