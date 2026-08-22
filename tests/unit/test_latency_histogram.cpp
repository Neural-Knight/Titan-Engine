#include <gtest/gtest.h>

#include "titan/bench/latency_histogram.hpp"

using namespace titan;

TEST(LatencyHistogram, EmptyHistogramReturnsZero) {
    LatencyHistogram hist;
    EXPECT_EQ(hist.count(), 0u);
    EXPECT_DOUBLE_EQ(hist.mean(), 0.0);
    EXPECT_DOUBLE_EQ(hist.percentile(50), 0.0);
    EXPECT_DOUBLE_EQ(hist.percentile(99), 0.0);
}

TEST(LatencyHistogram, PercentilesOnKnownDistribution) {
    LatencyHistogram hist;
    for (uint64_t i = 1; i <= 100; ++i)
        hist.record(i);

    EXPECT_EQ(hist.count(), 100u);
    EXPECT_DOUBLE_EQ(hist.mean(), 50.5);
    EXPECT_DOUBLE_EQ(hist.percentile(50), 50.0);
    EXPECT_DOUBLE_EQ(hist.percentile(95), 95.0);
    EXPECT_DOUBLE_EQ(hist.percentile(99), 99.0);
    EXPECT_DOUBLE_EQ(hist.percentile(99.9), 100.0);
}

TEST(LatencyHistogram, ClearResetsState) {
    LatencyHistogram hist;
    hist.record(10);
    hist.record(20);
    hist.clear();

    EXPECT_EQ(hist.count(), 0u);
    EXPECT_DOUBLE_EQ(hist.mean(), 0.0);
}

TEST(LatencyHistogram, RecordOrderDoesNotAffectSortedPercentiles) {
    LatencyHistogram hist;
    hist.record(30);
    hist.record(10);
    hist.record(20);

    EXPECT_DOUBLE_EQ(hist.percentile(50), 20.0);
}
