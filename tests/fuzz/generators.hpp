#pragma once

#include <random>

#include "titan/core/types.hpp"

// Random order/operation generators for the Module 5 fuzz harness. Not a
// production header — lives under tests/fuzz and is included directly by
// fuzz test .cpp files.
namespace titan::fuzz {

// Random order over a small, deliberately-collision-prone space so
// crossing/FIFO/priority logic actually gets exercised: price in [1,200],
// quantity in [1,1000]. `id` is the caller's choice (usually a monotonic
// counter) so callers control uniqueness/duplication.
//
// A few low-probability mutations are layered on top specifically to
// exercise validation-rejection paths, which a purely "always valid"
// generator would never touch:
//   - ~5% zero quantity              (-> RejectReason::ZeroQuantity)
//   - ~5% zero price on a Limit order (-> RejectReason::ZeroPrice)
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

// Weighted towards Add so the book actually stays populated enough for
// Match/Cancel/CancelReplace to have something to act on.
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
