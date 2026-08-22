#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

#include "reference/order_book.hpp"
#include "titan/book/i_matcher.hpp"

using namespace titan;

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

// Total remaining quantity resting at a price level (0 if the level is gone).
Quantity quantityAtLevel(const PriceLevels &levels, Price price) {
	auto levelIt = levels.find(price);
	if (levelIt == levels.end()) {
		return 0u;
	}

	Quantity total = 0u;
	for (const auto &order : levelIt->second) {
		total += order.quantity;
	}
	return total;
}

}  // namespace

TEST(OrderBookBehavior, AddOrderAppearsAtCorrectPrice) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Buy, 100, 10});

	const auto &bids = book.getBids();
	auto levelIt = bids.find(100);
	ASSERT_NE(levelIt, bids.end());
	ASSERT_EQ(levelIt->second.size(), 1u);
	EXPECT_EQ(levelIt->second.front().id, 1u);
	EXPECT_EQ(levelIt->second.front().quantity, 10u);
}

TEST(OrderBookBehavior, TwoOrdersSamePricePreserveFifo) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Buy, 100, 10});
	book.addOrder(Order{2, Side::Buy, 100, 20});

	EXPECT_EQ(idsAtLevel(book.getBids(), 100), (std::vector<OrderId>{1, 2}));
}

TEST(OrderBookBehavior, CancelMiddleOrderKeepsRemainingOrder) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Buy, 100, 10});
	book.addOrder(Order{2, Side::Buy, 100, 20});
	book.addOrder(Order{3, Side::Buy, 100, 30});

	book.cancelOrder(2);

	EXPECT_EQ(idsAtLevel(book.getBids(), 100), (std::vector<OrderId>{1, 3}));
	EXPECT_EQ(book.getOrderTable().count(2), 0u);
}

TEST(OrderBookBehavior, CancelUnknownIdDoesNotCorruptState) {
	ReferenceMatcher book;
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
	ReferenceMatcher book;
	book.addOrder(Order{99, Side::Sell, 200, 1});

	book.cancelOrder(99);

	EXPECT_EQ(book.getAsks().count(200), 0u);
	EXPECT_EQ(book.getOrderTable().count(99), 0u);
}

TEST(OrderBookBehavior, BestBidReturnsHighestPrice) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Buy, 100, 10});
	book.addOrder(Order{2, Side::Buy, 125, 20});
	book.addOrder(Order{3, Side::Buy, 110, 30});

	EXPECT_EQ(book.bestBid(), 125u);
}

TEST(OrderBookBehavior, BestAskReturnsLowestPrice) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Sell, 150, 10});
	book.addOrder(Order{2, Side::Sell, 125, 20});
	book.addOrder(Order{3, Side::Sell, 140, 30});

	EXPECT_EQ(book.bestAsk(), 125u);
}

TEST(OrderBookBehavior, EmptyBookReturnsConfiguredSentinels) {
	ReferenceMatcher book;

	EXPECT_EQ(book.bestBid(), std::numeric_limits<Price>::max());
	EXPECT_EQ(book.bestAsk(), 0u);
}

TEST(OrderBookBehavior, AddOrderCrossingLimitMatchesBeforeResting) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Sell, 50, 100});

	book.addOrder(Order{2, Side::Buy, 50, 100});

	EXPECT_TRUE(book.getBids().empty());
	EXPECT_TRUE(book.getAsks().empty());
	EXPECT_TRUE(book.getOrderTable().empty());
}

TEST(OrderBookBehavior, AddOrderNonCrossingRests) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Sell, 51, 100});

	book.addOrder(Order{2, Side::Buy, 50, 100});

	EXPECT_LT(book.bestBid(), book.bestAsk());
	EXPECT_EQ(idsAtLevel(book.getBids(), 50), (std::vector<OrderId>{2}));
	EXPECT_EQ(idsAtLevel(book.getAsks(), 51), (std::vector<OrderId>{1}));
}

