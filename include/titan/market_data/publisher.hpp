#pragma once

#include <unordered_map>
#include <vector>

#include "titan/core/events.hpp"
#include "titan/market_data/execution_report.hpp"

namespace titan {

// Derives ExecutionReports purely from InstrumentRegistry::eventLog() --
// never calls back into OrderManager, so a crossing submitOrder's missing
// TradeExecuted (see InstrumentRegistry::submitOrder) stays missing here too.
class EventPublisher {
public:
    void process(const Event& event);

    const std::vector<ExecutionReport>& reports() const { return reports_; }
    void clear();

private:
    struct OrderState {
        Quantity originalQty{0};
        Quantity cumQty{0};
        Quantity cumNotional{0};
    };

    void handle(const OrderSubmitted& event);
    void handle(const OrderCancelled& event);
    void handle(const OrderReplaced& event);
    void handle(const TradeExecuted& event);
    void handle(const OrderRejected& event);

    void emitFill(const Symbol& symbol, OrderId orderId, Quantity tradeQty, Price tradePx);
    uint64_t nextSeq(const Symbol& symbol);

    std::unordered_map<Symbol, std::unordered_map<OrderId, OrderState>> orderStates_;
    std::unordered_map<Symbol, uint64_t> nextSequenceNumber_;
    std::vector<ExecutionReport> reports_;
};

}  // namespace titan
