#include "titan/exchange/order_manager.hpp"

#include <algorithm>
#include <limits>

#include "titan/exchange/validator.hpp"

namespace titan {

OrderManager::OrderManager(IMatcher& matcher) : matcher_(matcher) {}

// (side, price) -> FIFO order-id mirror of the book.
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

bool OrderManager::wouldCross(const Order& order) const
{
    const auto& opposite = (order.side == Side::Buy) ? restingAsksAt_ : restingBidsAt_;
    if (opposite.empty())
        return false;
    return (order.side == Side::Buy)
        ? (order.price >= opposite.begin()->first)
        : (order.price <= std::prev(opposite.end())->first);
}

AcceptResult OrderManager::addOrder(Order order)
{
    if (order.type == OrderType::Market ||
        (order.type == OrderType::Limit && order.tif == TimeInForce::IOC))
        return AcceptResult{false, RejectReason::InvalidOrderType};

    const RejectReason reason = validateNewOrder(order, allKnownOrderIds_);
    if (reason != RejectReason::None)
        return AcceptResult{false, reason};

    // matchOrder() handles crossing and non-crossing GTC limits identically.
    matchOrder(order);
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
    if (activeOrderIds_.count(oldId) == 0)
        return AcceptResult{false, RejectReason::OrderNotResting};

    // Same-id amend only.
    if (newOrder.id != oldId)
        return AcceptResult{false, RejectReason::InvalidReplace};

    if (newOrder.type == OrderType::Market ||
        (newOrder.type == OrderType::Limit && newOrder.tif == TimeInForce::IOC))
        return AcceptResult{false, RejectReason::InvalidReplace};

    // Exclude oldId from the duplicate-id check: it's being replaced, not re-added.
    std::unordered_set<OrderId> idsExcludingOld = allKnownOrderIds_;
    idsExcludingOld.erase(oldId);
    const RejectReason reason = validateNewOrder(newOrder, idsExcludingOld);
    if (reason != RejectReason::None)
        return AcceptResult{false, reason};

    // cancelReplace only rests, never trades -- a crossing replacement is rejected.
    if (wouldCross(newOrder))
        return AcceptResult{false, RejectReason::ReplaceWouldCross};

    const Order existing = orders_.at(oldId);
    const bool samePrice = (newOrder.price == existing.price);
    const bool preservesPriority = samePrice && (newOrder.quantity <= existing.quantity);

    newOrder.status = OrderStatus::New;

    if (preservesPriority)
    {
        // Quantity decrease at the same price: rebuild the level in its
        // recorded FIFO order so every other order keeps its exact slot.
        auto& levels = (newOrder.side == Side::Buy) ? restingBidsAt_ : restingAsksAt_;
        const std::vector<OrderId>& ids = levels[newOrder.price];

        orders_[oldId] = newOrder;

        for (OrderId id : ids)
            matcher_.cancelOrder(id);
        for (OrderId id : ids)
            matcher_.addOrder(orders_.at(id));

        return AcceptResult{true, RejectReason::None};
    }

    // Price change, or quantity increase at the same price: back of the new level.
    matcher_.cancelOrder(oldId);
    untrackResting(oldId);

    matcher_.addOrder(newOrder);
    orders_[oldId] = newOrder;
    activeOrderIds_.insert(oldId);
    trackResting(newOrder);

    return AcceptResult{true, RejectReason::None};
}

std::vector<Trade> OrderManager::matchWithStp(const Order& incoming, Quantity& filled)
{
    std::vector<Trade> allTrades;
    filled = 0;

    Quantity remaining = incoming.quantity;
    const bool buy = (incoming.side == Side::Buy);
    auto& levels = buy ? restingAsksAt_ : restingBidsAt_;

    while (remaining > 0)
    {
        if (levels.empty())
            break;

        auto levelIt = buy ? levels.begin() : std::prev(levels.end());
        const Price levelPrice = levelIt->first;

        const bool crosses = buy ? (incoming.price >= levelPrice) : (incoming.price <= levelPrice);
        if (!crosses)
            break;

        std::vector<OrderId>& ids = levelIt->second;
        if (ids.empty())
        {
            levels.erase(levelIt);
            continue;
        }

        const OrderId frontId = ids.front();
        const Order& resting = orders_.at(frontId);

        // Stop entirely at the first same-account collision.
        if (incoming.accountId != 0 && resting.accountId == incoming.accountId)
            break;

        // Probe sized to the smaller side always fully consumes itself in
        // one trade against `resting`, so nothing partial is left to rest.
        const Quantity chunk = std::min(remaining, resting.quantity);

        Order probe = incoming;
        probe.quantity = chunk;

        const std::vector<Trade> trades = matcher_.matchOrder(probe);
        if (trades.empty())
            break;  // shadow desynced from the real book; stop rather than loop forever

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

MatchResult OrderManager::matchOrder(Order order)
{
    const RejectReason reason = validateNewOrder(order, allKnownOrderIds_);
    if (reason != RejectReason::None)
        return MatchResult{{}, AcceptResult{false, reason}};

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

    // A GTC remainder only rests if it wouldn't cross; STP-blocked crossing
    // remainders are cancelled instead.
    const bool rests = mayRest && remaining > 0 && !wouldCross(order);

    if (rests)
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
    else if (!rests)
        snapshot.status = (filled > 0) ? OrderStatus::Filled : OrderStatus::Cancelled;
    else
        snapshot.status = (filled > 0) ? OrderStatus::PartiallyFilled : OrderStatus::New;
    orders_[order.id] = snapshot;

    return MatchResult{trades, AcceptResult{true, RejectReason::None}};
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
