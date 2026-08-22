#include "titan/replay/parity_checker.hpp"

#include <type_traits>
#include <variant>

namespace titan {

namespace {

std::optional<StockLocate> extractLocate(const ItchMessage& message)
{
    return std::visit(
        [](const auto& msg) -> std::optional<StockLocate> {
            using T = std::decay_t<decltype(msg)>;
            if constexpr (std::is_same_v<T, TimestampSecondsMessage>)
                return std::nullopt;
            else if constexpr (std::is_same_v<T, AddOrderMpidMessage>)
                return msg.base.stockLocate;
            else if constexpr (std::is_same_v<T, OrderExecutedWithPriceMessage>)
                return msg.base.stockLocate;
            else
                return msg.stockLocate;
        },
        message);
}

uint32_t executedSharesOf(const ItchMessage& message)
{
    if (const auto* executed = std::get_if<OrderExecutedMessage>(&message))
        return executed->executedShares;
    if (const auto* executedWithPrice = std::get_if<OrderExecutedWithPriceMessage>(&message))
        return executedWithPrice->base.executedShares;
    return 0;
}

}  // namespace

ParityChecker::ParityChecker(ItchBookBuilder& builder, ItchEngineAdapter& adapter, size_t checkpointInterval)
    : builder_(builder), adapter_(adapter), checkpointInterval_(checkpointInterval)
{
}

void ParityChecker::process(const ItchMessage& message)
{
    if (const auto locate = extractLocate(message))
        knownLocates_.insert(*locate);

    builder_.apply(message);
    adapter_.apply(message);

    report_.feedTradeVolume += executedSharesOf(message);
    report_.engineTradeVolume = adapter_.tradeVolume();

    ++report_.messagesProcessed;
    if (report_.messagesProcessed % checkpointInterval_ == 0)
        compareAllLocates();
}

void ParityChecker::compareAllLocates()
{
    for (StockLocate locate : knownLocates_)
    {
        const std::optional<BookSnapshot> feedSnap = builder_.snapshot(locate, 1);
        if (!feedSnap)
            continue;  // feed side hasn't registered this locate yet (no R seen)
        const BookSnapshot engineSnap = adapter_.snapshot(locate, 1);

        ++report_.checkpointsCompared;

        const bool bidMatch = (feedSnap->bids.empty() == engineSnap.bids.empty()) &&
            (feedSnap->bids.empty() || feedSnap->bids[0] == engineSnap.bids[0]);
        const bool askMatch = (feedSnap->asks.empty() == engineSnap.asks.empty()) &&
            (feedSnap->asks.empty() || feedSnap->asks[0] == engineSnap.asks[0]);

        if (bidMatch && askMatch)
            continue;

        ++report_.mismatchCount;
        if (report_.firstMismatches.size() < kMaxStoredMismatches)
            report_.firstMismatches.push_back(ParityMismatch{report_.messagesProcessed, *feedSnap, engineSnap});
    }
}

}  // namespace titan
