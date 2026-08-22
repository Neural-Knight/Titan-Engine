#include <gtest/gtest.h>

#include "reference/order_book.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

TEST(Stp, SameAccountCrossIsBlocked) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order resting{1, Side::Sell, 50, 100};
	resting.accountId = 7;
	ASSERT_TRUE(manager.addOrder(resting).accepted);

	Order incoming{2, Side::Buy, 50, 100};
	incoming.accountId = 7;

	const auto trades = manager.matchOrder(incoming);

	EXPECT_TRUE(trades.empty());
	// Blocked resting order is completely untouched.
	EXPECT_EQ(matcher.getAsks().at(50).front().quantity, 100u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::New);
	// Incoming GTC limit rests instead of trading against itself.
	EXPECT_EQ(manager.statusOf(2), OrderStatus::New);
	EXPECT_EQ(matcher.getBids().at(50).front().id, 2u);
}

TEST(Stp, DifferentAccountsCrossNormally) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order resting{1, Side::Sell, 50, 100};
	resting.accountId = 7;
	ASSERT_TRUE(manager.addOrder(resting).accepted);

	Order incoming{2, Side::Buy, 50, 100};
	incoming.accountId = 8;

	const auto trades = manager.matchOrder(incoming);

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

	// Market buy from the SAME account: must not trade against its own resting sell.
	const auto trades = manager.matchOrder(Order{2, Side::Buy, 0, 100, 7, OrderType::Market});

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(matcher.getAsks().at(50).front().quantity, 100u);
	// Market never rests: no fill happened, so it's Cancelled.
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Cancelled);
	EXPECT_EQ(matcher.getOrderTable().count(2), 0u);
}

// Own order rests at the best price; a different account's order rests
// behind it at a worse price. STP (CancelIncoming) stops matching entirely
// at the first self-collision -- it does not skip past its own order to
// reach the other account's liquidity behind it.
TEST(Stp, OwnOrderAtBestBlocksReachingOtherAccountBehindIt) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order ownBest{1, Side::Sell, 50, 100};
	ownBest.accountId = 7;
	ASSERT_TRUE(manager.addOrder(ownBest).accepted);

	Order otherWorse{2, Side::Sell, 51, 100};
	otherWorse.accountId = 8;
	ASSERT_TRUE(manager.addOrder(otherWorse).accepted);

	// Market buy sweeps everything price-wise, but account 7's own order
	// sits at the front of the book.
	const auto trades = manager.matchOrder(Order{3, Side::Buy, 0, 150, 7, OrderType::Market});

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(manager.statusOf(3), OrderStatus::Cancelled);
	// Neither resting order was touched -- including the other account's,
	// which was never reached.
	EXPECT_EQ(matcher.getAsks().at(50).front().quantity, 100u);
	EXPECT_EQ(matcher.getAsks().at(51).front().quantity, 100u);
}

// Own order rests behind another account's order at the best price: the
// other account's liquidity is reached and traded first, and only the
// self-owned order behind it blocks further matching.
TEST(Stp, OtherAccountAtBestFillsBeforeOwnOrderBlocksTheRest) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order otherBest{1, Side::Sell, 50, 40};
	otherBest.accountId = 8;
	ASSERT_TRUE(manager.addOrder(otherBest).accepted);

	Order ownWorse{2, Side::Sell, 51, 100};
	ownWorse.accountId = 7;
	ASSERT_TRUE(manager.addOrder(ownWorse).accepted);

	const auto trades = manager.matchOrder(Order{3, Side::Buy, 0, 150, 7, OrderType::Market});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].quantity, 40u);
	// Filled what it could from the other account, discarded the rest
	// because the only remaining liquidity was self-owned.
	EXPECT_EQ(manager.statusOf(3), OrderStatus::Filled);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Filled);
	EXPECT_EQ(matcher.getAsks().at(51).front().quantity, 100u);
}