TEST(MatchOrderBehavior, FullFillRemovesBothOrders) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Sell, 50, 100});

	const auto trades = book.matchOrder(Order{2, Side::Buy, 50, 100});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].incomingOrderId, 2u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].price, 50u);
	EXPECT_EQ(trades[0].quantity, 100u);

	EXPECT_EQ(book.getAsks().count(50), 0u);
	EXPECT_EQ(book.getBids().count(50), 0u);
	EXPECT_TRUE(book.getOrderTable().empty());
}

TEST(MatchOrderBehavior, PartialFillLeavesRestingRemainder) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Sell, 50, 100});

	const auto trades = book.matchOrder(Order{2, Side::Buy, 50, 40});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].quantity, 40u);
	EXPECT_EQ(idsAtLevel(book.getAsks(), 50), (std::vector<OrderId>{1}));
	EXPECT_EQ(quantityAtLevel(book.getAsks(), 50), 60u);
	EXPECT_EQ(book.getBids().count(50), 0u);
}

// SELL 100@50 and SELL 100@51, then BUY 150@55 sweeps both levels.
TEST(MatchOrderBehavior, MultiLevelSweepFillsAcrossPrices) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Sell, 50, 100});
	book.addOrder(Order{2, Side::Sell, 51, 100});

	const auto trades = book.matchOrder(Order{3, Side::Buy, 55, 150});

	ASSERT_EQ(trades.size(), 2u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].price, 50u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_EQ(trades[1].restingOrderId, 2u);
	EXPECT_EQ(trades[1].price, 51u);
	EXPECT_EQ(trades[1].quantity, 50u);

	EXPECT_EQ(book.getAsks().count(50), 0u);
	EXPECT_EQ(idsAtLevel(book.getAsks(), 51), (std::vector<OrderId>{2}));
	EXPECT_TRUE(book.getBids().empty());
}

TEST(MatchOrderBehavior, IncomingSellFullyFillsRestingBuy) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Buy, 50, 100});

	const auto trades = book.matchOrder(Order{2, Side::Sell, 50, 100});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_TRUE(book.getOrderTable().empty());
}

// FIFO: two SELLs @ 50, BUY 150@50 fills the older one first, fully.
TEST(MatchOrderBehavior, FifoPriorityFillsOldestOrderFirst) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Sell, 50, 100});
	book.addOrder(Order{2, Side::Sell, 50, 100});

	const auto trades = book.matchOrder(Order{3, Side::Buy, 50, 150});

	ASSERT_EQ(trades.size(), 2u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_EQ(trades[1].restingOrderId, 2u);
	EXPECT_EQ(trades[1].quantity, 50u);

	EXPECT_EQ(idsAtLevel(book.getAsks(), 50), (std::vector<OrderId>{2}));
	EXPECT_EQ(quantityAtLevel(book.getAsks(), 50), 50u);
	EXPECT_TRUE(book.getBids().empty());
}

TEST(MatchOrderBehavior, NonCrossingBuyRestsWithoutTrading) {
	ReferenceMatcher book;
	book.addOrder(Order{1, Side::Sell, 51, 100});

	const auto trades = book.matchOrder(Order{2, Side::Buy, 50, 100});

	EXPECT_TRUE(trades.empty());
	EXPECT_EQ(quantityAtLevel(book.getAsks(), 51), 100u);
	EXPECT_EQ(quantityAtLevel(book.getBids(), 50), 100u);
}

TEST(IMatcherPolymorphism, DispatchesThroughBasePointer) {
	ReferenceMatcher concrete;
	IMatcher &book = concrete;

	book.addOrder(Order{1, Side::Sell, 50, 100});
	book.addOrder(Order{2, Side::Buy, 40, 10});

	EXPECT_EQ(book.bestAsk(), 50u);
	EXPECT_EQ(book.bestBid(), 40u);

	book.cancelOrder(2);
	EXPECT_EQ(book.bestBid(), std::numeric_limits<Price>::max());

	const auto trades = book.matchOrder(Order{3, Side::Buy, 50, 100});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_EQ(book.bestAsk(), 0u);
}
