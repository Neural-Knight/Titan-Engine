#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "order.h"

static_assert(std::is_same_v<OrderId, uint64_t>, "OrderId must be uint64_t");
static_assert(std::is_same_v<Price, uint64_t>, "Price must be uint64_t");
static_assert(std::is_same_v<Quantity, uint64_t>, "Quantity must be uint64_t");

TEST(OrderTypes, SideEnumValuesAreDifferent) {
	EXPECT_NE(static_cast<int>(Side::Buy), static_cast<int>(Side::Sell));
}

TEST(OrderTypes, OrderStoresAllFields) {
	const Order order{42, Side::Buy, 10150, 9};

	EXPECT_EQ(order.id, 42);
	EXPECT_EQ(order.side, Side::Buy);
	EXPECT_EQ(order.price, 10150);
	EXPECT_EQ(order.quantity, 9);
}

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

TEST(OrderBookBehavior, AddOrderAppearsAtCorrectPrice) {
	OrderBook book;
	book.addOrder(Order{1, Side::Buy, 100, 10});

	const auto &bids = book.getBids();
	auto levelIt = bids.find(100);
	ASSERT_NE(levelIt, bids.end());
	ASSERT_EQ(levelIt->second.size(), 1u);
	EXPECT_EQ(levelIt->second.front().id, 1u);
	EXPECT_EQ(levelIt->second.front().quantity, 10u);
}

TEST(OrderBookBehavior, TwoOrdersSamePricePreserveFifo) {
	OrderBook book;
	book.addOrder(Order{1, Side::Buy, 100, 10});
	book.addOrder(Order{2, Side::Buy, 100, 20});

	EXPECT_EQ(idsAtLevel(book.getBids(), 100), (std::vector<OrderId>{1, 2}));
}

TEST(OrderBookBehavior, CancelMiddleOrderKeepsRemainingOrder) {
	OrderBook book;
	book.addOrder(Order{1, Side::Buy, 100, 10});
	book.addOrder(Order{2, Side::Buy, 100, 20});
	book.addOrder(Order{3, Side::Buy, 100, 30});

	book.cancelOrder(2);

	EXPECT_EQ(idsAtLevel(book.getBids(), 100), (std::vector<OrderId>{1, 3}));
	EXPECT_EQ(book.getOrderTable().count(2), 0u);
}

TEST(OrderBookBehavior, CancelUnknownIdDoesNotCorruptState) {
	OrderBook book;
	book.addOrder(Order{10, Side::Buy, 101, 11});
	book.addOrder(Order{11, Side::Sell, 102, 12});

	const auto beforeBidIds = idsAtLevel(book.getBids(), 101);
	const auto beforeAskIds = idsAtLevel(book.getAsks(), 102);
	const auto beforeTableSize = book.getOrderTable().size();

	book.cancelOrder(999999);

	EXPECT_EQ(idsAtLevel(book.getBids(), 101), beforeBidIds);
	EXPECT_EQ(idsAtLevel(book.getAsks(), 102), beforeAskIds);
	EXPECT_EQ(book.getOrderTable().size(), beforeTableSize);
}

TEST(OrderBookBehavior, CancelLastOrderRemovesPriceLevel) {
	OrderBook book;
	book.addOrder(Order{99, Side::Sell, 200, 1});

	book.cancelOrder(99);

	EXPECT_EQ(book.getAsks().count(200), 0u);
	EXPECT_EQ(book.getOrderTable().count(99), 0u);
}
