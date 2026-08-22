#pragma once

#include <vector>

#include "titan/core/types.hpp"

namespace titan {

// IMatcher — contract for a single-symbol order-book matching engine.
//
// Expected semantics (must hold for every implementation):
//
//  - addOrder(order): rests `order` unconditionally at its price level.
//    Orders at the same price level maintain FIFO (time priority) order —
//    the order added first is matched first. Does not attempt to cross.
//
//  - cancelOrder(id): removes a resting order by id. Cancelling an unknown
//    or already-filled id is a no-op; it must not corrupt book state.
//
//  - matchOrder(incoming): attempts to fill `incoming` against the
//    opposite side of the book wherever prices cross — a Buy crosses
//    while incoming.price >= bestAsk(), a Sell crosses while
//    incoming.price <= bestBid(). Resting orders are consumed in strict
//    price-time priority: best price first, then FIFO within that price
//    level. Every trade prints at the RESTING order's price. Any
//    quantity of `incoming` left unfilled after crossing is rested via
//    addOrder(). Returns the trades produced, in the order they occurred.
//
//  - bestBid()/bestAsk(): top-of-book price on each side. Each
//    implementation must define a sentinel for "side is empty"; the
//    reference implementation returns numeric_limits<Price>::max() for an
//    empty bid side and 0 for an empty ask side.
class IMatcher {
public:
    virtual ~IMatcher() = default;

    virtual void addOrder(const Order& order) = 0;
    virtual void cancelOrder(OrderId id) = 0;
    virtual std::vector<Trade> matchOrder(Order incoming) = 0;

    virtual Price bestBid() const = 0;
    virtual Price bestAsk() const = 0;
};

}  // namespace titan
