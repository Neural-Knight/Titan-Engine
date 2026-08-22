#include <gtest/gtest.h>

#include "titan/exchange/instrument_registry.hpp"

using namespace titan;

TEST(BookSnapshots, EmptyBookHasEmptyLevels) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    const BookSnapshot snap = registry.snapshot("AAPL", 5);
    EXPECT_TRUE(snap.bids.empty());
    EXPECT_TRUE(snap.asks.empty());
}

TEST(BookSnapshots, UnknownSymbolProducesEmptySnapshot) {
    InstrumentRegistry registry;

    const BookSnapshot snap = registry.snapshot("AAPL", 5);
    EXPECT_TRUE(snap.bids.empty());
    EXPECT_TRUE(snap.asks.empty());
}

TEST(BookSnapshots, TopLevelsAggregatedAndSortedBestFirst) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    registry.submitOrder("AAPL", Order{2, Side::Buy, 102, 20});
    registry.submitOrder("AAPL", Order{3, Side::Buy, 101, 5});
    registry.submitOrder("AAPL", Order{4, Side::Buy, 101, 7});
    registry.submitOrder("AAPL", Order{5, Side::Buy, 99, 3});

    // Priced above every bid so none of these cross the resting buys.
    registry.submitOrder("AAPL", Order{6, Side::Sell, 110, 3});
    registry.submitOrder("AAPL", Order{7, Side::Sell, 110, 2});
    registry.submitOrder("AAPL", Order{8, Side::Sell, 111, 5});
    registry.submitOrder("AAPL", Order{9, Side::Sell, 112, 1});

    const BookSnapshot snap = registry.snapshot("AAPL", 3);

    ASSERT_EQ(snap.bids.size(), 3u);
    EXPECT_EQ(snap.bids[0], (PriceLevel{102, 20}));
    EXPECT_EQ(snap.bids[1], (PriceLevel{101, 12}));
    EXPECT_EQ(snap.bids[2], (PriceLevel{100, 10}));

    ASSERT_EQ(snap.asks.size(), 3u);
    EXPECT_EQ(snap.asks[0], (PriceLevel{110, 5}));
    EXPECT_EQ(snap.asks[1], (PriceLevel{111, 5}));
    EXPECT_EQ(snap.asks[2], (PriceLevel{112, 1}));
}

TEST(BookSnapshots, CancelRemovesLevel) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    ASSERT_EQ(registry.snapshot("AAPL", 5).bids.size(), 1u);

    registry.cancelOrder("AAPL", 1);
    EXPECT_TRUE(registry.snapshot("AAPL", 5).bids.empty());
}

TEST(BookSnapshots, TwoSymbolsHaveIndependentSnapshots) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    registry.createInstrument("GOOG");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    registry.submitOrder("GOOG", Order{2, Side::Buy, 200, 20});

    const BookSnapshot aapl = registry.snapshot("AAPL", 5);
    const BookSnapshot goog = registry.snapshot("GOOG", 5);

    ASSERT_EQ(aapl.bids.size(), 1u);
    EXPECT_EQ(aapl.bids[0], (PriceLevel{100, 10}));

    ASSERT_EQ(goog.bids.size(), 1u);
    EXPECT_EQ(goog.bids[0], (PriceLevel{200, 20}));
}

TEST(BookSnapshots, SequenceNumberMonotonicPerSymbol) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    EXPECT_EQ(registry.snapshot("AAPL", 5).sequenceNumber, 0u);
    EXPECT_EQ(registry.snapshot("AAPL", 5).sequenceNumber, 1u);
    EXPECT_EQ(registry.snapshot("AAPL", 5).sequenceNumber, 2u);
}
