#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "reference/order_book.hpp"
#include "titan/core/events.hpp"
#include "titan/core/types.hpp"
#include "titan/exchange/order_manager.hpp"
#include "titan/market_data/book_snapshot.hpp"

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

    // Empty bids/asks and sequenceNumber 0 if `symbol` has no instrument.
    BookSnapshot snapshot(const Symbol& symbol, size_t depth) const;

    // nullopt if `symbol` has no instrument or `id` is unknown to it.
    std::optional<Order> findOrder(const Symbol& symbol, OrderId id) const;

    const std::vector<Event>& eventLog() const { return eventLog_; }
    void clearEventLog() { eventLog_.clear(); }

    // Drops every instrument and the event log. For feed session-end (ITCH 'S'/'C').
    void reset();

private:
    struct Instrument {
        std::unique_ptr<ReferenceMatcher> matcher;
        std::unique_ptr<OrderManager> manager;
        mutable uint64_t nextSnapshotSequenceNumber{0};
    };

    Instrument* find(const Symbol& symbol);
    const Instrument* find(const Symbol& symbol) const;
    void emit(EventPayload payload);

    std::unordered_map<Symbol, Instrument> instruments_;
    std::vector<Event> eventLog_;
    uint64_t nextSequenceNumber_{0};
};

}  // namespace titan
