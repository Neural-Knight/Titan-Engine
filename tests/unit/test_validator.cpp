#include <gtest/gtest.h>

#include <unordered_set>

#include "titan/exchange/validator.hpp"

using namespace titan;

TEST(Validator, RejectsZeroQuantity) {
	const Order order{1, Side::Buy, 100, 0};
	const std::unordered_set<OrderId> existingIds;

	EXPECT_EQ(validateNewOrder(order, existingIds), RejectReason::ZeroQuantity);
}

TEST(Validator, RejectsZeroPriceForLimitOrder) {
	const Order order{1, Side::Buy, 0, 10};
	const std::unordered_set<OrderId> existingIds;

	EXPECT_EQ(validateNewOrder(order, existingIds), RejectReason::ZeroPrice);
}

TEST(Validator, RejectsDuplicateOrderId) {
	const Order order{1, Side::Buy, 100, 10};
	const std::unordered_set<OrderId> existingIds{1};

	EXPECT_EQ(validateNewOrder(order, existingIds), RejectReason::DuplicateOrderId);
}

TEST(Validator, AcceptsWellFormedNewOrder) {
	const Order order{1, Side::Buy, 100, 10};
	const std::unordered_set<OrderId> existingIds;

	EXPECT_EQ(validateNewOrder(order, existingIds), RejectReason::None);
}

TEST(Validator, RejectsCancelOfUnknownId) {
	const std::unordered_set<OrderId> activeIds{1, 2};

	EXPECT_EQ(validateCancel(999, activeIds), RejectReason::UnknownOrder);
}

TEST(Validator, AcceptsCancelOfKnownId) {
	const std::unordered_set<OrderId> activeIds{1, 2};

	EXPECT_EQ(validateCancel(1, activeIds), RejectReason::None);
}
