#include "titan/exchange/order_manager.hpp"

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

    const Quantity originalQuantity = order.quantity;
    allKnownOrderIds_.insert(order.id);

    const std::vector<Trade> trades = matcher_.matchOrder(order);

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

    Order snapshot = order;
    snapshot.quantity = remaining;
    if (remaining == 0)
    {
        snapshot.status = OrderStatus::Filled;
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
