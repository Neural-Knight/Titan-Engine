#pragma once

#include <random>

#include "titan/core/types.hpp"

// Random order/operation generators for the fuzz harness.
namespace titan::fuzz {

// Small, collision-prone space (price 1-200, qty 1-1000) so crossing/FIFO
// logic gets exercised. ~5% zero quantity, ~5% zero price, to hit rejects too.
inline Order randomOrder(std::mt19937& rng, OrderId id)
{
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<uint64_t> priceDist(1, 200);
    std::uniform_int_distribution<uint64_t> quantityDist(1, 1000);
    std::uniform_int_distribution<int> typeDist(0, 9);      // 80% Limit, 20% Market
    std::uniform_int_distribution<int> tifDist(0, 1);       // GTC / IOC
    std::uniform_int_distribution<int> accountBucketDist(0, 1);
    std::uniform_int_distribution<uint64_t> accountDist(1, 10);
    std::uniform_int_distribution<int> mutationDist(0, 99);

    Order order{};
    order.id = id;
    order.side = sideDist(rng) == 0 ? Side::Buy : Side::Sell;
    order.price = priceDist(rng);
    order.quantity = quantityDist(rng);
    order.type = typeDist(rng) < 8 ? OrderType::Limit : OrderType::Market;
    order.tif = tifDist(rng) == 0 ? TimeInForce::GTC : TimeInForce::IOC;
    order.accountId = accountBucketDist(rng) == 0 ? AccountId{0} : accountDist(rng);
    order.status = OrderStatus::New;

    if (mutationDist(rng) < 5)
        order.quantity = 0;
    if (order.type == OrderType::Limit && mutationDist(rng) < 5)
        order.price = 0;

    return order;
}

enum class FuzzOp {
    Add,
    Match,
    Cancel,
    CancelReplace
};

// Weighted towards Add so the book stays populated for the other ops.
inline FuzzOp randomOp(std::mt19937& rng)
{
    std::uniform_int_distribution<int> dist(0, 99);
    const int r = dist(rng);
    if (r < 40) return FuzzOp::Add;            // 40%
    if (r < 70) return FuzzOp::Match;          // 30%
    if (r < 85) return FuzzOp::Cancel;         // 15%
    return FuzzOp::CancelReplace;               // 15%
}

}  // namespace titan::fuzz
