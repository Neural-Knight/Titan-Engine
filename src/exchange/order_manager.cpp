#include "titan/exchange/order_manager.hpp"

#include <limits>

#include "titan/exchange/validator.hpp"

namespace titan {

OrderManager::OrderManager(IMatcher& matcher) : matcher_(matcher) {}

AcceptResult OrderManager::addOrder(Order order)
{
    const RejectReason reason = validateNewOrder(order, allKnownOrderIds_);
    if (reason != RejectReason::None)
        return AcceptResult{false, reason};

    order.status = OrderStatus::New;
    matcher_.addOrder(order);

    allKnownOrderIds_.insert(order.id);
    activeOrderIds_.insert(order.id);
    orders_[order.id] = order;

    return AcceptResult{true, RejectReason::None};
}

AcceptResult OrderManager::cancelOrder(OrderId id)
{
    const RejectReason reason = validateCancel(id, activeOrderIds_);
    if (reason != RejectReason::None)
        return AcceptResult{false, reason};

    matcher_.cancelOrder(id);

    activeOrderIds_.erase(id);
    orders_.at(id).status = OrderStatus::Cancelled;

    return AcceptResult{true, RejectReason::None};
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

    // ReferenceMatcher::matchOrder has no price limit concept for "sweep
    // everything" — a Market order gets one synthesized by pointing its
    // price at the extreme end of the book, so the existing crossing
    // condition (incoming.price >= bestAsk() / <= bestBid()) is trivially
    // true at every level. IOC Limit crosses at its own price, unchanged.
    Order toMatch = order;
    if (isMarket)
        toMatch.price = (order.side == Side::Buy) ? std::numeric_limits<Price>::max() : Price{0};

    const Quantity originalQuantity = order.quantity;
    allKnownOrderIds_.insert(order.id);

    const std::vector<Trade> trades = matcher_.matchOrder(toMatch);

    Quantity filled = 0;
    for (const Trade& trade : trades)
    {
        filled += trade.quantity;

        auto restingIt = orders_.find(trade.restingOrderId);
        if (restingIt == orders_.end())
            continue;

        Order& resting = restingIt->second;
        resting.quantity -= trade.quantity;
        if (resting.quantity == 0)
        {
            resting.status = OrderStatus::Filled;
            activeOrderIds_.erase(trade.restingOrderId);
        }
        else
        {
            resting.status = OrderStatus::PartiallyFilled;
        }
    }

    const Quantity remaining = originalQuantity - filled;

    // ReferenceMatcher::matchOrder unconditionally rests any leftover
    // quantity under `order.id`. For Market/IOC that must never be
    // observable, so remove it immediately — safe because this all
    // happens synchronously within this call, before any other order can
    // reach the book.
    if (remaining > 0 && !mayRest)
        matcher_.cancelOrder(order.id);

    Order snapshot = order;
    snapshot.quantity = remaining;
    if (remaining == 0)
    {
        snapshot.status = OrderStatus::Filled;
    }
    else if (!mayRest)
    {
        snapshot.status = (filled > 0) ? OrderStatus::Filled : OrderStatus::Cancelled;
    }
    else if (filled > 0)
    {
        snapshot.status = OrderStatus::PartiallyFilled;
        activeOrderIds_.insert(order.id);
    }
    else
    {
        snapshot.status = OrderStatus::New;
        activeOrderIds_.insert(order.id);
    }
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
