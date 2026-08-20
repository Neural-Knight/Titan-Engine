#include <gtest/gtest.h>

#include <limits>
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

// Total remaining quantity resting at a given price level (0 if the level is gone).
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

TEST(OrderBookBehavior, BestBidReturnsHighestPrice) {
	OrderBook book;
	book.addOrder(Order{1, Side::Buy, 100, 10});
	book.addOrder(Order{2, Side::Buy, 125, 20});
	book.addOrder(Order{3, Side::Buy, 110, 30});

	EXPECT_EQ(book.bestBid(), 125u);
}

TEST(OrderBookBehavior, BestAskReturnsLowestPrice) {
	OrderBook book;
	book.addOrder(Order{1, Side::Sell, 150, 10});
	book.addOrder(Order{2, Side::Sell, 125, 20});
	book.addOrder(Order{3, Side::Sell, 140, 30});

	EXPECT_EQ(book.bestAsk(), 125u);
}

TEST(OrderBookBehavior, EmptyBookReturnsConfiguredSentinels) {
	OrderBook book;

	EXPECT_EQ(book.bestBid(), std::numeric_limits<Price>::max());
	EXPECT_EQ(book.bestAsk(), 0u);
}

// ---------------------------------------------------------------------------
// Matching engine behaviour
// ---------------------------------------------------------------------------

// Full fill: SELL 100 @ 50 then BUY 100 @ 50 -> one trade, both orders gone.
TEST(MatchOrderBehavior, FullFillRemovesBothOrders) {
	OrderBook book;
	book.addOrder(Order{1, Side::Sell, 50, 100});

	const auto trades = book.matchOrder(Order{2, Side::Buy, 50, 100});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].incomingOrderId, 2u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].price, 50u);
	EXPECT_EQ(trades[0].quantity, 100u);

	// Nothing rests on either side and the order table is empty.
	EXPECT_EQ(book.getAsks().count(50), 0u);
	EXPECT_EQ(book.getBids().count(50), 0u);
	EXPECT_EQ(book.getOrderTable().count(1), 0u);
	EXPECT_EQ(book.getOrderTable().count(2), 0u);
	EXPECT_TRUE(book.getOrderTable().empty());
}

// Partial fill: SELL 100 @ 50 then BUY 40 @ 50 -> SELL 60 remains resting.
TEST(MatchOrderBehavior, PartialFillLeavesRestingRemainder) {
	OrderBook book;
	book.addOrder(Order{1, Side::Sell, 50, 100});

	const auto trades = book.matchOrder(Order{2, Side::Buy, 50, 40});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].incomingOrderId, 2u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].price, 50u);
	EXPECT_EQ(trades[0].quantity, 40u);

	// The resting SELL keeps 60 units; the incoming BUY is fully consumed.
	EXPECT_EQ(idsAtLevel(book.getAsks(), 50), (std::vector<OrderId>{1}));
	EXPECT_EQ(quantityAtLevel(book.getAsks(), 50), 60u);
	EXPECT_EQ(book.getBids().count(50), 0u);
	EXPECT_EQ(book.getOrderTable().count(2), 0u);
	EXPECT_EQ(book.getOrderTable().count(1), 1u);
}

// Multi-level sweep: SELL 100 @ 50 and SELL 100 @ 51, then BUY 150 @ 55.
// Expect trades 100@50 and 50@51; SELL 50 @ 51 remains, level 50 gone.
TEST(MatchOrderBehavior, MultiLevelSweepFillsAcrossPrices) {
	OrderBook book;
	book.addOrder(Order{1, Side::Sell, 50, 100});
	book.addOrder(Order{2, Side::Sell, 51, 100});

	const auto trades = book.matchOrder(Order{3, Side::Buy, 55, 150});

	ASSERT_EQ(trades.size(), 2u);

	// First trade sweeps the cheaper level completely.
	EXPECT_EQ(trades[0].incomingOrderId, 3u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].price, 50u);
	EXPECT_EQ(trades[0].quantity, 100u);

	// Second trade partially fills the next level at its resting price.
	EXPECT_EQ(trades[1].incomingOrderId, 3u);
	EXPECT_EQ(trades[1].restingOrderId, 2u);
	EXPECT_EQ(trades[1].price, 51u);
	EXPECT_EQ(trades[1].quantity, 50u);

	// Level 50 is fully consumed; 50 units of SELL #2 remain at 51.
	EXPECT_EQ(book.getAsks().count(50), 0u);
	EXPECT_EQ(idsAtLevel(book.getAsks(), 51), (std::vector<OrderId>{2}));
	EXPECT_EQ(quantityAtLevel(book.getAsks(), 51), 50u);

	// The incoming BUY was fully filled, so nothing rests on the bid side.
	EXPECT_TRUE(book.getBids().empty());
	EXPECT_EQ(book.getOrderTable().count(3), 0u);
	EXPECT_EQ(book.getOrderTable().count(1), 0u);
	EXPECT_EQ(book.getOrderTable().count(2), 1u);
}

// Mirror of the full-fill case driven from the SELL side, plus a
// no-cross guard: a BUY below the best ask must simply rest, not trade.
TEST(MatchOrderBehavior, IncomingSellFullyFillsRestingBuy) {
	OrderBook book;
	book.addOrder(Order{1, Side::Buy, 50, 100});

	const auto trades = book.matchOrder(Order{2, Side::Sell, 50, 100});

	ASSERT_EQ(trades.size(), 1u);
	EXPECT_EQ(trades[0].incomingOrderId, 2u);
	EXPECT_EQ(trades[0].restingOrderId, 1u);
	EXPECT_EQ(trades[0].price, 50u);
	EXPECT_EQ(trades[0].quantity, 100u);
	EXPECT_TRUE(book.getOrderTable().empty());
}

TEST(MatchOrderBehavior, NonCrossingBuyRestsWithoutTrading) {
	OrderBook book;
	book.addOrder(Order{1, Side::Sell, 51, 100});

	const auto trades = book.matchOrder(Order{2, Side::Buy, 50, 100});

	EXPECT_TRUE(trades.empty());
	// Both orders rest untouched at their own price levels.
	EXPECT_EQ(quantityAtLevel(book.getAsks(), 51), 100u);
	EXPECT_EQ(quantityAtLevel(book.getBids(), 50), 100u);
	EXPECT_EQ(book.getOrderTable().count(1), 1u);
	EXPECT_EQ(book.getOrderTable().count(2), 1u);
}
