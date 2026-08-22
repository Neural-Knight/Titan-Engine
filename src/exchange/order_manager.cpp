#include "titan/exchange/order_manager.hpp"

#include <algorithm>
#include <limits>

#include "titan/exchange/validator.hpp"

namespace titan {

OrderManager::OrderManager(IMatcher& matcher) : matcher_(matcher) {}

// --- resting-shadow bookkeeping ---------------------------------------------
// A minimal (side, price) -> FIFO order-id mirror of the book, updated on
// every rest/fill/cancel/replace this manager performs. Lets matchWithStp
// peek the front resting order at a price level, and cancelReplace rebuild a
// level in original order, without depending on a concrete matcher for
// anything beyond addOrder/cancelOrder/matchOrder.

void OrderManager::trackResting(const Order& order)
{
    auto& levels = (order.side == Side::Buy) ? restingBidsAt_ : restingAsksAt_;
    levels[order.price].push_back(order.id);
}

void OrderManager::untrackResting(OrderId id)
{
    auto eraseFrom = [id](std::map<Price, std::vector<OrderId>>& levels) -> bool {
        for (auto it = levels.begin(); it != levels.end(); ++it)
        {
            auto& ids = it->second;
            auto pos = std::find(ids.begin(), ids.end(), id);
            if (pos != ids.end())
            {
                ids.erase(pos);
                if (ids.empty())
                    levels.erase(it);
                return true;
            }
        }
        return false;
    };
    if (!eraseFrom(restingBidsAt_))
        eraseFrom(restingAsksAt_);
}

// --- order entry -------------------------------------------------------------

AcceptResult OrderManager::addOrder(Order order)
{
    // addOrder is the resting path only. Market never rests; IOC must not
    // rest — both belong on matchOrder(). Reject them here so a caller
    // cannot silently rest a would-be-aggressive order.
    if (order.type == OrderType::Market ||
        (order.type == OrderType::Limit && order.tif == TimeInForce::IOC))
        return AcceptResult{false, RejectReason::InvalidOrderType};

    const RejectReason reason = validateNewOrder(order, allKnownOrderIds_);
    if (reason != RejectReason::None)
        return AcceptResult{false, reason};

    order.status = OrderStatus::New;
    matcher_.addOrder(order);

    allKnownOrderIds_.insert(order.id);
    activeOrderIds_.insert(order.id);
    orders_[order.id] = order;
    trackResting(order);

    return AcceptResult{true, RejectReason::None};
}

AcceptResult OrderManager::cancelOrder(OrderId id)
{
    const RejectReason reason = validateCancel(id, activeOrderIds_);
    if (reason != RejectReason::None)
        return AcceptResult{false, reason};

    matcher_.cancelOrder(id);

    activeOrderIds_.erase(id);
    untrackResting(id);
    orders_.at(id).status = OrderStatus::Cancelled;

    return AcceptResult{true, RejectReason::None};
}

AcceptResult OrderManager::cancelReplace(OrderId oldId, Order newOrder)
{
    // The order being replaced must currently rest.
    if (activeOrderIds_.count(oldId) == 0)
        return AcceptResult{false, RejectReason::OrderNotResting};

    // Same-id amend only.
    if (newOrder.id != oldId)
        return AcceptResult{false, RejectReason::InvalidReplace};

    // A replacement only rests; Market/IOC have no resting semantics.
    if (newOrder.type == OrderType::Market ||
        (newOrder.type == OrderType::Limit && newOrder.tif == TimeInForce::IOC))
        return AcceptResult{false, RejectReason::InvalidReplace};

    // Validate as a fresh order, excluding oldId from the duplicate check
    // (it's being replaced, not re-added as a new id). Any failure leaves
    // the OLD order completely untouched.
    std::unordered_set<OrderId> idsExcludingOld = allKnownOrderIds_;
    idsExcludingOld.erase(oldId);
    const RejectReason reason = validateNewOrder(newOrder, idsExcludingOld);
    if (reason != RejectReason::None)
        return AcceptResult{false, reason};

    const Order existing = orders_.at(oldId);
    const bool samePrice = (newOrder.price == existing.price);
    const bool preservesPriority = samePrice && (newOrder.quantity <= existing.quantity);

    newOrder.status = OrderStatus::New;

    if (preservesPriority)
    {
        // Same price, quantity decreased (or unchanged): preserve queue
        // position. IMatcher has no in-place-mutate primitive, so the whole
        // price level is rebuilt — cancelled and re-added in its recorded
        // FIFO order — substituting oldId's reduced quantity. Every other
        // order at the level keeps its exact relative slot; only oldId's
        // size actually changes.
        auto& levels = (newOrder.side == Side::Buy) ? restingBidsAt_ : restingAsksAt_;
        const std::vector<OrderId>& ids = levels[newOrder.price];

        orders_[oldId] = newOrder;

        for (OrderId id : ids)
            matcher_.cancelOrder(id);
        for (OrderId id : ids)
            matcher_.addOrder(orders_.at(id));

        // Shadow queue order is unchanged; oldId keeps its slot and stays
        // active — nothing else to update.
        return AcceptResult{true, RejectReason::None};
    }

    // Price change, or quantity increase at the same price: full loss of
    // priority. Remove the old resting order and insert the replacement
    // fresh, at the back of its (possibly new) price level.
    matcher_.cancelOrder(oldId);
    untrackResting(oldId);

    matcher_.addOrder(newOrder);
    orders_[oldId] = newOrder;
    activeOrderIds_.insert(oldId);  // was already active; stays active under the same id
    trackResting(newOrder);

    return AcceptResult{true, RejectReason::None};
}

// --- matching + self-trade prevention ---------------------------------------

std::vector<Trade> OrderManager::matchWithStp(const Order& incoming, Quantity& filled)
{
    std::vector<Trade> allTrades;
    filled = 0;

    Quantity remaining = incoming.quantity;
    const bool buy = (incoming.side == Side::Buy);

    // The crossing side is the opposite book: a buy lifts asks, a sell
    // hits bids. The shadow queues mirror exactly what the real matcher
    // holds, since this manager is the sole path to every book mutation.
    auto& levels = buy ? restingAsksAt_ : restingBidsAt_;

    while (remaining > 0)
    {
        if (levels.empty())
            break;

        // Best crossable price: lowest ask for a buy, highest bid for a sell.
        auto levelIt = buy ? levels.begin() : std::prev(levels.end());
        const Price levelPrice = levelIt->first;

        const bool crosses = buy ? (incoming.price >= levelPrice) : (incoming.price <= levelPrice);
        if (!crosses)
            break;

        std::vector<OrderId>& ids = levelIt->second;
        if (ids.empty())
        {
            levels.erase(levelIt);  // defensive: shouldn't happen, mirror is kept non-empty
            continue;
        }

        const OrderId frontId = ids.front();
        const Order& resting = orders_.at(frontId);

        // STP (CancelIncoming): the front resting order at the best
        // crossable level belongs to the same account as the incoming
        // order. Stop matching entirely — do not skip past it to reach
        // further liquidity, even liquidity from other accounts resting
        // behind it. The resting order itself is left untouched.
        // AccountId 0 means "unassigned" and never collides with itself.
        if (incoming.accountId != 0 && resting.accountId == incoming.accountId)
            break;

        // Cross exactly this one resting order: a probe sized to whichever
        // side is smaller always fully consumes itself in a single trade
        // against `resting` (never leaving a remainder for the real
        // matcher to rest), so this never creates book state we'd need to
        // clean up afterward.
        const Quantity chunk = std::min(remaining, resting.quantity);

        Order probe = incoming;
        probe.quantity = chunk;

        const std::vector<Trade> trades = matcher_.matchOrder(probe);
        if (trades.empty())
            break;  // defensive: shadow desynced from the real book, stop rather than loop forever

        for (const Trade& trade : trades)
        {
            allTrades.push_back(trade);
            filled += trade.quantity;
            remaining -= trade.quantity;

            auto restingIt = orders_.find(trade.restingOrderId);
            if (restingIt == orders_.end())
                continue;

            Order& restingSnapshot = restingIt->second;
            restingSnapshot.quantity -= trade.quantity;
            if (restingSnapshot.quantity == 0)
            {
                restingSnapshot.status = OrderStatus::Filled;
                activeOrderIds_.erase(trade.restingOrderId);
                untrackResting(trade.restingOrderId);
            }
            else
            {
                restingSnapshot.status = OrderStatus::PartiallyFilled;
            }
        }
    }

    return allTrades;
}

std::vector<Trade> OrderManager::matchOrder(Order order)
{
    const RejectReason reason = validateNewOrder(order, allKnownOrderIds_);
    if (reason != RejectReason::None)
        return {};

    // Market never rests, regardless of tif; IOC Limit never rests either.
    // GTC Limit is the only combination allowed to rest a remainder.
    const bool isMarket = order.type == OrderType::Market;
    const bool mayRest = !isMarket && order.tif != TimeInForce::IOC;

    // Market sweeps at a synthesized extreme price so it crosses every level.
    Order toMatch = order;
    if (isMarket)
        toMatch.price = (order.side == Side::Buy) ? std::numeric_limits<Price>::max() : Price{0};

    const Quantity originalQuantity = order.quantity;
    allKnownOrderIds_.insert(order.id);

    Quantity filled = 0;
    const std::vector<Trade> trades = matchWithStp(toMatch, filled);

    const Quantity remaining = originalQuantity - filled;

    if (remaining > 0 && mayRest)
    {
        Order resting = order;
        resting.quantity = remaining;
        resting.status = (filled > 0) ? OrderStatus::PartiallyFilled : OrderStatus::New;
        matcher_.addOrder(resting);
        activeOrderIds_.insert(order.id);
        trackResting(resting);
    }

    Order snapshot = order;
    snapshot.quantity = remaining;
    if (remaining == 0)
        snapshot.status = OrderStatus::Filled;
    else if (!mayRest)
        snapshot.status = (filled > 0) ? OrderStatus::Filled : OrderStatus::Cancelled;
    else
        snapshot.status = (filled > 0) ? OrderStatus::PartiallyFilled : OrderStatus::New;
    orders_[order.id] = snapshot;

    return trades;
}

std::optional<OrderStatus> OrderManager::statusOf(OrderId id) const
{
    auto it = orders_.find(id);
    if (it == orders_.end())
        return std::nullopt;
    return it->second.status;
}

std::optional<Order> OrderManager::find(OrderId id) const
{
    auto it = orders_.find(id);
    if (it == orders_.end())
        return std::nullopt;
    return it->second;
}

}  // namespace titan
