#pragma once

#include <cstddef>
#include <map>
#include <unordered_map>
#include <vector>

#include "titan/book/i_matcher.hpp"
#include "titan/core/memory_pool.hpp"

namespace titan {

// Drop-in IMatcher with identical price-time-priority semantics to
// ReferenceMatcher. See design note at the top of optimized_matcher.cpp.
class OptimizedMatcher : public IMatcher {
public:
    void addOrder(const Order& order) override;
    void cancelOrder(OrderId id) override;
    std::vector<Trade> matchOrder(Order incoming) override;

    Price bestBid() const override;
    Price bestAsk() const override;

    std::vector<PriceLevel> bidDepth(size_t maxLevels) const override;
    std::vector<PriceLevel> askDepth(size_t maxLevels) const override;

private:
    // Append-only; canceled/filled orders release their pool slot and become
    // nullptr tombstones, so indices into `orders` never move.
    struct Level {
        std::vector<Order*> orders;
        size_t frontIndex{0};
        size_t liveCount{0};
    };
    struct OrderRef {
        Side side;
        Price price;
        size_t index;
    };
    using Levels = std::map<Price, Level>;

    void restOrder(const Order& order);
    void removeOrder(OrderId id);
    Order* frontLive(Level& level);
    std::vector<Trade> cross(Order& incoming);
    std::vector<PriceLevel> depthFrom(const Levels& levels, bool reverse, size_t maxLevels) const;

    Levels bids_;
    Levels asks_;
    std::unordered_map<OrderId, OrderRef> orderTable_;
    FixedObjectPool<Order> orderPool_;
};

}  // namespace titan
