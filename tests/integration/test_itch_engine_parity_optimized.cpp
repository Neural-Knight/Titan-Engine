#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "itch_test_helpers.hpp"
#include "titan/exchange/instrument_registry.hpp"
#include "titan/feed/itch/book_builder.hpp"
#include "titan/replay/event_replayer.hpp"
#include "titan/replay/itch_adapter.hpp"
#include "titan/replay/parity_checker.hpp"

using namespace titan;
using namespace titan::test;

// Same sessions as test_itch_engine_parity.cpp, backed by OptimizedMatcher
// instead of the default ReferenceMatcher -- proves ITCH parity holds either way.

TEST(ItchEngineParityOptimized, SingleSymbolSessionParity)
{
    InstrumentRegistry registry(MatcherBackend::Optimized);
    ItchBookBuilder builder;
    ItchEngineAdapter adapter(registry);
    ParityChecker checker(builder, adapter);

    const std::vector<ItchMessage> session = {
        makeDirectory(1, "AAPL"),
        makeAdd(1, 10, 'B', 100, 990000),
        makeAdd(1, 11, 'S', 80, 1010000),
        makeExecuted(1, 10, 30),
        makeCancel(1, 11, 20),
        makeReplace(1, 10, 12, 70, 995000),
        makeDelete(1, 11),
    };

    for (const auto& message : session)
        checker.process(message);

    EXPECT_EQ(checker.report().mismatchCount, 0u);
    EXPECT_GT(checker.report().checkpointsCompared, 0u);
}

TEST(ItchEngineParityOptimized, TwoSymbolIndependentParity)
{
    InstrumentRegistry registry(MatcherBackend::Optimized);
    ItchBookBuilder builder;
    ItchEngineAdapter adapter(registry);
    ParityChecker checker(builder, adapter);

    const std::vector<ItchMessage> session = {
        makeDirectory(1, "AAPL"),
        makeDirectory(2, "MSFT"),
        makeAdd(1, 1, 'B', 10, 1000000),
        makeAdd(2, 2, 'S', 20, 2000000),
        makeExecuted(1, 1, 4),
        makeCancel(2, 2, 5),
    };

    for (const auto& message : session)
        checker.process(message);

    EXPECT_EQ(checker.report().mismatchCount, 0u);
}

TEST(ItchEngineParityOptimized, TradeVolumeMatches)
{
    InstrumentRegistry registry(MatcherBackend::Optimized);
    ItchBookBuilder builder;
    ItchEngineAdapter adapter(registry);
    ParityChecker checker(builder, adapter);

    const std::vector<ItchMessage> session = {
        makeDirectory(1, "AAPL"),
        makeAdd(1, 10, 'B', 100, 990000),
        makeExecuted(1, 10, 30),
        makeExecuted(1, 10, 20),
    };
    for (const auto& message : session)
        checker.process(message);

    EXPECT_EQ(checker.report().feedTradeVolume, 50u);
    EXPECT_EQ(checker.report().engineTradeVolume, 50u);
}

TEST(ItchEngineParityOptimized, FixtureFileParity)
{
    InstrumentRegistry registry(MatcherBackend::Optimized);
    EventReplayer replayer(registry);
    const ReplayResult result = replayer.replayFile(std::string(TITAN_FIXTURES_DIR) + "/itch/sample_session.itch");

    EXPECT_EQ(result.messagesSkipped, 0u);
    EXPECT_EQ(result.messagesDecoded, 5u);
    EXPECT_EQ(result.parity.mismatchCount, 0u);
}
