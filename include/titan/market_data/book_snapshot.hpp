#pragma once

#include <cstdint>
#include <vector>

#include "titan/core/types.hpp"

namespace titan {

// Aggregated resting quantity at one price level.
struct PriceLevel {
    Price price;
    Quantity quantity;

    bool operator==(const PriceLevel&) const = default;
};

// bids best-first descending, asks best-first ascending.
struct BookSnapshot {
    Symbol symbol;
    uint64_t sequenceNumber;
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;

    bool operator==(const BookSnapshot&) const = default;
};

}  // namespace titan
