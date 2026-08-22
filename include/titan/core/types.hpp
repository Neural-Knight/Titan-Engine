#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <string>
#include <unordered_map>

#include "titan/core/order_types.hpp"

namespace titan {

enum class Side {
    Buy,
    Sell
};

using OrderId = uint64_t;
using Price = uint64_t;
using Quantity = uint64_t;

// Plain alias, not a wrapper: symbols are opaque strings to every layer here.
using Symbol = std::string;

// accountId/type/tif/status are defaulted so existing 4-field aggregate
// inits (`Order{1, Side::Buy, 100, 10}`) keep compiling unchanged.
struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
    AccountId accountId{};
    OrderType type{OrderType::Limit};
    TimeInForce tif{TimeInForce::GTC};
    OrderStatus status{OrderStatus::New};
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
