#pragma once

#include <cstdint>

#include "titan/core/order_types.hpp"
#include "titan/core/types.hpp"

namespace titan {

enum class ExecType {
    New,
    Trade,
    Cancelled,
    Replaced,
    Rejected
};

// rejectReason is only meaningful when execType == Rejected.
struct ExecutionReport {
    Symbol symbol;
    uint64_t sequenceNumber;
    OrderId orderId;
    ExecType execType;
    Quantity lastQty;
    Quantity cumQty;
    Quantity leavesQty;
    Price lastPx;
    Price avgPx;
    RejectReason rejectReason;

    bool operator==(const ExecutionReport&) const = default;
};

}  // namespace titan
