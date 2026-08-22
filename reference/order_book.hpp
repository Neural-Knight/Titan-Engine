#pragma once

#include <unordered_map>

#include "titan/book/i_matcher.hpp"

namespace titan {

// ReferenceMatcher — the correctness oracle. Price-time-priority book.
// Do not optimize; optimized implementations are checked against this.
class ReferenceMatcher : public IMatcher {
public:
    void addOrder(const Order& order) override;
    void cancelOrder(OrderId id) override;
    std::vector<Trade> matchOrder(Order incoming) override;

    Price bestBid() const override;
    Price bestAsk() const override;

    std::vector<PriceLevel> bidDepth(size_t maxLevels) const override;
    std::vector<PriceLevel> askDepth(size_t maxLevels) const override;

    // Test/tool introspection only, not part of IMatcher.
    const PriceLevels& getBids() const;
    const PriceLevels& getAsks() const;
    const std::unordered_map<OrderId, OrderLocation>& getOrderTable() const;

private:
    PriceLevels bids;
    PriceLevels asks;
    std::unordered_map<OrderId, OrderLocation> orderTable;

    void removeOrder(OrderId id);
    void restOrder(const Order& order);
    std::vector<Trade> cross(Order& incoming);
};

}  // namespace titan
