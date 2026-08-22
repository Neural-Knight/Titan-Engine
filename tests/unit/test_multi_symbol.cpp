#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "titan/exchange/instrument_registry.hpp"

using namespace titan;

TEST(InstrumentRegistry, UnknownSymbolRejectsAllRoutes) {
    InstrumentRegistry registry;

    const AcceptResult submitResult = registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    EXPECT_FALSE(submitResult.accepted);
    EXPECT_EQ(submitResult.reason, RejectReason::UnknownSymbol);

    const AcceptResult cancelResult = registry.cancelOrder("AAPL", 1);
    EXPECT_FALSE(cancelResult.accepted);
    EXPECT_EQ(cancelResult.reason, RejectReason::UnknownSymbol);

    const auto trades = registry.matchOrder("AAPL", Order{2, Side::Buy, 100, 10});
    EXPECT_TRUE(trades.empty());

    const AcceptResult replaceResult = registry.cancelReplace("AAPL", 1, Order{1, Side::Buy, 105, 10});
    EXPECT_FALSE(replaceResult.accepted);
    EXPECT_EQ(replaceResult.reason, RejectReason::UnknownSymbol);
}

TEST(InstrumentRegistry, CreateInstrumentIsIdempotent) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    registry.createInstrument("AAPL");

    EXPECT_TRUE(registry.hasInstrument("AAPL"));
    EXPECT_FALSE(registry.hasInstrument("GOOG"));
}

TEST(InstrumentRegistry, CrossingOrdersDoNotAffectOtherSymbols) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    registry.createInstrument("GOOG");

    registry.submitOrder("AAPL", Order{1, Side::Sell, 50, 100});
    const auto aaplTrades = registry.matchOrder("AAPL", Order{2, Side::Buy, 50, 100});
    ASSERT_EQ(aaplTrades.size(), 1u);

    const auto googTrades = registry.matchOrder("GOOG", Order{3, Side::Buy, 50, 100});
    EXPECT_TRUE(googTrades.empty());
}

TEST(InstrumentRegistry, SameOrderIdIndependentAcrossSymbols) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    registry.createInstrument("GOOG");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    registry.submitOrder("GOOG", Order{1, Side::Buy, 200, 20});

    const AcceptResult cancelInAapl = registry.cancelOrder("AAPL", 1);
    EXPECT_TRUE(cancelInAapl.accepted);

    // Still resting in GOOG: its own OrderManager instance, untouched by AAPL's cancel.
    const AcceptResult cancelInGoog = registry.cancelOrder("GOOG", 1);
    EXPECT_TRUE(cancelInGoog.accepted);

    const AcceptResult secondCancelInGoog = registry.cancelOrder("GOOG", 1);
    EXPECT_FALSE(secondCancelInGoog.accepted);
}

TEST(InstrumentRegistry, TenSymbolsStayIndependent) {
    InstrumentRegistry registry;
    std::vector<Symbol> symbols;
    for (int i = 0; i < 10; ++i)
        symbols.push_back("SYM" + std::to_string(i));

    for (const Symbol& symbol : symbols)
        registry.createInstrument(symbol);

    OrderId nextId = 1;
    for (const Symbol& symbol : symbols) {
        registry.submitOrder(symbol, Order{nextId++, Side::Sell, 50, 100});
        const auto trades = registry.matchOrder(symbol, Order{nextId++, Side::Buy, 50, 100});
        ASSERT_EQ(trades.size(), 1u);
    }

    for (const Symbol& symbol : symbols) {
        const auto trades = registry.matchOrder(symbol, Order{nextId++, Side::Buy, 50, 100});
        EXPECT_TRUE(trades.empty());
    }
}
