#include <gtest/gtest.h>

#include "reference/order_book.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

namespace {

std::vector<OrderId> idsAtLevel(const PriceLevels &levels, Price price) {
	auto levelIt = levels.find(price);
	if (levelIt == levels.end()) {
		return {};
	}

	std::vector<OrderId> ids;
	for (const auto &order : levelIt->second) {
		ids.push_back(order.id);
	}
	return ids;
}

}  // namespace

TEST(CancelReplace, PriceChangeGoesToBackOfNewLevel) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 100, 10}).accepted);
	// Two orders already resting at the new price, ahead of the replacement.
	ASSERT_TRUE(manager.addOrder(Order{2, Side::Buy, 101, 5}).accepted);
	ASSERT_TRUE(manager.addOrder(Order{3, Side::Buy, 101, 5}).accepted);

	const auto result = manager.cancelReplace(1, Order{1, Side::Buy, 101, 10});

	EXPECT_TRUE(result.accepted);
	EXPECT_EQ(idsAtLevel(matcher.getBids(), 101), (std::vector<OrderId>{2, 3, 1}));
	EXPECT_EQ(matcher.getBids().count(100), 0u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::New);
}

TEST(CancelReplace, QuantityDecreaseAtSamePricePreservesQueuePosition) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 100}).accepted);
	ASSERT_TRUE(manager.addOrder(Order{2, Side::Sell, 50, 100}).accepted);
	// Added after the original order #1 -- must remain behind it post-replace.
	ASSERT_TRUE(manager.addOrder(Order{3, Side::Sell, 50, 100}).accepted);

	const auto result = manager.cancelReplace(1, Order{1, Side::Sell, 50, 40});

	EXPECT_TRUE(result.accepted);
	// #1 keeps its original front-of-queue slot, just with less quantity.
	EXPECT_EQ(idsAtLevel(matcher.getAsks(), 50), (std::vector<OrderId>{1, 2, 3}));

	const auto order1 = matcher.getAsks().at(50).front();
	EXPECT_EQ(order1.id, 1u);
	EXPECT_EQ(order1.quantity, 40u);
}

TEST(CancelReplace, QuantityIncreaseAtSamePriceLosesPriority) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 40}).accepted);
	ASSERT_TRUE(manager.addOrder(Order{2, Side::Sell, 50, 100}).accepted);

	const auto result = manager.cancelReplace(1, Order{1, Side::Sell, 50, 90});

	EXPECT_TRUE(result.accepted);
	// #1 increased size at the same price: goes to the back, behind #2.
	EXPECT_EQ(idsAtLevel(matcher.getAsks(), 50), (std::vector<OrderId>{2, 1}));
}

TEST(CancelReplace, RejectedOnNonRestingOldOrder) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 100, 10}).accepted);
	ASSERT_TRUE(manager.cancelOrder(1).accepted);

	const auto result = manager.cancelReplace(1, Order{1, Side::Buy, 105, 10});

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::OrderNotResting);
}

TEST(CancelReplace, RejectedOnUnknownOldOrder) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	const auto result = manager.cancelReplace(999, Order{999, Side::Buy, 105, 10});

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::OrderNotResting);
}

TEST(CancelReplace, InvalidReplacementRejectedAndOldOrderUntouched) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 100, 10}).accepted);

	const auto result = manager.cancelReplace(1, Order{1, Side::Buy, 100, 0});

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::ZeroQuantity);
	// The original order is untouched: still resting, original quantity.
	EXPECT_EQ(matcher.getBids().at(100).front().quantity, 10u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::New);
}

TEST(CancelReplace, MismatchedNewIdRejected) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 100, 10}).accepted);

	const auto result = manager.cancelReplace(1, Order{2, Side::Buy, 100, 10});

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::InvalidReplace);
	EXPECT_EQ(matcher.getBids().at(100).front().id, 1u);
}
