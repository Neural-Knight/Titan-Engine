#include <gtest/gtest.h>

#include "reference/order_book.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

TEST(MarketOrders, BuySweepsMultipleLevels) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 100}).accepted);
	ASSERT_TRUE(manager.addOrder(Order{2, Side::Sell, 51, 100}).accepted);

	const auto trades = manager.matchOrder(Order{3, Side::Buy, 0, 150, 0, OrderType::Market});

	ASSERT_EQ(trades.size(), 2u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].price, 50u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_EQ(trades[1].restingOrderId, 2u);
	EXPECT_EQ(trades[1].price, 51u);
	EXPECT_EQ(trades[1].quantity, 50u);

	EXPECT_EQ(manager.statusOf(3), OrderStatus::Filled);
	// The market buy never rests, at its synthesized extreme price or otherwise.
	EXPECT_TRUE(matcher.getBids().empty());
	EXPECT_EQ(matcher.getOrderTable().count(3), 0u);
}

TEST(MarketOrders, BuyPartialFillDiscardsRemainder) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 100}).accepted);

	// Market buy for 150 when only 100 is available: takes what it can,
	// the remaining 50 is discarded, not rested.
	const auto trades = manager.matchOrder(Order{2, Side::Buy, 0, 150, 0, OrderType::Market});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Filled);
	EXPECT_TRUE(matcher.getBids().empty());
	EXPECT_TRUE(matcher.getAsks().empty());
	EXPECT_EQ(matcher.getOrderTable().count(2), 0u);
}

TEST(MarketOrders, BuyOnEmptyBookProducesNoTrades) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	const auto trades = manager.matchOrder(Order{1, Side::Buy, 0, 100, 0, OrderType::Market});

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Cancelled);
	EXPECT_TRUE(matcher.getBids().empty());
	EXPECT_EQ(matcher.getOrderTable().count(1), 0u);
}

TEST(MarketOrders, SellSweepsMultipleLevels) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 51, 100}).accepted);
	ASSERT_TRUE(manager.addOrder(Order{2, Side::Buy, 50, 100}).accepted);

	const auto trades = manager.matchOrder(Order{3, Side::Sell, 0, 150, 0, OrderType::Market});

	ASSERT_EQ(trades.size(), 2u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].price, 51u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_EQ(trades[1].restingOrderId, 2u);
	EXPECT_EQ(trades[1].price, 50u);
	EXPECT_EQ(trades[1].quantity, 50u);

	EXPECT_EQ(manager.statusOf(3), OrderStatus::Filled);
	EXPECT_TRUE(matcher.getAsks().empty());
	EXPECT_EQ(matcher.getOrderTable().count(3), 0u);
}

TEST(MarketOrders, SellOnEmptyBookProducesNoTrades) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	const auto trades = manager.matchOrder(Order{1, Side::Sell, 0, 100, 0, OrderType::Market});

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Cancelled);
	EXPECT_TRUE(matcher.getAsks().empty());
	EXPECT_EQ(matcher.getOrderTable().count(1), 0u);
}

TEST(MarketOrders, NeverEndsUpInMatcherOrderTableEvenOnPartialFill) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 40}).accepted);

	manager.matchOrder(Order{2, Side::Buy, 0, 100, 0, OrderType::Market});

	EXPECT_EQ(matcher.getOrderTable().count(2), 0u);
}
