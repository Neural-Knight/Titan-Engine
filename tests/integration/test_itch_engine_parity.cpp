#include <gtest/gtest.h>

#include <fstream>
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

TEST(ItchEngineParity, SingleSymbolSessionParity)
{
    InstrumentRegistry registry;
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

TEST(ItchEngineParity, TwoSymbolIndependentParity)
{
    InstrumentRegistry registry;
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

TEST(ItchEngineParity, DeterministicDoubleRun)
{
    const std::vector<ItchMessage> session = {
        makeDirectory(1, "AAPL"),
        makeAdd(1, 10, 'B', 100, 990000),
        makeAdd(1, 11, 'S', 80, 1010000),
        makeExecuted(1, 10, 30),
        makeDelete(1, 11),
    };

    InstrumentRegistry registryA;
    ItchBookBuilder builderA;
    ItchEngineAdapter adapterA(registryA);
    ParityChecker checkerA(builderA, adapterA);
    for (const auto& message : session)
        checkerA.process(message);

    InstrumentRegistry registryB;
    ItchBookBuilder builderB;
    ItchEngineAdapter adapterB(registryB);
    ParityChecker checkerB(builderB, adapterB);
    for (const auto& message : session)
        checkerB.process(message);

    EXPECT_TRUE(EventReplayer::eventLogsEqual(registryA.eventLog(), registryB.eventLog()));
}

TEST(ItchEngineParity, TradeVolumeMatches)
{
    InstrumentRegistry registry;
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

    uint64_t tradeEventVolume = 0;
    for (const Event& event : registry.eventLog())
        if (const auto* trade = std::get_if<TradeExecuted>(&event.payload))
            tradeEventVolume += trade->quantity;
    EXPECT_EQ(tradeEventVolume, checker.report().engineTradeVolume);
}

TEST(ItchEngineParity, FixtureFileParity)
{
    InstrumentRegistry registry;
    EventReplayer replayer(registry);
    const ReplayResult result = replayer.replayFile(std::string(TITAN_FIXTURES_DIR) + "/itch/sample_session.itch");

    EXPECT_EQ(result.messagesSkipped, 0u);
    EXPECT_EQ(result.messagesDecoded, 5u);
    EXPECT_EQ(result.parity.mismatchCount, 0u);
}

TEST(ItchEngineParity, UnknownLocateIgnoredByBothFeedAndEngine)
{
    InstrumentRegistry registry;
    ItchBookBuilder builder;
    ItchEngineAdapter adapter(registry);
    ParityChecker checker(builder, adapter);

    checker.process(makeAdd(9, 100, 'B', 50, 1000000));

    EXPECT_FALSE(builder.snapshot(9, 5).has_value());
    EXPECT_FALSE(adapter.symbolForLocate(9).has_value());
    EXPECT_EQ(checker.report().mismatchCount, 0u);
}
