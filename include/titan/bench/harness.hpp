#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "titan/bench/latency_histogram.hpp"

namespace titan {

struct BenchConfig {
    size_t warmupIters;
    size_t measureIters;
};

struct BenchResult {
    uint64_t ops;
    double opsPerSec;
    LatencyHistogram latency;
};

// Runs warmupIters unmeasured, then measureIters with per-call latency recorded.
BenchResult run(const BenchConfig& config, const std::function<void()>& op);

}  // namespace titan
