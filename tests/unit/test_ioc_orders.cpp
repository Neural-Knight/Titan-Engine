#include <gtest/gtest.h>

#include "reference/order_book.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

TEST(IocOrders, PartialFillCancelsRemainder) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 100}).accepted);

	const auto trades = manager.matchOrder(Order{2, Side::Buy, 50, 150, 0, OrderType::Limit, TimeInForce::IOC}).trades;

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Filled);
	EXPECT_EQ(matcher.getOrderTable().count(2), 0u);
	EXPECT_TRUE(matcher.getBids().empty());
}

TEST(IocOrders, FullFillStatusFilled) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 100}).accepted);

	const auto trades = manager.matchOrder(Order{2, Side::Buy, 50, 100, 0, OrderType::Limit, TimeInForce::IOC}).trades;

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Filled);
	EXPECT_TRUE(matcher.getBids().empty());
	EXPECT_EQ(matcher.getOrderTable().count(2), 0u);
}

TEST(IocOrders, EmptyBookCancelled) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	const auto trades = manager.matchOrder(Order{1, Side::Buy, 50, 100, 0, OrderType::Limit, TimeInForce::IOC}).trades;

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Cancelled);
	EXPECT_TRUE(matcher.getBids().empty());
	EXPECT_EQ(matcher.getOrderTable().count(1), 0u);
}

TEST(IocOrders, NoCrossingLiquidityCancelled) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 51, 100}).accepted);

	// IOC buy at 50 can't cross the resting ask at 51: no trade, cancelled.
	const auto trades = manager.matchOrder(Order{2, Side::Buy, 50, 100, 0, OrderType::Limit, TimeInForce::IOC}).trades;

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Cancelled);
	EXPECT_EQ(matcher.getOrderTable().count(2), 0u);
	// The resting sell is untouched.
	EXPECT_EQ(matcher.getAsks().count(51), 1u);
}

// Regression: an ordinary GTC limit that doesn't cross must still rest,
// same as before Market/IOC were introduced.
TEST(IocOrders, GtcLimitStillRestsOnNoCross) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 51, 100}).accepted);

	const auto trades = manager.matchOrder(Order{2, Side::Buy, 50, 100}).trades;

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(manager.statusOf(2), OrderStatus::New);
	EXPECT_EQ(matcher.getBids().count(50), 1u);
}
