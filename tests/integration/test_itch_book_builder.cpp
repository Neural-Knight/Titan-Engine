#include <gtest/gtest.h>

#include "titan/feed/itch/book_builder.hpp"

using namespace titan;

namespace {

std::array<char, 8> makeStock(const std::string& symbol)
{
    std::array<char, 8> out{};
    out.fill(' ');
    for (size_t i = 0; i < symbol.size() && i < out.size(); ++i)
        out[i] = symbol[i];
    return out;
}

StockDirectoryMessage makeDirectory(StockLocate locate, const std::string& symbol)
{
    StockDirectoryMessage msg{};
    msg.stockLocate = locate;
    msg.stock = makeStock(symbol);
    return msg;
}

AddOrderMessage makeAdd(StockLocate locate, OrderRefNumber ref, char side, uint32_t shares, uint32_t price)
{
    AddOrderMessage msg{};
    msg.stockLocate = locate;
    msg.orderReferenceNumber = ref;
    msg.buySellIndicator = side;
    msg.shares = shares;
    msg.price = price;
    return msg;
}

OrderExecutedMessage makeExecuted(StockLocate locate, OrderRefNumber ref, uint32_t shares)
{
    OrderExecutedMessage msg{};
    msg.stockLocate = locate;
    msg.orderReferenceNumber = ref;
    msg.executedShares = shares;
    return msg;
}

OrderCancelMessage makeCancel(StockLocate locate, OrderRefNumber ref, uint32_t shares)
{
    OrderCancelMessage msg{};
    msg.stockLocate = locate;
    msg.orderReferenceNumber = ref;
    msg.cancelledShares = shares;
    return msg;
}

OrderDeleteMessage makeDelete(StockLocate locate, OrderRefNumber ref)
{
    OrderDeleteMessage msg{};
    msg.stockLocate = locate;
    msg.orderReferenceNumber = ref;
    return msg;
}

OrderReplaceMessage makeReplace(StockLocate locate, OrderRefNumber oldRef, OrderRefNumber newRef, uint32_t shares,
                                 uint32_t price)
{
    OrderReplaceMessage msg{};
    msg.stockLocate = locate;
    msg.originalOrderReferenceNumber = oldRef;
    msg.newOrderReferenceNumber = newRef;
    msg.shares = shares;
    msg.price = price;
    return msg;
}

}  // namespace

TEST(ItchBookBuilder, AddThenCancelEmptiesBook)
{
    ItchBookBuilder builder;
    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeAdd(1, 100, 'B', 50, 1000000));
    builder.apply(makeCancel(1, 100, 50));

    const auto snap = builder.snapshot(1, 5);
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(snap->bids.empty());
    EXPECT_EQ(builder.orderCount(1), 0u);
}

TEST(ItchBookBuilder, AddThenDeleteEmptiesBook)
{
    ItchBookBuilder builder;
    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeAdd(1, 100, 'S', 50, 1000000));
    builder.apply(makeDelete(1, 100));

    const auto snap = builder.snapshot(1, 5);
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(snap->asks.empty());
    EXPECT_EQ(builder.orderCount(1), 0u);
}

TEST(ItchBookBuilder, PartialExecuteLeavesRemainder)
{
    ItchBookBuilder builder;
    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeAdd(1, 200, 'S', 100, 500000));
    builder.apply(makeExecuted(1, 200, 40));

    const auto snap = builder.snapshot(1, 5);
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(snap->asks.size(), 1u);
    EXPECT_EQ(snap->asks[0].price, 500000u);
    EXPECT_EQ(snap->asks[0].quantity, 60u);
    EXPECT_EQ(builder.orderCount(1), 1u);
}

TEST(ItchBookBuilder, FullExecuteRemovesLevel)
{
    ItchBookBuilder builder;
    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeAdd(1, 200, 'S', 100, 500000));
    builder.apply(makeExecuted(1, 200, 100));

    const auto snap = builder.snapshot(1, 5);
    ASSERT_TRUE(snap.has_value());
    EXPECT_TRUE(snap->asks.empty());
    EXPECT_EQ(builder.orderCount(1), 0u);
}

TEST(ItchBookBuilder, ReplaceMovesPriceLevel)
{
    ItchBookBuilder builder;
    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeAdd(1, 300, 'B', 10, 1000000));
    builder.apply(makeReplace(1, 300, 301, 10, 1050000));

    const auto snap = builder.snapshot(1, 5);
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(snap->bids.size(), 1u);
    EXPECT_EQ(snap->bids[0].price, 1050000u);
    EXPECT_EQ(builder.orderCount(1), 1u);
}

TEST(ItchBookBuilder, MultiLevelBook)
{
    ItchBookBuilder builder;
    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeAdd(1, 1, 'B', 10, 990000));
    builder.apply(makeAdd(1, 2, 'B', 20, 1000000));
    builder.apply(makeAdd(1, 3, 'B', 5, 1000000));
    builder.apply(makeAdd(1, 4, 'B', 7, 980000));

    const auto snap = builder.snapshot(1, 3);
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(snap->bids.size(), 3u);
    EXPECT_EQ(snap->bids[0].price, 1000000u);
    EXPECT_EQ(snap->bids[0].quantity, 25u);
    EXPECT_EQ(snap->bids[1].price, 990000u);
    EXPECT_EQ(snap->bids[1].quantity, 10u);
    EXPECT_EQ(snap->bids[2].price, 980000u);
    EXPECT_EQ(snap->bids[2].quantity, 7u);
}

TEST(ItchBookBuilder, StockDirectoryRequired)
{
    ItchBookBuilder builder;
    builder.apply(makeAdd(1, 100, 'B', 50, 1000000));

    EXPECT_FALSE(builder.snapshot(1, 5).has_value());
    EXPECT_EQ(builder.orderCount(1), 0u);

    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeAdd(1, 100, 'B', 50, 1000000));

    const auto snap = builder.snapshot(1, 5);
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(snap->bids.size(), 1u);
}

TEST(ItchBookBuilder, TwoSymbolsIndependent)
{
    ItchBookBuilder builder;
    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeDirectory(2, "MSFT"));
    builder.apply(makeAdd(1, 1, 'B', 10, 1000000));
    builder.apply(makeAdd(2, 2, 'S', 20, 2000000));

    const auto snap1 = builder.snapshot(1, 5);
    const auto snap2 = builder.snapshot(2, 5);
    ASSERT_TRUE(snap1.has_value());
    ASSERT_TRUE(snap2.has_value());
    EXPECT_EQ(snap1->symbol, "AAPL");
    EXPECT_EQ(snap1->bids.size(), 1u);
    EXPECT_TRUE(snap1->asks.empty());
    EXPECT_EQ(snap2->symbol, "MSFT");
    EXPECT_TRUE(snap2->bids.empty());
    EXPECT_EQ(snap2->asks.size(), 1u);
}
