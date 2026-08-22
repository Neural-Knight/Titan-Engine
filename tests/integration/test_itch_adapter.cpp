#include <gtest/gtest.h>

#include "itch_test_helpers.hpp"
#include "titan/exchange/instrument_registry.hpp"
#include "titan/replay/itch_adapter.hpp"

using namespace titan;
using namespace titan::test;

TEST(ItchEngineAdapter, StockDirectoryCreatesInstrument)
{
    InstrumentRegistry registry;
    ItchEngineAdapter adapter(registry);

    EXPECT_TRUE(adapter.apply(makeDirectory(1, "AAPL")));
    EXPECT_TRUE(registry.hasInstrument("AAPL"));
    ASSERT_TRUE(adapter.symbolForLocate(1).has_value());
    EXPECT_EQ(*adapter.symbolForLocate(1), "AAPL");
}

TEST(ItchEngineAdapter, AddOrderIgnoredWithoutStockDirectory)
{
    InstrumentRegistry registry;
    ItchEngineAdapter adapter(registry);

    EXPECT_FALSE(adapter.apply(makeAdd(1, 100, 'B', 50, 1000000)));
    EXPECT_FALSE(registry.hasInstrument("AAPL"));
}

TEST(ItchEngineAdapter, AddOrderRestsOnEngineBook)
{
    InstrumentRegistry registry;
    ItchEngineAdapter adapter(registry);
    adapter.apply(makeDirectory(1, "AAPL"));

    EXPECT_TRUE(adapter.apply(makeAdd(1, 100, 'B', 50, 1000000)));

    const auto snap = adapter.snapshot(1, 5);
    ASSERT_EQ(snap.bids.size(), 1u);
    EXPECT_EQ(snap.bids[0], (PriceLevel{1000000, 50}));
}

TEST(ItchEngineAdapter, ExecuteReducesRestingQty)
{
    InstrumentRegistry registry;
    ItchEngineAdapter adapter(registry);
    adapter.apply(makeDirectory(1, "AAPL"));
    adapter.apply(makeAdd(1, 200, 'S', 100, 500000));

    EXPECT_TRUE(adapter.apply(makeExecuted(1, 200, 40)));

    const auto snap = adapter.snapshot(1, 5);
    ASSERT_EQ(snap.asks.size(), 1u);
    EXPECT_EQ(snap.asks[0], (PriceLevel{500000, 60}));
    EXPECT_EQ(adapter.tradeVolume(), 40u);
}

TEST(ItchEngineAdapter, ExecuteWithPriceUsesGivenExecutionPrice)
{
    InstrumentRegistry registry;
    ItchEngineAdapter adapter(registry);
    adapter.apply(makeDirectory(1, "AAPL"));
    adapter.apply(makeAdd(1, 300, 'B', 100, 500000));

    EXPECT_TRUE(adapter.apply(makeExecutedWithPrice(1, 300, 30, 495000)));

    const auto snap = adapter.snapshot(1, 5);
    ASSERT_EQ(snap.bids.size(), 1u);
    EXPECT_EQ(snap.bids[0], (PriceLevel{500000, 70}));
}

TEST(ItchEngineAdapter, CancelReducesQty)
{
    InstrumentRegistry registry;
    ItchEngineAdapter adapter(registry);
    adapter.apply(makeDirectory(1, "AAPL"));
    adapter.apply(makeAdd(1, 400, 'B', 100, 500000));

    EXPECT_TRUE(adapter.apply(makeCancel(1, 400, 30)));

    const auto snap = adapter.snapshot(1, 5);
    ASSERT_EQ(snap.bids.size(), 1u);
    EXPECT_EQ(snap.bids[0], (PriceLevel{500000, 70}));
}

TEST(ItchEngineAdapter, DeleteRemovesOrder)
{
    InstrumentRegistry registry;
    ItchEngineAdapter adapter(registry);
    adapter.apply(makeDirectory(1, "AAPL"));
    adapter.apply(makeAdd(1, 500, 'S', 50, 600000));

    EXPECT_TRUE(adapter.apply(makeDelete(1, 500)));

    const auto snap = adapter.snapshot(1, 5);
    EXPECT_TRUE(snap.asks.empty());
}

TEST(ItchEngineAdapter, ReplaceMovesPrice)
{
    InstrumentRegistry registry;
    ItchEngineAdapter adapter(registry);
    adapter.apply(makeDirectory(1, "AAPL"));
    adapter.apply(makeAdd(1, 600, 'B', 20, 700000));

    EXPECT_TRUE(adapter.apply(makeReplace(1, 600, 601, 20, 705000)));

    {
        const auto snap = adapter.snapshot(1, 5);
        ASSERT_EQ(snap.bids.size(), 1u);
        EXPECT_EQ(snap.bids[0], (PriceLevel{705000, 20}));
    }

    // New ref (601) must resolve to the same resting order for later messages.
    EXPECT_TRUE(adapter.apply(makeCancel(1, 601, 5)));

    {
        const auto snap = adapter.snapshot(1, 5);
        ASSERT_EQ(snap.bids.size(), 1u);
        EXPECT_EQ(snap.bids[0], (PriceLevel{705000, 15}));
    }
}
