#include <gtest/gtest.h>

#include "reference/order_book.hpp"
#include "titan/book/invariants.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

TEST(OrderEntryRouting, AddOrderCrossingLimitAutoMatches) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 100}).accepted);

	const auto result = manager.addOrder(Order{2, Side::Buy, 50, 100});

	EXPECT_TRUE(result.accepted);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Filled);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Filled);
	EXPECT_TRUE(matcher.getBids().empty());
	EXPECT_TRUE(matcher.getAsks().empty());
}

TEST(OrderEntryRouting, AddOrderNonCrossingStillRests) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 51, 100}).accepted);

	const auto result = manager.addOrder(Order{2, Side::Buy, 50, 100});

	EXPECT_TRUE(result.accepted);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::New);
	EXPECT_EQ(matcher.getBids().at(50).front().id, 2u);
	EXPECT_EQ(matcher.getAsks().at(51).front().id, 1u);
}

TEST(OrderEntryRouting, AddOrderCrossingLimitPartialFillRestsRemainderNonCrossed) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 40}).accepted);

	const auto result = manager.addOrder(Order{2, Side::Buy, 50, 100});

	EXPECT_TRUE(result.accepted);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Filled);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::PartiallyFilled);
	EXPECT_TRUE(matcher.getAsks().empty());
	ASSERT_EQ(matcher.getBids().count(50), 1u);
	EXPECT_EQ(matcher.getBids().at(50).front().quantity, 60u);
}

TEST(OrderEntryRouting, CancelReplaceRejectsCrossingPrice) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 40, 10}).accepted);
	ASSERT_TRUE(manager.addOrder(Order{2, Side::Sell, 50, 10}).accepted);

	// Replacing order 1's price to 50 would cross the resting ask at 50.
	const auto result = manager.cancelReplace(1, Order{1, Side::Buy, 50, 10});

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::ReplaceWouldCross);
	// Old order untouched: still resting at its original price.
	EXPECT_EQ(matcher.getBids().at(40).front().id, 1u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::New);
}

TEST(OrderEntryRouting, OrderManagerNeverCrossedAtRest) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 55, 100}).accepted);
	ASSERT_TRUE(manager.addOrder(Order{2, Side::Buy, 50, 100}).accepted);
	ASSERT_TRUE(manager.addOrder(Order{3, Side::Sell, 60, 50}).accepted);
	// Crosses: trades 30 against order 2, leaving 70 resting at 50.
	ASSERT_TRUE(manager.addOrder(Order{4, Side::Sell, 45, 30}).accepted);
	manager.matchOrder(Order{5, Side::Sell, 0, 70, 0, OrderType::Market});
	ASSERT_TRUE(manager.cancelOrder(3).accepted);

	const auto violations = checkOrderManagerInvariants(manager, matcher);

	EXPECT_TRUE(violations.empty());
	if (!matcher.getBids().empty() && !matcher.getAsks().empty())
		EXPECT_LT(matcher.bestBid(), matcher.bestAsk());
}
