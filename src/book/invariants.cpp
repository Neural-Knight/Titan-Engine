#include "titan/book/invariants.hpp"

#include "reference/order_book.hpp"

namespace titan {

namespace {

// Deep structural checks, only possible against the concrete ReferenceMatcher.
void checkReferenceMatcherInvariants(const ReferenceMatcher& matcher,
                                      std::vector<InvariantViolation>& violations)
{
    const PriceLevels& bids = matcher.getBids();
    const PriceLevels& asks = matcher.getAsks();
    const auto& orderTable = matcher.getOrderTable();

    std::size_t restingCount = 0;

    auto walkLevels = [&](const PriceLevels& levels, const char* sideName) {
        bool first = true;
        Price previous = 0;
        for (const auto& levelEntry : levels)
        {
            const Price price = levelEntry.first;
            const OrderList& orders = levelEntry.second;

            if (!first && price <= previous)
            {
                violations.push_back({"PriceLevelsOrdered",
                    std::string(sideName) + " price levels not strictly ordered at " +
                        std::to_string(price)});
            }
            previous = price;
            first = false;

            for (const Order& order : orders)
            {
                ++restingCount;

                if (order.quantity == 0)
                {
                    violations.push_back({"NoZeroQuantityRestingOrders",
                        std::string(sideName) + " order " + std::to_string(order.id) +
                            " has zero quantity while resting"});
                }

                if (order.price != price)
                {
                    violations.push_back({"OrderPriceMatchesLevel",
                        std::string(sideName) + " order " + std::to_string(order.id) +
                            " resting at level " + std::to_string(price) +
                            " but order.price=" + std::to_string(order.price)});
                }
            }
        }
    };
    walkLevels(bids, "bid");
    walkLevels(asks, "ask");

    if (restingCount != orderTable.size())
    {
        violations.push_back({"OrderTableSizeMatchesRestingCount",
            "orderTable size=" + std::to_string(orderTable.size()) +
                " but walked resting-order count=" + std::to_string(restingCount)});
    }

    for (const auto& entry : orderTable)
    {
        const OrderId id = entry.first;
        const OrderLocation& location = entry.second;
        if (location.it->id != id)
        {
            violations.push_back({"OrderTableLocationsAccurate",
                "orderTable[" + std::to_string(id) + "] points at order id " +
                    std::to_string(location.it->id)});
        }
    }

    if (!bids.empty() && !asks.empty())
    {
        const Price bid = matcher.bestBid();
        const Price ask = matcher.bestAsk();
        if (!(bid < ask))
        {
            violations.push_back({"NoCrossedBookAtRest",
                "bestBid=" + std::to_string(bid) + " bestAsk=" + std::to_string(ask)});
        }
    }
}

}  // namespace

std::vector<InvariantViolation> checkInvariants(const IMatcher& matcher)
{
    std::vector<InvariantViolation> violations;

    if (const auto* reference = dynamic_cast<const ReferenceMatcher*>(&matcher))
        checkReferenceMatcherInvariants(*reference, violations);

    return violations;
}

std::vector<InvariantViolation> checkOrderManagerInvariants(const OrderManager& manager, const IMatcher& matcher)
{
    std::vector<InvariantViolation> violations = checkInvariants(matcher);

    const auto* reference = dynamic_cast<const ReferenceMatcher*>(&matcher);

    for (OrderId id : manager.activeOrderIdsForInvariants())
    {
        const auto status = manager.statusOf(id);
        if (!status || (*status != OrderStatus::New && *status != OrderStatus::PartiallyFilled))
        {
            violations.push_back({"StatusConsistentWithActive",
                "active order " + std::to_string(id) + " has a non-resting status"});
        }

        if (reference != nullptr && reference->getOrderTable().count(id) == 0)
        {
            violations.push_back({"ActiveIdsSubsetOfOrderTable",
                "active order " + std::to_string(id) + " missing from matcher orderTable"});
        }
    }

    if (reference != nullptr)
    {
        auto checkShadow = [&](const std::map<Price, std::vector<OrderId>>& shadow,
                                const PriceLevels& real, const char* sideName) {
            for (const auto& shadowEntry : shadow)
            {
                const Price price = shadowEntry.first;
                const std::vector<OrderId>& shadowIds = shadowEntry.second;

                std::vector<OrderId> realIds;
                auto levelIt = real.find(price);
                if (levelIt != real.end())
                    for (const Order& order : levelIt->second)
                        realIds.push_back(order.id);

                if (realIds != shadowIds)
                {
                    violations.push_back({"ShadowQueueMatchesMatcherFifo",
                        std::string(sideName) + " price " + std::to_string(price) +
                            " shadow/matcher FIFO order mismatch"});
                }
            }
        };
        checkShadow(manager.restingBidsForInvariants(), reference->getBids(), "bid");
        checkShadow(manager.restingAsksForInvariants(), reference->getAsks(), "ask");
    }

    return violations;
}

}  // namespace titan
