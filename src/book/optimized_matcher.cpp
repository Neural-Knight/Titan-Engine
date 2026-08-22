// Design: same std::map<Price, Level> as ReferenceMatcher (so best-of-book
// and iteration order are identical), but each Level is a flat vector with
// lazy tombstoning on cancel/fill instead of a linked list. This avoids a
// heap node per order and keeps a level's live orders contiguous, at the
// cost of unbounded per-level memory growth (no compaction) -- fine for
// bounded benchmark/test runs, not attempted here (Module 13's concern).
#include "titan/book/optimized_matcher.hpp"

#include <algorithm>
#include <limits>

namespace titan {

void OptimizedMatcher::restOrder(const Order& order)
{
    Levels& levels = (order.side == Side::Buy) ? bids_ : asks_;
    Level& level = levels[order.price];
    const size_t index = level.orders.size();
    level.orders.push_back(order);
    ++level.liveCount;
    orderTable_[order.id] = OrderRef{order.side, order.price, index};
}

void OptimizedMatcher::removeOrder(OrderId id)
{
    const auto it = orderTable_.find(id);
    if (it == orderTable_.end())
        return;

    const OrderRef ref = it->second;
    Levels& levels = (ref.side == Side::Buy) ? bids_ : asks_;
    const auto levelIt = levels.find(ref.price);

    Level& level = levelIt->second;
    level.orders[ref.index].quantity = 0;
    --level.liveCount;
    orderTable_.erase(it);

    if (level.liveCount == 0)
        levels.erase(levelIt);
}

// Skips tombstones, advancing frontIndex; nullptr if the level has no live orders left.
Order* OptimizedMatcher::frontLive(Level& level)
{
    while (level.frontIndex < level.orders.size() && level.orders[level.frontIndex].quantity == 0)
        ++level.frontIndex;
    return (level.frontIndex < level.orders.size()) ? &level.orders[level.frontIndex] : nullptr;
}

std::vector<Trade> OptimizedMatcher::cross(Order& incoming)
{
    std::vector<Trade> trades;
    Levels& opposite = (incoming.side == Side::Buy) ? asks_ : bids_;

    while (incoming.quantity > 0 && !opposite.empty())
    {
        const auto levelIt = (incoming.side == Side::Buy) ? opposite.begin() : std::prev(opposite.end());
        const Price levelPrice = levelIt->first;
        const bool crosses =
            (incoming.side == Side::Buy) ? (incoming.price >= levelPrice) : (incoming.price <= levelPrice);
        if (!crosses)
            break;

        Order* resting = frontLive(levelIt->second);
        if (!resting)
        {
            opposite.erase(levelIt);
            continue;
        }

        const Quantity traded = std::min(incoming.quantity, resting->quantity);
        trades.push_back(Trade{incoming.id, resting->id, resting->price, traded});
        resting->quantity -= traded;
        incoming.quantity -= traded;
        if (resting->quantity == 0)
            removeOrder(resting->id);
    }
    return trades;
}

void OptimizedMatcher::addOrder(const Order& order)
{
    Order incoming = order;
    cross(incoming);
    if (incoming.quantity > 0)
        restOrder(incoming);
}

void OptimizedMatcher::cancelOrder(OrderId id)
{
    removeOrder(id);
}

Price OptimizedMatcher::bestBid() const
{
    if (!bids_.empty())
        return std::prev(bids_.end())->first;
    return std::numeric_limits<Price>::max();
}

Price OptimizedMatcher::bestAsk() const
{
    if (!asks_.empty())
        return asks_.begin()->first;
    return 0;
}

std::vector<PriceLevel> OptimizedMatcher::depthFrom(const Levels& levels, bool reverse, size_t maxLevels) const
{
    std::vector<PriceLevel> result;
    auto append = [&](Price price, const Level& level) {
        if (result.size() >= maxLevels)
            return;
        Quantity total = 0;
        for (const Order& order : level.orders)
            total += order.quantity;  // tombstones are quantity==0, contribute nothing
        result.push_back(PriceLevel{price, total});
    };

    if (reverse)
        for (auto it = levels.rbegin(); it != levels.rend() && result.size() < maxLevels; ++it)
            append(it->first, it->second);
    else
        for (auto it = levels.begin(); it != levels.end() && result.size() < maxLevels; ++it)
            append(it->first, it->second);
    return result;
}

std::vector<PriceLevel> OptimizedMatcher::bidDepth(size_t maxLevels) const
{
    return depthFrom(bids_, true, maxLevels);
}

std::vector<PriceLevel> OptimizedMatcher::askDepth(size_t maxLevels) const
{
    return depthFrom(asks_, false, maxLevels);
}

std::vector<Trade> OptimizedMatcher::matchOrder(Order incomingOrder)
{
    std::vector<Trade> trades = cross(incomingOrder);
    if (incomingOrder.quantity > 0)
        restOrder(incomingOrder);
    return trades;
}

}  // namespace titan
