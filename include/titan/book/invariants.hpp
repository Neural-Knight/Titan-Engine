#pragma once

#include <string>
#include <vector>

#include "titan/book/i_matcher.hpp"
#include "titan/exchange/order_manager.hpp"

namespace titan {

struct InvariantViolation {
    std::string name;
    std::string detail;
};

// Checks OrderTableSizeMatchesRestingCount, OrderTableLocationsAccurate,
// NoZeroQuantityRestingOrders, OrderPriceMatchesLevel, PriceLevelsOrdered, NoCrossedBookAtRest.
std::vector<InvariantViolation> checkInvariants(const IMatcher& matcher);

// checkInvariants(matcher) plus StatusConsistentWithActive,
// ActiveIdsSubsetOfOrderTable, ShadowQueueMatchesMatcherFifo.
std::vector<InvariantViolation> checkOrderManagerInvariants(const OrderManager& manager, const IMatcher& matcher);

}  // namespace titan
