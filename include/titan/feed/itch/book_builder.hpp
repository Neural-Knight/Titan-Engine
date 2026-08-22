#pragma once

#include <map>
#include <optional>
#include <string>
#include <unordered_map>

#include "titan/core/types.hpp"
#include "titan/feed/itch/messages.hpp"
#include "titan/market_data/book_snapshot.hpp"

namespace titan {

// Per-symbol resting-order book built from decoded ITCH messages.
// Feed-side only: not wired into InstrumentRegistry/ReferenceMatcher (Module 11).
class ItchBookBuilder {
public:
    void apply(const ItchMessage& message);

    // nullopt if `locate` has no registered book (no R seen yet).
    std::optional<BookSnapshot> snapshot(StockLocate locate, size_t depth) const;
    std::optional<std::string> symbolForLocate(StockLocate locate) const;
    size_t orderCount(StockLocate locate) const;

    void reset();

private:
    struct RestingOrder {
        Side side;
        Price price;
        Quantity remainingShares;
    };

    struct SymbolBook {
        std::string symbol;
        std::unordered_map<OrderRefNumber, RestingOrder> orders;
        std::map<Price, Quantity> bidLevels;
        std::map<Price, Quantity> askLevels;
        mutable uint64_t nextSnapshotSequenceNumber{0};
    };

    void handle(const SystemEventMessage& message);
    void handle(const StockDirectoryMessage& message);
    void handle(const AddOrderMessage& message);
    void handle(const AddOrderMpidMessage& message);
    void handle(const OrderExecutedMessage& message);
    void handle(const OrderExecutedWithPriceMessage& message);
    void handle(const OrderCancelMessage& message);
    void handle(const OrderDeleteMessage& message);
    void handle(const OrderReplaceMessage& message);
    void handle(const TimestampSecondsMessage& message);

    SymbolBook* findBook(StockLocate locate);
    void addOrder(SymbolBook& book, OrderRefNumber ref, Side side, Quantity shares, Price price);
    void reduceOrder(SymbolBook& book, OrderRefNumber ref, Quantity shares);
    void deleteOrder(SymbolBook& book, OrderRefNumber ref);
    void addLevel(SymbolBook& book, Side side, Price price, Quantity qty);
    void removeLevel(SymbolBook& book, Side side, Price price, Quantity qty);

    std::unordered_map<StockLocate, SymbolBook> books_;
};

}  // namespace titan
