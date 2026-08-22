#include "titan/market_data/publisher.hpp"

namespace titan {

void EventPublisher::process(const Event& event)
{
    std::visit([this](const auto& payload) { handle(payload); }, event.payload);
}

void EventPublisher::clear()
{
    reports_.clear();
    orderStates_.clear();
    nextSequenceNumber_.clear();
}

uint64_t EventPublisher::nextSeq(const Symbol& symbol)
{
    return nextSequenceNumber_[symbol]++;
}

void EventPublisher::handle(const OrderSubmitted& event)
{
    orderStates_[event.symbol][event.id] = OrderState{event.quantity, 0, 0};

    reports_.push_back(ExecutionReport{
        event.symbol, nextSeq(event.symbol), event.id, ExecType::New,
        0, 0, event.quantity, 0, 0, RejectReason::None});
}

void EventPublisher::emitFill(const Symbol& symbol, OrderId orderId, Quantity tradeQty, Price tradePx)
{
    auto symbolIt = orderStates_.find(symbol);
    if (symbolIt == orderStates_.end())
        return;

    auto stateIt = symbolIt->second.find(orderId);
    if (stateIt == symbolIt->second.end())
        return;  // predates this Publisher's view of the log

    OrderState& state = stateIt->second;
    state.cumQty += tradeQty;
    state.cumNotional += tradeQty * tradePx;
    const Quantity leaves = state.originalQty - state.cumQty;
    const Price avgPx = state.cumNotional / state.cumQty;

    reports_.push_back(ExecutionReport{
        symbol, nextSeq(symbol), orderId, ExecType::Trade,
        tradeQty, state.cumQty, leaves, tradePx, avgPx, RejectReason::None});
}

void EventPublisher::handle(const TradeExecuted& event)
{
    emitFill(event.symbol, event.incomingOrderId, event.quantity, event.price);
    emitFill(event.symbol, event.restingOrderId, event.quantity, event.price);
}

void EventPublisher::handle(const OrderCancelled& event)
{
    Quantity cumQty = 0;
    Price avgPx = 0;

    auto symbolIt = orderStates_.find(event.symbol);
    if (symbolIt != orderStates_.end())
    {
        auto stateIt = symbolIt->second.find(event.id);
        if (stateIt != symbolIt->second.end())
        {
            cumQty = stateIt->second.cumQty;
            avgPx = (cumQty > 0) ? (stateIt->second.cumNotional / cumQty) : 0;
        }
    }

    reports_.push_back(ExecutionReport{
        event.symbol, nextSeq(event.symbol), event.id, ExecType::Cancelled,
        0, cumQty, 0, 0, avgPx, RejectReason::None});
}

void EventPublisher::handle(const OrderReplaced& event)
{
    OrderState& state = orderStates_[event.symbol][event.id];
    state.originalQty = state.cumQty + event.newQuantity;
    const Price avgPx = (state.cumQty > 0) ? (state.cumNotional / state.cumQty) : 0;

    reports_.push_back(ExecutionReport{
        event.symbol, nextSeq(event.symbol), event.id, ExecType::Replaced,
        0, state.cumQty, event.newQuantity, event.newPrice, avgPx, RejectReason::None});
}

void EventPublisher::handle(const OrderRejected& event)
{
    reports_.push_back(ExecutionReport{
        event.symbol, nextSeq(event.symbol), event.id, ExecType::Rejected,
        0, 0, 0, 0, 0, event.reason});
}

}  // namespace titan
