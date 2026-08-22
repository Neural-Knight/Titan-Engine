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

// Hand-crafted session: R, A(bid), A(ask), E(partial), X(partial),
// U(reprice bid), D(remove ask). Checkpoints after E, U, and D.
TEST(ItchCheckpoints, SessionCheckpointsMatchExpectedTopOfBook)
{
    ItchBookBuilder builder;
    builder.apply(makeDirectory(1, "AAPL"));
    builder.apply(makeAdd(1, 10, 'B', 100, 990000));
    builder.apply(makeAdd(1, 11, 'S', 80, 1010000));

    builder.apply(makeExecuted(1, 10, 30));

    // Checkpoint 1: bid reduced to 70@990000, ask untouched at 80@1010000.
    {
        const auto snap = builder.snapshot(1, 5);
        ASSERT_TRUE(snap.has_value());
        ASSERT_EQ(snap->bids.size(), 1u);
        EXPECT_EQ(snap->bids[0], (PriceLevel{990000, 70}));
        ASSERT_EQ(snap->asks.size(), 1u);
        EXPECT_EQ(snap->asks[0], (PriceLevel{1010000, 80}));
    }

    builder.apply(makeCancel(1, 11, 20));

    // Checkpoint 2: ask reduced to 60@1010000, bid unchanged.
    {
        const auto snap = builder.snapshot(1, 5);
        ASSERT_TRUE(snap.has_value());
        ASSERT_EQ(snap->bids.size(), 1u);
        EXPECT_EQ(snap->bids[0], (PriceLevel{990000, 70}));
        ASSERT_EQ(snap->asks.size(), 1u);
        EXPECT_EQ(snap->asks[0], (PriceLevel{1010000, 60}));
    }

    builder.apply(makeReplace(1, 10, 12, 70, 995000));

    // Checkpoint 3: bid moved to 995000, old 990000 level gone.
    {
        const auto snap = builder.snapshot(1, 5);
        ASSERT_TRUE(snap.has_value());
        ASSERT_EQ(snap->bids.size(), 1u);
        EXPECT_EQ(snap->bids[0], (PriceLevel{995000, 70}));
        EXPECT_EQ(builder.orderCount(1), 2u);
    }

    builder.apply(makeDelete(1, 11));

    // Checkpoint 4: ask side empty, bid side unaffected.
    {
        const auto snap = builder.snapshot(1, 5);
        ASSERT_TRUE(snap.has_value());
        EXPECT_TRUE(snap->asks.empty());
        ASSERT_EQ(snap->bids.size(), 1u);
        EXPECT_EQ(snap->bids[0], (PriceLevel{995000, 70}));
        EXPECT_EQ(builder.orderCount(1), 1u);
    }
}
