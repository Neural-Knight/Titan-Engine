#include "titan/exchange/instrument_registry.hpp"

#include <utility>

namespace titan {

void InstrumentRegistry::createInstrument(const Symbol& symbol)
{
    if (instruments_.count(symbol))
        return;

    Instrument instrument;
    instrument.matcher = std::make_unique<ReferenceMatcher>();
    instrument.manager = std::make_unique<OrderManager>(*instrument.matcher);
    instruments_.emplace(symbol, std::move(instrument));
}

bool InstrumentRegistry::hasInstrument(const Symbol& symbol) const
{
    return instruments_.count(symbol) != 0;
}

InstrumentRegistry::Instrument* InstrumentRegistry::find(const Symbol& symbol)
{
    auto it = instruments_.find(symbol);
    return (it == instruments_.end()) ? nullptr : &it->second;
}

const InstrumentRegistry::Instrument* InstrumentRegistry::find(const Symbol& symbol) const
{
    auto it = instruments_.find(symbol);
    return (it == instruments_.end()) ? nullptr : &it->second;
}

BookSnapshot InstrumentRegistry::snapshot(const Symbol& symbol, size_t depth) const
{
    const Instrument* instrument = find(symbol);
    if (!instrument)
        return BookSnapshot{symbol, 0, {}, {}};

    return BookSnapshot{
        symbol,
        instrument->nextSnapshotSequenceNumber++,
        instrument->matcher->bidDepth(depth),
        instrument->matcher->askDepth(depth),
    };
}

void InstrumentRegistry::emit(EventPayload payload)
{
    eventLog_.push_back(Event{nextSequenceNumber_++, std::move(payload)});
}

AcceptResult InstrumentRegistry::submitOrder(const Symbol& symbol, Order order)
{
    Instrument* instrument = find(symbol);
    if (!instrument)
    {
        emit(OrderRejected{symbol, order.id, RejectReason::UnknownSymbol});
        return AcceptResult{false, RejectReason::UnknownSymbol};
    }

    const AcceptResult result = instrument->manager->addOrder(order);
    if (result.accepted)
        emit(OrderSubmitted{symbol, order.id, order.side, order.price, order.quantity});
    else
        emit(OrderRejected{symbol, order.id, result.reason});
    return result;
}

AcceptResult InstrumentRegistry::cancelOrder(const Symbol& symbol, OrderId id)
{
    Instrument* instrument = find(symbol);
    if (!instrument)
    {
        emit(OrderRejected{symbol, id, RejectReason::UnknownSymbol});
        return AcceptResult{false, RejectReason::UnknownSymbol};
    }

    const AcceptResult result = instrument->manager->cancelOrder(id);
    if (result.accepted)
        emit(OrderCancelled{symbol, id});
    else
        emit(OrderRejected{symbol, id, result.reason});
    return result;
}

std::vector<Trade> InstrumentRegistry::matchOrder(const Symbol& symbol, Order order)
{
    Instrument* instrument = find(symbol);
    if (!instrument)
    {
        emit(OrderRejected{symbol, order.id, RejectReason::UnknownSymbol});
        return {};
    }

    const MatchResult result = instrument->manager->matchOrder(order);
    if (result.accept.accepted)
        emit(OrderSubmitted{symbol, order.id, order.side, order.price, order.quantity});
    else
        emit(OrderRejected{symbol, order.id, result.accept.reason});
    for (const Trade& trade : result.trades)
        emit(TradeExecuted{symbol, trade.incomingOrderId, trade.restingOrderId, trade.price, trade.quantity});
    return result.trades;
}

AcceptResult InstrumentRegistry::cancelReplace(const Symbol& symbol, OrderId oldId, Order newOrder)
{
    Instrument* instrument = find(symbol);
    if (!instrument)
    {
        emit(OrderRejected{symbol, oldId, RejectReason::UnknownSymbol});
        return AcceptResult{false, RejectReason::UnknownSymbol};
    }

    const AcceptResult result = instrument->manager->cancelReplace(oldId, newOrder);
    if (result.accepted)
        emit(OrderReplaced{symbol, oldId, newOrder.price, newOrder.quantity});
    else
        emit(OrderRejected{symbol, oldId, result.reason});
    return result;
}

}  // namespace titan
