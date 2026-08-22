#pragma once

#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "titan/book/i_matcher.hpp"
#include "titan/core/order_types.hpp"
#include "titan/core/types.hpp"
#include "titan/exchange/self_trade_prevention.hpp"

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
    //
    // Only resting order types belong here: a Market order never rests
    // and an IOC Limit must not rest, so submitting either via addOrder()
    // is rejected with RejectReason::InvalidOrderType — those go through
    // matchOrder() instead.
    AcceptResult addOrder(Order order);

    // Cancels a currently-resting order. Status -> Cancelled.
    AcceptResult cancelOrder(OrderId id);

    // Atomic cancel/replace (amend) of a currently-resting order.
    //
    // `oldId` must be currently resting; otherwise RejectReason::OrderNotResting.
    // `newOrder.id` must equal `oldId` — same-id amend is the only form
    // supported, so a replace keeps the caller's order identity. A mismatch,
    // or a `newOrder` that can't rest (Market, or Limit+IOC), is
    // RejectReason::InvalidReplace. Otherwise `newOrder` is validated as a
    // fresh order (with `oldId` excluded from the duplicate-id check, since
    // it's being replaced, not re-added) — any validation failure returns
    // that specific reason (e.g. ZeroQuantity). On any rejection the OLD
    // order is left completely untouched.
    //
    // Priority rules (documented exchange conventions), comparing against
    // the old order's CURRENT remaining resting quantity:
    //   - Price change             -> full loss of time priority: removed
    //                                  and re-inserted at the back of the
    //                                  new price level.
    //   - Same price, qty DECREASE
    //     (or unchanged)            -> queue position preserved. IMatcher has
    //                                  no in-place-mutate primitive, so this
    //                                  is done by cancelling and re-adding
    //                                  every order resting at that price
    //                                  level in their original relative
    //                                  order, substituting the old id's new
    //                                  quantity — net effect: same slot,
    //                                  smaller size.
    //   - Same price, qty INCREASE -> loses time priority (back of the
    //                                  level). Chosen because the added size
    //                                  did not earn the original queue slot.
    //
    // The replacement never crosses — like addOrder, it only rests. Status
    // of the (same-id) order is reset to New on success.
    AcceptResult cancelReplace(OrderId oldId, Order newOrder);

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
    // its own price like GTC. Any unfilled remainder is discarded rather
    // than rested. Final status is Filled if anything traded, else
    // Cancelled — never PartiallyFilled or New.
    //
    // Self-Trade Prevention (CancelIncoming — see self_trade_prevention.hpp):
    // applies uniformly to Limit GTC/IOC and Market. Matching proceeds one
    // resting order at a time; before each fill, if the resting order about
    // to be hit shares `order`'s accountId, matching stops immediately —
    // no trade against it, it is left untouched, and everything from that
    // point on is treated as unfilled remainder (discarded or rested per
    // the rules above).
    //
    // Returns an empty vector if `order` is rejected by validation.
    std::vector<Trade> matchOrder(Order order);

    // nullopt if `id` has never been accepted by this manager.
    std::optional<OrderStatus> statusOf(OrderId id) const;

    // nullopt if `id` has never been accepted by this manager.
    std::optional<Order> find(OrderId id) const;

private:
    IMatcher& matcher_;

    // The STP mode enforced by this manager. Fixed at CancelIncoming for
    // Module 4; kept as a member so a future module can make it configurable.
    StpMode stpMode_{StpMode::CancelIncoming};

    // Matches `incoming` against the book one resting order at a time,
    // stopping before any fill against a resting order owned by
    // incoming.accountId (StpMode::CancelIncoming). Never rests anything —
    // callers decide what to do with the unfilled remainder. Sets `filled`
    // to the total quantity actually traded.
    std::vector<Trade> matchWithStp(const Order& incoming, Quantity& filled);

    // OrderManager-side mirror of resting order-id FIFO queues, keyed by
    // (side, price), kept in lock-step with every rest/fill/cancel/replace
    // this manager performs. This is what lets matchWithStp peek "who is at
    // the front of the best crossable price level" — and cancelReplace
    // rebuild a level in-order — purely at this layer, without asking a
    // concrete IMatcher for anything beyond addOrder/cancelOrder/matchOrder.
    std::map<Price, std::vector<OrderId>> restingBidsAt_;
    std::map<Price, std::vector<OrderId>> restingAsksAt_;

    void trackResting(const Order& order);
    void untrackResting(OrderId id);

    // Every id ever accepted, whether still resting or not. Backs
    // duplicate-id rejection.
    std::unordered_set<OrderId> allKnownOrderIds_;

    // Ids currently resting on the book. Backs cancel-eligibility checks.
    std::unordered_set<OrderId> activeOrderIds_;

    // Latest known snapshot (remaining quantity + status) per known id.
    std::unordered_map<OrderId, Order> orders_;
};

}  // namespace titan
