#include <gtest/gtest.h>

#include "reference/order_book.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

TEST(OrderLifecycle, RejectsZeroQuantity) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	const auto result = manager.addOrder(Order{1, Side::Buy, 100, 0});

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::ZeroQuantity);
	EXPECT_FALSE(manager.statusOf(1).has_value());
}

TEST(OrderLifecycle, RejectsZeroPrice) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	const auto result = manager.addOrder(Order{1, Side::Buy, 0, 10});

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::ZeroPrice);
	EXPECT_FALSE(manager.statusOf(1).has_value());
}

TEST(OrderLifecycle, RejectsDuplicateOrderId) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 100, 10}).accepted);

	const auto result = manager.addOrder(Order{1, Side::Sell, 200, 5});

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::DuplicateOrderId);
	// The original order is untouched by the rejected duplicate submission.
	EXPECT_EQ(manager.statusOf(1), OrderStatus::New);
}

TEST(OrderLifecycle, AddOrderStartsAtNew) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 100, 10}).accepted);

	EXPECT_EQ(manager.statusOf(1), OrderStatus::New);
}

TEST(OrderLifecycle, FullMatchTransitionsBothOrdersToFilled) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 100}).accepted);

	const auto trades = manager.matchOrder(Order{2, Side::Buy, 50, 100}).trades;

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Filled);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Filled);
}

TEST(OrderLifecycle, PartialFillThenFullFillReachesFilled) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Sell, 50, 100}).accepted);

	// First incoming BUY only takes 40 of the resting 100.
	const auto firstTrades = manager.matchOrder(Order{2, Side::Buy, 50, 40}).trades;
	ASSERT_EQ(firstTrades.size(), 1u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::PartiallyFilled);
	EXPECT_EQ(manager.statusOf(2), OrderStatus::Filled);

	// Second incoming BUY takes the remaining 60.
	const auto secondTrades = manager.matchOrder(Order{3, Side::Buy, 50, 60}).trades;
	ASSERT_EQ(secondTrades.size(), 1u);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Filled);
	EXPECT_EQ(manager.statusOf(3), OrderStatus::Filled);
}

TEST(OrderLifecycle, CancelSetsStatusCancelled) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	ASSERT_TRUE(manager.addOrder(Order{1, Side::Buy, 100, 10}).accepted);

	const auto result = manager.cancelOrder(1);

	EXPECT_TRUE(result.accepted);
	EXPECT_EQ(manager.statusOf(1), OrderStatus::Cancelled);
	EXPECT_EQ(matcher.getOrderTable().count(1), 0u);
}

TEST(OrderLifecycle, CancelUnknownIdRejectedByOrderManager) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	const auto result = manager.cancelOrder(999);

	EXPECT_FALSE(result.accepted);
	EXPECT_EQ(result.reason, RejectReason::UnknownOrder);
}

TEST(OrderLifecycle, AccountIdStoredAndRetrievable) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);

	Order order{1, Side::Buy, 100, 10};
	order.accountId = 42;

	ASSERT_TRUE(manager.addOrder(order).accepted);

	const auto found = manager.find(1);
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->accountId, 42u);
}
