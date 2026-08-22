#pragma once

#include <unordered_set>

#include "titan/core/order_types.hpp"
#include "titan/core/types.hpp"

namespace titan {

// Stateless validation rules, checked before an order reaches a matcher.
// `existingIds` is the caller's notion of "already in use" — OrderManager
// passes all-time-seen ids for validateNewOrder (ids are never reused) and
// currently-resting ids for validateCancel (only a resting order can be
// cancelled).

// Checks, in order: zero quantity, zero price (Limit orders), duplicate id.
RejectReason validateNewOrder(const Order& order, const std::unordered_set<OrderId>& existingIds);

// Unknown id -> RejectReason::UnknownOrder; otherwise RejectReason::None.
RejectReason validateCancel(OrderId id, const std::unordered_set<OrderId>& existingIds);

}  // namespace titan
