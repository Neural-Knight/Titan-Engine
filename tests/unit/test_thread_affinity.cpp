#include <gtest/gtest.h>

#include <thread>

#include "titan/platform/thread_affinity.hpp"

using namespace titan;

namespace {

TEST(ThreadAffinity, PinCurrentThreadReturnsFalseOnUnsupportedPlatform)
{
#ifdef __linux__
    (void)pinCurrentThreadToCpu(0);  // cpu 0 exists on any runner; just check no crash
#else
    EXPECT_FALSE(pinCurrentThreadToCpu(0));
#endif
}

TEST(ThreadAffinity, PinInvalidCpuReturnsFalse)
{
    EXPECT_FALSE(pinCurrentThreadToCpu(9999));
    EXPECT_FALSE(pinCurrentThreadToCpu(-1));
}

TEST(ThreadAffinity, PinThreadOverloadDoesNotCrash)
{
    std::thread worker([]() {});
    const bool result = pinThread(worker, 0);
#ifndef __linux__
    EXPECT_FALSE(result);
#else
    (void)result;  // may legitimately be true or false depending on the runner
#endif
    worker.join();
}

}  // namespace
