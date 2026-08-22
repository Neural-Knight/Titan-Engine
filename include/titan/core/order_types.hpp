#pragma once

#include <cstdint>

namespace titan {

using AccountId = uint64_t;

// Market orders never rest, regardless of TimeInForce — TimeInForce is
// meaningless on a Market order and ignored by OrderManager for it.
enum class OrderType {
    Limit,
    Market
};

// IOC only applies to Limit orders: crossing is attempted at the limit
// price, and any unfilled remainder is cancelled instead of resting.
enum class TimeInForce {
    GTC,
    IOC
};

enum class OrderStatus {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

enum class RejectReason {
    None,
    ZeroQuantity,
    ZeroPrice,
    DuplicateOrderId,
    UnknownOrder
};

}  // namespace titan
