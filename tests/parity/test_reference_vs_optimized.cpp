#include <gtest/gtest.h>

#include <random>
#include <vector>

#include "generators.hpp"
#include "reference/order_book.hpp"
#include "titan/book/optimized_matcher.hpp"

using namespace titan;
using namespace titan::fuzz;

namespace {

constexpr unsigned kSeed = 7;
constexpr int kOpCount = 10000;

void expectSameTrades(const std::vector<Trade>& a, const std::vector<Trade>& b, int opIndex)
{
    ASSERT_EQ(a.size(), b.size()) << "trade count mismatch at op #" << opIndex;
    for (size_t i = 0; i < a.size(); ++i)
    {
        EXPECT_EQ(a[i].incomingOrderId, b[i].incomingOrderId) << "op #" << opIndex;
        EXPECT_EQ(a[i].restingOrderId, b[i].restingOrderId) << "op #" << opIndex;
        EXPECT_EQ(a[i].price, b[i].price) << "op #" << opIndex;
        EXPECT_EQ(a[i].quantity, b[i].quantity) << "op #" << opIndex;
    }
}

void expectSameSnapshot(const ReferenceMatcher& reference, const OptimizedMatcher& optimized, int opIndex)
{
    ASSERT_EQ(reference.bestBid(), optimized.bestBid()) << "bestBid mismatch at op #" << opIndex;
    ASSERT_EQ(reference.bestAsk(), optimized.bestAsk()) << "bestAsk mismatch at op #" << opIndex;
    ASSERT_EQ(reference.bidDepth(10), optimized.bidDepth(10)) << "bidDepth mismatch at op #" << opIndex;
    ASSERT_EQ(reference.askDepth(10), optimized.askDepth(10)) << "askDepth mismatch at op #" << opIndex;
}

}  // namespace

TEST(ReferenceVsOptimized, RandomOperationSequenceStaysInParity)
{
    ReferenceMatcher reference;
    OptimizedMatcher optimized;
    std::mt19937 rng(kSeed);
    std::vector<OrderId> restingIds;
    OrderId nextId = 1;

    for (int i = 0; i < kOpCount; ++i)
    {
        FuzzOp op = randomOp(rng);
        if (op == FuzzOp::CancelReplace)
            op = FuzzOp::Add;  // no IMatcher equivalent, same remap as fuzz_order_book.cpp

        restingIds.clear();
        for (const auto& entry : reference.getOrderTable())
            restingIds.push_back(entry.first);

        switch (op)
        {
            case FuzzOp::Add:
            {
                const Order order = randomOrder(rng, nextId++);
                if (order.quantity > 0)
                {
                    reference.addOrder(order);
                    optimized.addOrder(order);
                }
                break;
            }
            case FuzzOp::Match:
            {
                const Order order = randomOrder(rng, nextId++);
                if (order.quantity > 0)
                {
                    const std::vector<Trade> refTrades = reference.matchOrder(order);
                    const std::vector<Trade> optTrades = optimized.matchOrder(order);
                    expectSameTrades(refTrades, optTrades, i);
                }
                break;
            }
            case FuzzOp::Cancel:
            {
                const OrderId target = restingIds.empty() ? nextId : restingIds[rng() % restingIds.size()];
                reference.cancelOrder(target);
                optimized.cancelOrder(target);
                break;
            }
            case FuzzOp::CancelReplace:
                break;  // unreachable, remapped to Add above
        }

        expectSameSnapshot(reference, optimized, i);
        if (::testing::Test::HasFailure())
            return;
    }
}

TEST(ReferenceVsOptimized, CrossingGtcViaAddOrderMatches)
{
    ReferenceMatcher reference;
    OptimizedMatcher optimized;

    const Order restingAsk{1, Side::Sell, 100, 50};
    reference.addOrder(restingAsk);
    optimized.addOrder(restingAsk);

    const Order crossingBid{2, Side::Buy, 100, 30};
    reference.addOrder(crossingBid);
    optimized.addOrder(crossingBid);

    expectSameSnapshot(reference, optimized, 0);
}

TEST(ReferenceVsOptimized, PartialFillLeavesRemainderMatches)
{
    ReferenceMatcher reference;
    OptimizedMatcher optimized;

    const Order restingBid{1, Side::Buy, 100, 100};
    reference.addOrder(restingBid);
    optimized.addOrder(restingBid);

    const Order incoming{2, Side::Sell, 100, 40};
    const std::vector<Trade> refTrades = reference.matchOrder(incoming);
    const std::vector<Trade> optTrades = optimized.matchOrder(incoming);

    expectSameTrades(refTrades, optTrades, 0);
    expectSameSnapshot(reference, optimized, 0);
}

TEST(ReferenceVsOptimized, MultiLevelSweepMatches)
{
    ReferenceMatcher reference;
    OptimizedMatcher optimized;

    const std::vector<Order> resting = {
        Order{1, Side::Sell, 100, 10},
        Order{2, Side::Sell, 101, 10},
        Order{3, Side::Sell, 102, 10},
    };
    for (const Order& order : resting)
    {
        reference.addOrder(order);
        optimized.addOrder(order);
    }

    const Order sweep{4, Side::Buy, 102, 25};
    const std::vector<Trade> refTrades = reference.matchOrder(sweep);
    const std::vector<Trade> optTrades = optimized.matchOrder(sweep);

    expectSameTrades(refTrades, optTrades, 0);
    expectSameSnapshot(reference, optimized, 0);
}

TEST(ReferenceVsOptimized, CancelUnknownIdIsNoOp)
{
    ReferenceMatcher reference;
    OptimizedMatcher optimized;

    reference.cancelOrder(999);
    optimized.cancelOrder(999);

    expectSameSnapshot(reference, optimized, 0);
}
