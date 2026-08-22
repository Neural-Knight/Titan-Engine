#pragma once

#include <unordered_set>

#include "titan/core/order_types.hpp"
#include "titan/core/types.hpp"

namespace titan {

// Stateless validation, checked before an order reaches a matcher.
// `existingIds`: all-time-seen ids for validateNewOrder, resting ids for validateCancel.

// Checks zero quantity, zero price (Limit only), duplicate id.
RejectReason validateNewOrder(const Order& order, const std::unordered_set<OrderId>& existingIds);

// Unknown id -> RejectReason::UnknownOrder.
RejectReason validateCancel(OrderId id, const std::unordered_set<OrderId>& existingIds);

}  // namespace titan
