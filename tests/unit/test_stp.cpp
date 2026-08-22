#include <gtest/gtest.h>

#include "reference/order_book.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

// CancelIncoming: a same-account block cancels the incoming GTC remainder
// (it would still cross), rather than resting it against its own order.
TEST(Stp, SameAccountCrossIsBlocked) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order resting{1, Side::Sell, 50, 100};
	resting.accountId = 7;
	ASSERT_TRUE(manager.addOrder(resting).accepted);

	Order incoming{2, Side::Buy, 50, 100};
	incoming.accountId = 7;

	const auto trades = manager.matchOrder(incoming).trades;

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(matcher.getAsks().at(50).front().quantity, 100u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::New);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Cancelled);
	EXPECT_EQ(matcher.getBids().count(50), 0u);
}

TEST(Stp, DifferentAccountsCrossNormally) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order resting{1, Side::Sell, 50, 100};
	resting.accountId = 7;
	ASSERT_TRUE(manager.addOrder(resting).accepted);

	Order incoming{2, Side::Buy, 50, 100};
	incoming.accountId = 8;

	const auto trades = manager.matchOrder(incoming).trades;

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Filled);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Filled);
}

TEST(Stp, MarketOrderAgainstOwnRestingLimitDoesNotSelfTrade) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order resting{1, Side::Sell, 50, 100};
	resting.accountId = 7;
	ASSERT_TRUE(manager.addOrder(resting).accepted);

	const auto trades = manager.matchOrder(Order{2, Side::Buy, 0, 100, 7, OrderType::Market}).trades;

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(matcher.getAsks().at(50).front().quantity, 100u);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Cancelled);
	EXPECT_EQ(matcher.getOrderTable().count(2), 0u);
}

// CancelIncoming stops entirely at the self-collision -- it does not skip
// past it to reach a different account's liquidity resting behind it.
TEST(Stp, OwnOrderAtBestBlocksReachingOtherAccountBehindIt) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order ownBest{1, Side::Sell, 50, 100};
	ownBest.accountId = 7;
	ASSERT_TRUE(manager.addOrder(ownBest).accepted);

	Order otherWorse{2, Side::Sell, 51, 100};
	otherWorse.accountId = 8;
	ASSERT_TRUE(manager.addOrder(otherWorse).accepted);

	const auto trades = manager.matchOrder(Order{3, Side::Buy, 0, 150, 7, OrderType::Market}).trades;

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(manager.statusOf(3), OrderStatus::Cancelled);
	EXPECT_EQ(matcher.getAsks().at(50).front().quantity, 100u);
	EXPECT_EQ(matcher.getAsks().at(51).front().quantity, 100u);
}

// Other account at best fills first; the self-owned order behind it blocks the rest.
TEST(Stp, OtherAccountAtBestFillsBeforeOwnOrderBlocksTheRest) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order otherBest{1, Side::Sell, 50, 40};
	otherBest.accountId = 8;
	ASSERT_TRUE(manager.addOrder(otherBest).accepted);

	Order ownWorse{2, Side::Sell, 51, 100};
	ownWorse.accountId = 7;
	ASSERT_TRUE(manager.addOrder(ownWorse).accepted);

	const auto trades = manager.matchOrder(Order{3, Side::Buy, 0, 150, 7, OrderType::Market}).trades;

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].quantity, 40u);
	EXPECT_EQ(manager.statusOf(3), OrderStatus::Filled);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Filled);
	EXPECT_EQ(matcher.getAsks().at(51).front().quantity, 100u);
}
