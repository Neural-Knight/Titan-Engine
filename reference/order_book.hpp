#pragma once

#include <unordered_map>

#include "titan/book/i_matcher.hpp"

namespace titan {

// ReferenceMatcher — the correctness oracle. Simple std::map/std::list
// price-time-priority book. Do not optimize; this is what optimized
// implementations are checked against (see Module 12 parity tests).
class ReferenceMatcher : public IMatcher {
public:
    void addOrder(const Order& order) override;
    void cancelOrder(OrderId id) override;
    std::vector<Trade> matchOrder(Order incoming) override;

    Price bestBid() const override;
    Price bestAsk() const override;

    // Introspection helpers for tests/tools. Not part of IMatcher, since
    // exposing raw internal containers isn't a contract every matcher
    // implementation should be forced to provide.
    const PriceLevels& getBids() const;
    const PriceLevels& getAsks() const;
    const std::unordered_map<OrderId, OrderLocation>& getOrderTable() const;

private:
    PriceLevels bids;
    PriceLevels asks;
    std::unordered_map<OrderId, OrderLocation> orderTable;

    void removeOrder(OrderId id);
};

}  // namespace titan
