#pragma once

#include <vector>

#include "titan/core/types.hpp"

namespace titan {

// IMatcher — single-symbol order-book matching engine contract.
class IMatcher {
public:
    virtual ~IMatcher() = default;

    // Crosses first, rests only the non-crossing remainder. FIFO per level.
    virtual void addOrder(const Order& order) = 0;
    // No-op if id is unknown or already filled.
    virtual void cancelOrder(OrderId id) = 0;
    // Same crossing behavior as addOrder, but returns the trades produced.
    virtual std::vector<Trade> matchOrder(Order incoming) = 0;

    // Top-of-book price; empty-side sentinel is implementation-defined.
    virtual Price bestBid() const = 0;
    virtual Price bestAsk() const = 0;
};

}  // namespace titan
