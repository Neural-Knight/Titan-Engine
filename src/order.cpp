#include "order.h"
#include <limits>
#include <algorithm>
void OrderBook::addOrder(const Order &order)
{
    OrderLocation location{
        .side = order.side,
        .price = order.price,
    };
    if (order.side == Side::Buy)
    {
        auto &level = bids[order.price];
        level.push_back(order);

        auto it = std::prev(level.end());
        location.it = it;
    }
    else
    {
        auto &level = asks[order.price];
        level.push_back(order);

        auto it = std::prev(level.end());
        location.it = it;
    }
    orderTable[order.id] = location;
}
void OrderBook::removeOrder(OrderId id)
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

void OrderBook::cancelOrder(OrderId id)
{
    removeOrder(id);
}

const PriceLevels &OrderBook::getBids() const
{
    return bids;
}

const PriceLevels &OrderBook::getAsks() const
{
    return asks;
}

const std::map<OrderId, OrderLocation> &OrderBook::getOrderTable() const
{
    return orderTable;
}

Price OrderBook::bestBid() const
{
    if (!bids.empty())
        return std::prev(bids.end())->first;
    return std::numeric_limits<Price>::max();
}

Price OrderBook::bestAsk() const
{
    if (!asks.empty())
        return asks.begin()->first;
    return 0;
}

std::vector<Trade> OrderBook::matchOrder(Order incomingOrder)
{
    std::vector<Trade> trades;
    if (incomingOrder.side == Side::Buy)
    {
        while (incomingOrder.quantity > 0 && incomingOrder.price >= bestAsk() && !asks.empty())
        {
            Order& restingOrder = asks[bestAsk()].front();
            Quantity traded = std::min(incomingOrder.quantity, restingOrder.quantity);
            trades.push_back(Trade{incomingOrder.id, restingOrder.id, restingOrder.price, traded});
            restingOrder.quantity -= traded;
            incomingOrder.quantity -= traded;
            if (restingOrder.quantity == 0)
                removeOrder(restingOrder.id);
        }
        if (incomingOrder.quantity > 0)
            addOrder(incomingOrder);
    }
    else
    {
        while (incomingOrder.quantity > 0 && incomingOrder.price <= bestBid() && !bids.empty())
        {
            Order& restingOrder = bids[bestBid()].front();
            Quantity traded = std::min(incomingOrder.quantity, restingOrder.quantity);
            trades.push_back(Trade{incomingOrder.id, restingOrder.id, restingOrder.price, traded});
            restingOrder.quantity -= traded;
            incomingOrder.quantity -= traded;
            if (restingOrder.quantity == 0)
                removeOrder(restingOrder.id);
        }
        if (incomingOrder.quantity > 0)
            addOrder(incomingOrder);
    }
    return trades;
}