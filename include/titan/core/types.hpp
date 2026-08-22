#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <unordered_map>

namespace titan {

enum class Side {
    Buy,
    Sell
};

using OrderId = uint64_t;
using Price = uint64_t;
using Quantity = uint64_t;

struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
};

using OrderList = std::list<Order>;
using PriceLevels = std::map<Price, OrderList>;

// Where a resting order lives, so it can be erased in O(1) on cancel/fill
// without re-searching the price level.
struct OrderLocation {
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

}  // namespace titan
