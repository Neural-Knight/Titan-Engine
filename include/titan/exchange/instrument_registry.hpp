#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "reference/order_book.hpp"
#include "titan/core/events.hpp"
#include "titan/core/types.hpp"
#include "titan/exchange/order_manager.hpp"

namespace titan {

// One ReferenceMatcher + OrderManager per symbol; routes calls and logs events.
class InstrumentRegistry {
public:
    // No-op if the symbol already has an instrument.
    void createInstrument(const Symbol& symbol);
    bool hasInstrument(const Symbol& symbol) const;

    // Each returns/rejects with RejectReason::UnknownSymbol if `symbol` has no instrument.
    AcceptResult submitOrder(const Symbol& symbol, Order order);
    AcceptResult cancelOrder(const Symbol& symbol, OrderId id);
    std::vector<Trade> matchOrder(const Symbol& symbol, Order order);
    AcceptResult cancelReplace(const Symbol& symbol, OrderId oldId, Order newOrder);

    const std::vector<Event>& eventLog() const { return eventLog_; }
    void clearEventLog() { eventLog_.clear(); }

private:
    struct Instrument {
        std::unique_ptr<ReferenceMatcher> matcher;
        std::unique_ptr<OrderManager> manager;
    };

    Instrument* find(const Symbol& symbol);
    void emit(EventPayload payload);

    std::unordered_map<Symbol, Instrument> instruments_;
    std::vector<Event> eventLog_;
    uint64_t nextSequenceNumber_{0};
};

}  // namespace titan
