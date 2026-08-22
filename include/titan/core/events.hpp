#pragma once

#include <cstdint>
#include <variant>

#include "titan/core/order_types.hpp"
#include "titan/core/types.hpp"

namespace titan {

struct OrderSubmitted {
    Symbol symbol;
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;

    bool operator==(const OrderSubmitted&) const = default;
};

struct OrderCancelled {
    Symbol symbol;
    OrderId id;

    bool operator==(const OrderCancelled&) const = default;
};

// cancelReplace is same-id, so one id covers both the old and new order.
struct OrderReplaced {
    Symbol symbol;
    OrderId id;
    Price newPrice;
    Quantity newQuantity;

    bool operator==(const OrderReplaced&) const = default;
};

struct TradeExecuted {
    Symbol symbol;
    OrderId incomingOrderId;
    OrderId restingOrderId;
    Price price;
    Quantity quantity;

    bool operator==(const TradeExecuted&) const = default;
};

struct OrderRejected {
    Symbol symbol;
    OrderId id;
    RejectReason reason;

    bool operator==(const OrderRejected&) const = default;
};

using EventPayload = std::variant<OrderSubmitted, OrderCancelled, OrderReplaced, TradeExecuted, OrderRejected>;

// sequenceNumber is assigned by InstrumentRegistry, monotonic across all symbols.
struct Event {
    uint64_t sequenceNumber;
    EventPayload payload;

    bool operator==(const Event&) const = default;
};

}  // namespace titan
