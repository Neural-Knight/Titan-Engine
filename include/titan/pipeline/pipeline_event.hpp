#pragma once

#include <cstdint>

#include "titan/core/types.hpp"

namespace titan {

// One queue slot per unit of consumer work. Shutdown is a sentinel with no payload.
enum class PipelineEventKind { SubmitOrder, CancelOrder, MatchOrder, CancelReplace, Shutdown };

// Preallocated in the ring buffer's own storage -- enqueue only assigns
// into an existing slot, no per-event heap traffic from the queue itself.
struct PipelineEvent {
    PipelineEventKind kind{PipelineEventKind::Shutdown};
    uint64_t sequence{0};
    uint64_t enqueueTimeNs{0};
    Symbol symbol;
    Order order{};
    OrderId cancelId{0};  // CancelOrder id, or the old id for CancelReplace
};

}  // namespace titan
