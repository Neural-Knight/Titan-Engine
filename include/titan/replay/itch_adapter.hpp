#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "titan/exchange/instrument_registry.hpp"
#include "titan/feed/itch/messages.hpp"

namespace titan {

// Engine-side counterpart to ItchBookBuilder: applies decoded ITCH messages
// onto InstrumentRegistry so the exchange book tracks the same resting state.
class ItchEngineAdapter {
public:
    explicit ItchEngineAdapter(InstrumentRegistry& registry);

    // False if the message was ignored (no prior R for its locate, or the
    // referenced order isn't resting).
    bool apply(const ItchMessage& message);

    std::optional<std::string> symbolForLocate(StockLocate locate) const;
    BookSnapshot snapshot(StockLocate locate, size_t depth) const;

    // Sum of trade quantities from synthetic E/C executions.
    uint64_t tradeVolume() const { return tradeVolume_; }

    void reset();

private:
    bool handle(const SystemEventMessage& message);
    bool handle(const StockDirectoryMessage& message);
    bool handle(const AddOrderMessage& message);
    bool handle(const AddOrderMpidMessage& message);
    bool handle(const OrderExecutedMessage& message);
    bool handle(const OrderExecutedWithPriceMessage& message);
    bool handle(const OrderCancelMessage& message);
    bool handle(const OrderDeleteMessage& message);
    bool handle(const OrderReplaceMessage& message);
    bool handle(const TimestampSecondsMessage& message);

    bool applyExecuted(StockLocate locate, OrderRefNumber ref, uint32_t executedShares,
                        std::optional<uint32_t> executionPrice);

    // ITCH ref -> engine OrderId. cancelReplace is same-id only, so a
    // replaced order keeps its old id; new refs remap onto it. See itch-notes.md.
    OrderId engineIdFor(OrderRefNumber ref) const;

    InstrumentRegistry& registry_;
    std::unordered_map<StockLocate, std::string> symbols_;
    std::unordered_map<OrderRefNumber, OrderId> engineIdForRef_;
    OrderId nextSyntheticId_{1'000'000'000};
    uint64_t tradeVolume_{0};
};

}  // namespace titan
