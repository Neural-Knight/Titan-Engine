#include "reference/order_book.hpp"

#include <algorithm>
#include <limits>

namespace titan {

// Unconditional rest, no crossing check. Only safe to call once the
// caller has already established the order won't cross.
void ReferenceMatcher::restOrder(const Order& order)
{
    OrderLocation location{
        .side = order.side,
        .price = order.price,
    };
    if (order.side == Side::Buy)
    {
        auto &level = bids[order.price];
        level.push_back(order);
        location.it = std::prev(level.end());
    }
    else
    {
        auto &level = asks[order.price];
        level.push_back(order);
        location.it = std::prev(level.end());
    }
    orderTable[order.id] = location;
}

// Matches `incoming` against the opposite side, mutating its quantity.
std::vector<Trade> ReferenceMatcher::cross(Order& incoming)
{
    std::vector<Trade> trades;
    if (incoming.side == Side::Buy)
    {
        while (incoming.quantity > 0 && incoming.price >= bestAsk() && !asks.empty())
        {
            Order& resting = asks[bestAsk()].front();
            Quantity traded = std::min(incoming.quantity, resting.quantity);
            trades.push_back(Trade{incoming.id, resting.id, resting.price, traded});
            resting.quantity -= traded;
            incoming.quantity -= traded;
            if (resting.quantity == 0)
                removeOrder(resting.id);
        }
    }
    else
    {
        while (incoming.quantity > 0 && incoming.price <= bestBid() && !bids.empty())
        {
            Order& resting = bids[bestBid()].front();
            Quantity traded = std::min(incoming.quantity, resting.quantity);
            trades.push_back(Trade{incoming.id, resting.id, resting.price, traded});
            resting.quantity -= traded;
            incoming.quantity -= traded;
            if (resting.quantity == 0)
                removeOrder(resting.id);
        }
    }
    return trades;
}

// Limit order entry: crosses first, rests only the non-crossing remainder.
void ReferenceMatcher::addOrder(const Order& order)
{
    Order incoming = order;
    cross(incoming);
    if (incoming.quantity > 0)
        restOrder(incoming);
}

void ReferenceMatcher::removeOrder(OrderId id)
{
    auto it = orderTable.find(id);
    if (it == orderTable.end())
    {
        return;
    }
    auto &location = it->second;
    if (location.side == Side::Buy)
    {
        auto levelIt = bids.find(location.price);
        levelIt->second.erase(location.it);
        if (levelIt->second.empty())
        {
            bids.erase(levelIt);
        }
    }
    else
    {
        auto levelIt = asks.find(location.price);
        levelIt->second.erase(location.it);
        if (levelIt->second.empty())
        {
            asks.erase(levelIt);
        }
    }
    orderTable.erase(id);
}

void ReferenceMatcher::cancelOrder(OrderId id)
{
    removeOrder(id);
}

const PriceLevels& ReferenceMatcher::getBids() const
{
    return bids;
}

const PriceLevels& ReferenceMatcher::getAsks() const
{
    return asks;
}

const std::unordered_map<OrderId, OrderLocation>& ReferenceMatcher::getOrderTable() const
{
    return orderTable;
}

Price ReferenceMatcher::bestBid() const
{
    if (!bids.empty())
        return std::prev(bids.end())->first;
    return std::numeric_limits<Price>::max();
}

Price ReferenceMatcher::bestAsk() const
{
    if (!asks.empty())
        return asks.begin()->first;
    return 0;
}

std::vector<Trade> ReferenceMatcher::matchOrder(Order incomingOrder)
{
    std::vector<Trade> trades = cross(incomingOrder);
    if (incomingOrder.quantity > 0)
        restOrder(incomingOrder);
    return trades;
}

}  // namespace titan
