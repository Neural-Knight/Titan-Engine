#pragma once

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "titan/book/i_matcher.hpp"
#include "titan/core/order_types.hpp"
#include "titan/core/types.hpp"

namespace titan {

struct AcceptResult {
    bool accepted;
    RejectReason reason;
};

// OrderManager — the only entry point exchange callers use. Validates
// before touching the matcher and tracks per-order lifecycle status
// (New/PartiallyFilled/Filled/Cancelled/Rejected) that IMatcher itself
// has no notion of.
//
// Order ids are never reused: once an id is accepted (via addOrder or
// matchOrder) it stays "known" forever, even after the order is fully
// filled or cancelled, so a later submission with the same id is
// rejected as a duplicate. Only currently-resting ids are cancellable.
class OrderManager {
public:
    explicit OrderManager(IMatcher& matcher);

    // Rests `order` unconditionally (after validation). Status -> New.
    AcceptResult addOrder(Order order);

    // Cancels a currently-resting order. Status -> Cancelled.
    AcceptResult cancelOrder(OrderId id);

    // Validates, then matches `order` against the book. Updates status
    // for every resting order it fills.
    //
    // GTC Limit (the only resting type): Filled if fully consumed,
    // PartiallyFilled if a remainder now rests, New if it didn't cross
    // at all.
    //
    // Market and IOC Limit never rest: Market sweeps at an OrderManager-
    // synthesized extreme price (Price::max() for a buy, 0 for a sell) so
    // it crosses every level on the opposite side; IOC Limit crosses at
    // its own price like GTC. Either way, any unfilled remainder that
    // ReferenceMatcher would otherwise rest is cancelled before this call
    // returns, so it is never externally observable as resting. Final
    // status is Filled if anything traded, else Cancelled — never
    // PartiallyFilled or New.
    //
    // Returns an empty vector if `order` is rejected by validation.
    std::vector<Trade> matchOrder(Order order);

    // nullopt if `id` has never been accepted by this manager.
    std::optional<OrderStatus> statusOf(OrderId id) const;

    // nullopt if `id` has never been accepted by this manager.
    std::optional<Order> find(OrderId id) const;

private:
    IMatcher& matcher_;

    // Every id ever accepted, whether still resting or not. Backs
    // duplicate-id rejection.
    std::unordered_set<OrderId> allKnownOrderIds_;

    // Ids currently resting on the book. Backs cancel-eligibility checks.
    std::unordered_set<OrderId> activeOrderIds_;

    // Latest known snapshot (remaining quantity + status) per known id.
    std::unordered_map<OrderId, Order> orders_;
};

}  // namespace titan
