#ifndef ORDER_H
#define ORDER_H

#include <cstdint>
#include <map>
#include <list>
#include <vector>

enum class Side
{
    Buy,
    Sell
};

using OrderId = uint64_t;
using Price = uint64_t;
using Quantity = uint64_t;

struct Order
{
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
};

using OrderList = std::list<Order>;
using PriceLevels = std::map<Price, OrderList>;

struct OrderLocation
{
    Side side;
    Price price;
    OrderList::iterator it;
};

struct Trade {
    OrderId incomingOrderId;
    OrderId restingOrderId;
    Price price;
    Quantity quantity;
};

class OrderBook
{
private:
    PriceLevels bids;
    PriceLevels asks;
    std::map<OrderId, OrderLocation> orderTable;
    void removeOrder(OrderId id);

public:
    void addOrder(const Order &order);
    void cancelOrder(OrderId id);

    const PriceLevels &getBids() const;
    const PriceLevels &getAsks() const;
    const std::map<OrderId, OrderLocation> &getOrderTable() const;

    Price bestBid() const;
    Price bestAsk() const;

    std::vector<Trade> matchOrder(Order incoming);
};

#endif
