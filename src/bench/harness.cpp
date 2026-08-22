#include "titan/bench/harness.hpp"

#include <chrono>

namespace titan {

BenchResult run(const BenchConfig& config, const std::function<void()>& op)
{
    for (size_t i = 0; i < config.warmupIters; ++i)
        op();

    BenchResult result{};
    const auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < config.measureIters; ++i)
    {
        const auto iterStart = std::chrono::steady_clock::now();
        op();
        const auto iterEnd = std::chrono::steady_clock::now();
        result.latency.record(
            std::chrono::duration_cast<std::chrono::nanoseconds>(iterEnd - iterStart).count());
    }
    const double totalSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    result.ops = config.measureIters;
    result.opsPerSec = (totalSeconds > 0.0) ? (static_cast<double>(config.measureIters) / totalSeconds) : 0.0;
    return result;
}

}  // namespace titan
