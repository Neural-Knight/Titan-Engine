#include "titan/bench/latency_histogram.hpp"

#include <algorithm>
#include <cmath>

namespace titan {

void LatencyHistogram::record(uint64_t ns)
{
    samples_.push_back(ns);
    sum_ += ns;
    sorted_ = false;
}

void LatencyHistogram::ensureSorted() const
{
    if (!sorted_)
    {
        std::sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
}

double LatencyHistogram::percentile(double p) const
{
    if (samples_.empty())
        return 0.0;

    ensureSorted();
    const double rank = (p / 100.0) * static_cast<double>(samples_.size());
    size_t idx = static_cast<size_t>(std::ceil(rank));
    idx = (idx == 0) ? 0 : idx - 1;
    if (idx >= samples_.size())
        idx = samples_.size() - 1;
    return static_cast<double>(samples_[idx]);
}

void LatencyHistogram::clear()
{
    samples_.clear();
    sum_ = 0;
    sorted_ = true;
}

double LatencyHistogram::mean() const
{
    if (samples_.empty())
        return 0.0;
    return static_cast<double>(sum_) / static_cast<double>(samples_.size());
}

}  // namespace titan
