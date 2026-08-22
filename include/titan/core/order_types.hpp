#pragma once

#include <cstdint>

namespace titan {

using AccountId = uint64_t;

// Only Limit exists today. Market/IOC arrive in Module 3.
enum class OrderType {
    Limit
};

// Only GTC exists today.
enum class TimeInForce {
    GTC
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
