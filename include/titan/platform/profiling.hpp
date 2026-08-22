#pragma once

#include <string>

namespace titan {

// Compile-time: perf(1) is Linux-only, detected via __linux__.
constexpr bool perfStatAvailable()
{
#ifdef __linux__
    return true;
#else
    return false;  // not available on this platform
#endif
}

// Suggested shell command for profiling `binaryPath`, or a generic note off-Linux.
inline std::string perfStatHint(const char* binaryPath)
{
#ifdef __linux__
    return std::string("perf stat -d ") + binaryPath;
#else
    return "perf unavailable on this platform -- see docs/benchmark-results/environment.md";
#endif
}

}  // namespace titan
