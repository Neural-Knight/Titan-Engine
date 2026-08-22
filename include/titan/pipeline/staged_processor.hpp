#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include "titan/exchange/instrument_registry.hpp"
#include "titan/pipeline/pipeline_event.hpp"
#include "titan/pipeline/spsc_queue.hpp"

namespace titan {

constexpr size_t kPipelineQueueCapacity = 4096;
using PipelineQueue = SpscQueue<PipelineEvent, kPipelineQueueCapacity>;

// Drives one InstrumentRegistry from a background consumer thread via a SPSC
// ring buffer. "Producer" = whichever thread calls enqueue*(); owns only the consumer thread.
class StagedProcessor {
public:
    // (sequence, trades produced, enqueue-to-applied latency in ns).
    using ApplyCallback = std::function<void(uint64_t, const std::vector<Trade>&, uint64_t)>;

    explicit StagedProcessor(MatcherBackend backend = MatcherBackend::Optimized);
    ~StagedProcessor();

    void setApplyCallback(ApplyCallback callback) { applyCallback_ = std::move(callback); }

    void createInstrument(const Symbol& symbol) { registry_.createInstrument(symbol); }

    // Spawns the consumer thread. Call once before any enqueue*().
    void start();
    // Enqueues the Shutdown sentinel and joins the consumer.
    void stop();
    void join();

    // Producer-side API. Spins (yields between retries) until the queue has
    // room -- callers must not enqueue after stop() has been called.
    void enqueueSubmit(const Symbol& symbol, const Order& order, uint64_t sequence);
    void enqueueCancel(const Symbol& symbol, OrderId id, uint64_t sequence);
    void enqueueMatch(const Symbol& symbol, const Order& order, uint64_t sequence);
    void enqueueCancelReplace(const Symbol& symbol, OrderId oldId, const Order& newOrder, uint64_t sequence);

    // Sequence numbers in the order the consumer actually applied them --
    // only safe to read after join() (consumer thread has exited by then).
    const std::vector<uint64_t>& appliedSequences() const { return appliedSequences_; }

    InstrumentRegistry& registryForInvariants() { return registry_; }

private:
    void enqueue(PipelineEvent event);
    void consumerLoop();

    InstrumentRegistry registry_;
    PipelineQueue queue_;
    std::thread consumer_;
    ApplyCallback applyCallback_;
    std::vector<uint64_t> appliedSequences_;
};

}  // namespace titan
