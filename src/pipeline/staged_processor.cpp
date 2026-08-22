#include "titan/pipeline/staged_processor.hpp"

#include <chrono>
#include <cstdlib>

#include "titan/platform/thread_affinity.hpp"

namespace titan {

namespace {
uint64_t nowNanos()
{
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}
}  // namespace

StagedProcessor::StagedProcessor(MatcherBackend backend) : registry_(backend) {}

StagedProcessor::~StagedProcessor()
{
    if (consumer_.joinable())
        stop();
}

void StagedProcessor::start()
{
    consumer_ = std::thread(&StagedProcessor::consumerLoop, this);
    if (const char* cpuEnv = std::getenv("TITAN_PIN_CONSUMER_CPU"))
        pinThread(consumer_, std::atoi(cpuEnv));  // no-op false off Linux, ignored if unset
}

void StagedProcessor::stop()
{
    PipelineEvent shutdown;
    shutdown.kind = PipelineEventKind::Shutdown;
    enqueue(std::move(shutdown));
    join();
}

void StagedProcessor::join()
{
    if (consumer_.joinable())
        consumer_.join();
}

void StagedProcessor::enqueue(PipelineEvent event)
{
    while (!queue_.try_enqueue(std::move(event)))
        std::this_thread::yield();  // producer spins on a full queue, no drop
}

void StagedProcessor::enqueueSubmit(const Symbol& symbol, const Order& order, uint64_t sequence)
{
    PipelineEvent event;
    event.kind = PipelineEventKind::SubmitOrder;
    event.sequence = sequence;
    event.enqueueTimeNs = nowNanos();
    event.symbol = symbol;
    event.order = order;
    enqueue(std::move(event));
}

void StagedProcessor::enqueueCancel(const Symbol& symbol, OrderId id, uint64_t sequence)
{
    PipelineEvent event;
    event.kind = PipelineEventKind::CancelOrder;
    event.sequence = sequence;
    event.enqueueTimeNs = nowNanos();
    event.symbol = symbol;
    event.cancelId = id;
    enqueue(std::move(event));
}

void StagedProcessor::enqueueMatch(const Symbol& symbol, const Order& order, uint64_t sequence)
{
    PipelineEvent event;
    event.kind = PipelineEventKind::MatchOrder;
    event.sequence = sequence;
    event.enqueueTimeNs = nowNanos();
    event.symbol = symbol;
    event.order = order;
    enqueue(std::move(event));
}

void StagedProcessor::enqueueCancelReplace(const Symbol& symbol, OrderId oldId, const Order& newOrder,
                                            uint64_t sequence)
{
    PipelineEvent event;
    event.kind = PipelineEventKind::CancelReplace;
    event.sequence = sequence;
    event.enqueueTimeNs = nowNanos();
    event.symbol = symbol;
    event.cancelId = oldId;
    event.order = newOrder;
    enqueue(std::move(event));
}

void StagedProcessor::consumerLoop()
{
    PipelineEvent event;
    while (true)
    {
        if (!queue_.try_dequeue(event))
        {
            std::this_thread::yield();
            continue;
        }
        if (event.kind == PipelineEventKind::Shutdown)
            break;

        std::vector<Trade> trades;
        switch (event.kind)
        {
            case PipelineEventKind::SubmitOrder:
                registry_.submitOrder(event.symbol, event.order);
                break;
            case PipelineEventKind::CancelOrder:
                registry_.cancelOrder(event.symbol, event.cancelId);
                break;
            case PipelineEventKind::MatchOrder:
                trades = registry_.matchOrder(event.symbol, event.order);
                break;
            case PipelineEventKind::CancelReplace:
                registry_.cancelReplace(event.symbol, event.cancelId, event.order);
                break;
            case PipelineEventKind::Shutdown:
                break;
        }

        appliedSequences_.push_back(event.sequence);
        if (applyCallback_)
            applyCallback_(event.sequence, trades, nowNanos() - event.enqueueTimeNs);
    }
}

}  // namespace titan
