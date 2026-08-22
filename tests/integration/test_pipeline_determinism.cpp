#include <gtest/gtest.h>

#include <random>
#include <vector>

#include "titan/exchange/instrument_registry.hpp"
#include "titan/pipeline/pipeline_event.hpp"
#include "titan/pipeline/spsc_queue.hpp"
#include "titan/pipeline/staged_processor.hpp"

using namespace titan;

namespace {

// ---- SPSC queue correctness (single-threaded, no concurrency needed to prove FIFO/full/empty) ----

TEST(SpscQueue, BurstEnqueueDequeuePreservesFifo)
{
    SpscQueue<int, 16> queue;
    for (int i = 0; i < 10; ++i)
        ASSERT_TRUE(queue.try_enqueue(int(i)));

    for (int i = 0; i < 10; ++i)
    {
        int value = -1;
        ASSERT_TRUE(queue.try_dequeue(value));
        EXPECT_EQ(value, i);
    }
}

TEST(SpscQueue, FullQueueRejectsEnqueue)
{
    SpscQueue<int, 4> queue;  // usable capacity 3
    EXPECT_EQ(queue.capacity(), 3u);
    ASSERT_TRUE(queue.try_enqueue(1));
    ASSERT_TRUE(queue.try_enqueue(2));
    ASSERT_TRUE(queue.try_enqueue(3));
    EXPECT_TRUE(queue.full());
    EXPECT_FALSE(queue.try_enqueue(4));

    int value = -1;
    ASSERT_TRUE(queue.try_dequeue(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(queue.try_enqueue(4));
}

TEST(SpscQueue, EmptyDequeueReturnsFalse)
{
    SpscQueue<int, 8> queue;
    int value = -1;
    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.try_dequeue(value));
    EXPECT_EQ(value, -1);
}

TEST(SpscQueue, SizeTracksThroughEnqueueDequeueCycles)
{
    SpscQueue<int, 8> queue;
    EXPECT_EQ(queue.size(), 0u);
    queue.try_enqueue(1);
    queue.try_enqueue(2);
    EXPECT_EQ(queue.size(), 2u);
    int value = -1;
    queue.try_dequeue(value);
    EXPECT_EQ(queue.size(), 1u);
}

// ---- Pipeline determinism: same op sequence via StagedProcessor vs direct InstrumentRegistry ----

struct PipelineTestOp {
    PipelineEventKind kind;
    OrderId orderId;
    Side side;
    Price price;
    Quantity quantity;
};

// Validity isn't guaranteed (e.g. cancelling an already-filled id) --
// both paths see the same sequence, so they accept/reject identically.
std::vector<PipelineTestOp> generateOps(size_t count, uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> kindRoll(0, 9);
    std::uniform_int_distribution<uint64_t> priceDist(90, 110);
    std::uniform_int_distribution<uint64_t> qtyDist(1, 50);

    std::vector<OrderId> issuedIds;
    std::vector<PipelineTestOp> ops;
    ops.reserve(count);
    OrderId nextId = 1;

    for (size_t i = 0; i < count; ++i)
    {
        const int roll = kindRoll(rng);
        PipelineEventKind kind = PipelineEventKind::SubmitOrder;
        if (roll <= 5)
            kind = PipelineEventKind::SubmitOrder;
        else if (roll <= 7 && !issuedIds.empty())
            kind = PipelineEventKind::CancelOrder;
        else if (roll == 8)
            kind = PipelineEventKind::MatchOrder;
        else if (!issuedIds.empty())
            kind = PipelineEventKind::CancelReplace;

        PipelineTestOp op{};
        op.kind = kind;
        op.side = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
        op.price = priceDist(rng);
        op.quantity = qtyDist(rng);

        if (kind == PipelineEventKind::CancelOrder || kind == PipelineEventKind::CancelReplace)
        {
            op.orderId = issuedIds[rng() % issuedIds.size()];
        }
        else
        {
            op.orderId = nextId++;
            issuedIds.push_back(op.orderId);
        }
        ops.push_back(op);
    }
    return ops;
}

size_t applyDirect(InstrumentRegistry& registry, const Symbol& symbol, const std::vector<PipelineTestOp>& ops)
{
    size_t tradeCount = 0;
    for (const auto& op : ops)
    {
        switch (op.kind)
        {
            case PipelineEventKind::SubmitOrder:
                registry.submitOrder(symbol, Order{op.orderId, op.side, op.price, op.quantity});
                break;
            case PipelineEventKind::CancelOrder:
                registry.cancelOrder(symbol, op.orderId);
                break;
            case PipelineEventKind::MatchOrder:
                tradeCount += registry.matchOrder(symbol, Order{op.orderId, op.side, op.price, op.quantity}).size();
                break;
            case PipelineEventKind::CancelReplace:
                registry.cancelReplace(symbol, op.orderId, Order{op.orderId, op.side, op.price, op.quantity});
                break;
            case PipelineEventKind::Shutdown:
                break;
        }
    }
    return tradeCount;
}

size_t applyViaPipeline(StagedProcessor& processor, const Symbol& symbol, const std::vector<PipelineTestOp>& ops)
{
    size_t tradeCount = 0;
    processor.setApplyCallback([&tradeCount](uint64_t, const std::vector<Trade>& trades, uint64_t) {
        tradeCount += trades.size();
    });
    processor.createInstrument(symbol);
    processor.start();

    uint64_t sequence = 0;
    for (const auto& op : ops)
    {
        switch (op.kind)
        {
            case PipelineEventKind::SubmitOrder:
                processor.enqueueSubmit(symbol, Order{op.orderId, op.side, op.price, op.quantity}, sequence++);
                break;
            case PipelineEventKind::CancelOrder:
                processor.enqueueCancel(symbol, op.orderId, sequence++);
                break;
            case PipelineEventKind::MatchOrder:
                processor.enqueueMatch(symbol, Order{op.orderId, op.side, op.price, op.quantity}, sequence++);
                break;
            case PipelineEventKind::CancelReplace:
                processor.enqueueCancelReplace(symbol, op.orderId, Order{op.orderId, op.side, op.price, op.quantity},
                                                sequence++);
                break;
            case PipelineEventKind::Shutdown:
                break;
        }
    }
    processor.stop();
    return tradeCount;
}

TEST(PipelineDeterminism, MatchesDirectReplayOnBookAndTradeCount)
{
    const Symbol symbol = "AAPL";
    const auto ops = generateOps(5000, 7);

    InstrumentRegistry direct;
    direct.createInstrument(symbol);
    const size_t directTrades = applyDirect(direct, symbol, ops);

    StagedProcessor processor(MatcherBackend::Optimized);
    const size_t pipelineTrades = applyViaPipeline(processor, symbol, ops);

    EXPECT_EQ(pipelineTrades, directTrades);

    const BookSnapshot directBook = direct.snapshot(symbol, 10);
    const BookSnapshot pipelineBook = processor.registryForInvariants().snapshot(symbol, 10);
    EXPECT_EQ(directBook.bids, pipelineBook.bids);
    EXPECT_EQ(directBook.asks, pipelineBook.asks);
}

TEST(PipelineDeterminism, StressNoLostOrDuplicateEvents)
{
    const Symbol symbol = "MSFT";
    const auto ops = generateOps(100000, 99);

    StagedProcessor processor(MatcherBackend::Optimized);
    processor.createInstrument(symbol);
    processor.start();

    uint64_t sequence = 0;
    for (const auto& op : ops)
    {
        switch (op.kind)
        {
            case PipelineEventKind::SubmitOrder:
                processor.enqueueSubmit(symbol, Order{op.orderId, op.side, op.price, op.quantity}, sequence++);
                break;
            case PipelineEventKind::CancelOrder:
                processor.enqueueCancel(symbol, op.orderId, sequence++);
                break;
            case PipelineEventKind::MatchOrder:
                processor.enqueueMatch(symbol, Order{op.orderId, op.side, op.price, op.quantity}, sequence++);
                break;
            case PipelineEventKind::CancelReplace:
                processor.enqueueCancelReplace(symbol, op.orderId, Order{op.orderId, op.side, op.price, op.quantity},
                                                sequence++);
                break;
            case PipelineEventKind::Shutdown:
                break;
        }
    }
    processor.stop();

    const std::vector<uint64_t>& applied = processor.appliedSequences();
    ASSERT_EQ(applied.size(), ops.size());
    for (size_t i = 0; i < applied.size(); ++i)
        EXPECT_EQ(applied[i], i);  // SPSC FIFO -- exact order, no gaps, no repeats
}

TEST(PipelineDeterminism, StopLeavesRegistryConsistentAndNoHang)
{
    const Symbol symbol = "GOOG";
    StagedProcessor processor(MatcherBackend::Optimized);
    processor.createInstrument(symbol);
    processor.start();

    processor.enqueueSubmit(symbol, Order{1, Side::Buy, 100, 10}, 0);
    processor.enqueueSubmit(symbol, Order{2, Side::Sell, 100, 10}, 1);
    processor.stop();

    const BookSnapshot snapshot = processor.registryForInvariants().snapshot(symbol, 10);
    EXPECT_TRUE(snapshot.bids.empty());
    EXPECT_TRUE(snapshot.asks.empty());
    EXPECT_EQ(processor.appliedSequences().size(), 2u);
}

}  // namespace
