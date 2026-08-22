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

struct MatchResult {
    std::vector<Trade> trades;
    AcceptResult accept;
};

// Exchange entry point: validation, lifecycle, STP on top of IMatcher.
class OrderManager {
public:
    explicit OrderManager(IMatcher& matcher);

    // GTC limit order entry. Market/IOC are rejected here (use matchOrder).
    AcceptResult addOrder(Order order);

    // Cancels a resting order. Status -> Cancelled.
    AcceptResult cancelOrder(OrderId id);

    // Same-id amend; price change or qty increase loses queue priority.
    // Rejected with ReplaceWouldCross if the new price would cross.
    AcceptResult cancelReplace(OrderId oldId, Order newOrder);

    // Matches order against the book; GTC rests any non-crossing remainder.
    MatchResult matchOrder(Order order);

    // nullopt if `id` was never accepted by this manager.
    std::optional<OrderStatus> statusOf(OrderId id) const;

    // nullopt if `id` was never accepted by this manager.
    std::optional<Order> find(OrderId id) const;

    // Test/invariant-only introspection into private bookkeeping.
    const std::unordered_set<OrderId>& activeOrderIdsForInvariants() const { return activeOrderIds_; }
    const std::map<Price, std::vector<OrderId>>& restingBidsForInvariants() const { return restingBidsAt_; }
    const std::map<Price, std::vector<OrderId>>& restingAsksForInvariants() const { return restingAsksAt_; }

private:
    IMatcher& matcher_;
    StpMode stpMode_{StpMode::CancelIncoming};

    // Matches `incoming` one resting order at a time, stopping before a
    // same-account fill. Never rests; sets `filled` to quantity traded.
    std::vector<Trade> matchWithStp(const Order& incoming, Quantity& filled);

    // Would `order` cross the opposite side, per this manager's own
    // shadow queues (avoids depending on matcher sentinel values).
    bool wouldCross(const Order& order) const;

    // (side, price) -> FIFO order-id mirror of the real book.
    std::map<Price, std::vector<OrderId>> restingBidsAt_;
    std::map<Price, std::vector<OrderId>> restingAsksAt_;

    void trackResting(const Order& order);
    void untrackResting(OrderId id);

    // Every id ever accepted; backs duplicate-id rejection.
    std::unordered_set<OrderId> allKnownOrderIds_;

    // Ids currently resting; backs cancel-eligibility checks.
    std::unordered_set<OrderId> activeOrderIds_;

    // Latest known snapshot per known id.
    std::unordered_map<OrderId, Order> orders_;
};

}  // namespace titan
