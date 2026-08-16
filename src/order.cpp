#include "order.h"

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

void OrderBook::cancelOrder(OrderId id)
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

const PriceLevels& OrderBook::getBids() const {
    return bids;
}

const PriceLevels& OrderBook::getAsks() const {
    return asks;
}

const std::map<OrderId, OrderLocation>& OrderBook::getOrderTable() const{
    return orderTable;
}

Price OrderBook::bestBid() const {
    
}

Price OrderBook::bestAsk() const {
    
}