#include <gtest/gtest.h>

#include "titan/exchange/instrument_registry.hpp"

using namespace titan;

namespace {

// Same scripted sequence used by both DeterministicAcrossTwoRuns instances.
std::vector<Event> runScript() {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    registry.createInstrument("GOOG");

    registry.submitOrder("AAPL", Order{1, Side::Sell, 50, 100});
    registry.matchOrder("AAPL", Order{2, Side::Buy, 50, 60});
    registry.submitOrder("GOOG", Order{3, Side::Buy, 200, 10});
    registry.cancelOrder("GOOG", 3);
    registry.cancelReplace("AAPL", 1, Order{1, Side::Sell, 55, 40});
    registry.cancelOrder("AAPL", 999);

    return registry.eventLog();
}

}  // namespace

TEST(EventDispatch, SequenceNumbersAreMonotonic) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    registry.createInstrument("GOOG");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    registry.submitOrder("GOOG", Order{2, Side::Buy, 200, 10});
    registry.cancelOrder("AAPL", 1);

    const auto& log = registry.eventLog();
    ASSERT_EQ(log.size(), 3u);
    for (size_t i = 0; i < log.size(); ++i)
        EXPECT_EQ(log[i].sequenceNumber, i);
}

TEST(EventDispatch, TradeExecutedEmittedOnCross) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Sell, 50, 100});
    registry.matchOrder("AAPL", Order{2, Side::Buy, 50, 40});

    const auto& log = registry.eventLog();
    ASSERT_EQ(log.size(), 3u);
    ASSERT_TRUE(std::holds_alternative<TradeExecuted>(log[2].payload));

    const auto& trade = std::get<TradeExecuted>(log[2].payload);
    EXPECT_EQ(trade.symbol, "AAPL");
    EXPECT_EQ(trade.incomingOrderId, 2u);
    EXPECT_EQ(trade.restingOrderId, 1u);
    EXPECT_EQ(trade.price, 50u);
    EXPECT_EQ(trade.quantity, 40u);
}

TEST(EventDispatch, OrderCancelledEmittedOnCancel) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    registry.cancelOrder("AAPL", 1);

    const auto& log = registry.eventLog();
    ASSERT_EQ(log.size(), 2u);
    ASSERT_TRUE(std::holds_alternative<OrderCancelled>(log[1].payload));

    const auto& cancelled = std::get<OrderCancelled>(log[1].payload);
    EXPECT_EQ(cancelled.symbol, "AAPL");
    EXPECT_EQ(cancelled.id, 1u);
}

TEST(EventDispatch, OrderRejectedEmittedOnMatchOrderValidationFailure) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    const auto trades = registry.matchOrder("AAPL", Order{1, Side::Buy, 100, 10});

    EXPECT_TRUE(trades.empty());
    const auto& log = registry.eventLog();
    ASSERT_EQ(log.size(), 2u);
    ASSERT_TRUE(std::holds_alternative<OrderRejected>(log[1].payload));
    EXPECT_EQ(std::get<OrderRejected>(log[1].payload).reason, RejectReason::DuplicateOrderId);
}

TEST(EventDispatch, OrderRejectedEmittedOnUnknownSymbol) {
    InstrumentRegistry registry;

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});

    const auto& log = registry.eventLog();
    ASSERT_EQ(log.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<OrderRejected>(log[0].payload));
    EXPECT_EQ(std::get<OrderRejected>(log[0].payload).reason, RejectReason::UnknownSymbol);
}

TEST(EventDispatch, ClearEventLogEmptiesLog) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});

    ASSERT_FALSE(registry.eventLog().empty());
    registry.clearEventLog();
    EXPECT_TRUE(registry.eventLog().empty());
}

TEST(EventDispatch, DeterministicAcrossTwoRuns) {
    const std::vector<Event> firstRun = runScript();
    const std::vector<Event> secondRun = runScript();

    ASSERT_EQ(firstRun.size(), secondRun.size());
    for (size_t i = 0; i < firstRun.size(); ++i)
        EXPECT_EQ(firstRun[i], secondRun[i]);
}
