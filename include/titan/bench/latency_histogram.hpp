#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace titan {

// Sorted-sample histogram; percentile() sorts lazily on first call after record().
class LatencyHistogram {
public:
    void record(uint64_t ns);

    // p in [0, 100]. Nearest-rank method. 0.0 on an empty histogram.
    double percentile(double p) const;

    void clear();
    size_t count() const { return samples_.size(); }
    double mean() const;

private:
    void ensureSorted() const;

    mutable std::vector<uint64_t> samples_;
    mutable bool sorted_{true};
    uint64_t sum_{0};
};

}  // namespace titan
