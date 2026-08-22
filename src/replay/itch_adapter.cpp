#include "titan/replay/itch_adapter.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <variant>

namespace titan {

namespace {

std::string trimTrailingSpaces(const std::array<char, 8>& stock)
{
    std::string_view view(stock.data(), stock.size());
    const size_t end = view.find_last_not_of(' ');
    return end == std::string_view::npos ? std::string{} : std::string(view.substr(0, end + 1));
}

}  // namespace

ItchEngineAdapter::ItchEngineAdapter(InstrumentRegistry& registry) : registry_(registry) {}

bool ItchEngineAdapter::apply(const ItchMessage& message)
{
    return std::visit([this](const auto& msg) { return handle(msg); }, message);
}

OrderId ItchEngineAdapter::engineIdFor(OrderRefNumber ref) const
{
    const auto it = engineIdForRef_.find(ref);
    return (it == engineIdForRef_.end()) ? static_cast<OrderId>(ref) : it->second;
}

bool ItchEngineAdapter::handle(const SystemEventMessage& message)
{
    // Mirrors ItchBookBuilder: real ITCH 5.0 'C' is End of Messages.
    if (message.eventCode == 'C')
        reset();
    return true;
}

bool ItchEngineAdapter::handle(const StockDirectoryMessage& message)
{
    const std::string symbol = trimTrailingSpaces(message.stock);
    registry_.createInstrument(symbol);
    symbols_[message.stockLocate] = symbol;
    return true;
}

bool ItchEngineAdapter::handle(const AddOrderMessage& message)
{
    const auto symbolIt = symbols_.find(message.stockLocate);
    if (symbolIt == symbols_.end())
        return false;

    const Side side = (message.buySellIndicator == 'B') ? Side::Buy : Side::Sell;
    const OrderId id = static_cast<OrderId>(message.orderReferenceNumber);
    engineIdForRef_[message.orderReferenceNumber] = id;

    const Order order{id, side, static_cast<Price>(message.price), static_cast<Quantity>(message.shares)};
    return registry_.submitOrder(symbolIt->second, order).accepted;
}

bool ItchEngineAdapter::handle(const AddOrderMpidMessage& message)
{
    return handle(message.base);
}

bool ItchEngineAdapter::applyExecuted(StockLocate locate, OrderRefNumber ref, uint32_t executedShares,
                                       std::optional<uint32_t> executionPrice)
{
    const auto symbolIt = symbols_.find(locate);
    if (symbolIt == symbols_.end())
        return false;

    const OrderId id = engineIdFor(ref);
    const std::optional<Order> existing = registry_.findOrder(symbolIt->second, id);
    if (!existing)
        return false;

    const Quantity quantity = std::min<Quantity>(executedShares, existing->quantity);
    const Price price = executionPrice ? static_cast<Price>(*executionPrice) : existing->price;
    const Side oppositeSide = (existing->side == Side::Buy) ? Side::Sell : Side::Buy;

    const Order synthetic{nextSyntheticId_++, oppositeSide, price, quantity, 0, OrderType::Limit, TimeInForce::IOC};
    const std::vector<Trade> trades = registry_.matchOrder(symbolIt->second, synthetic);
    for (const Trade& trade : trades)
        tradeVolume_ += trade.quantity;
    return !trades.empty();
}

bool ItchEngineAdapter::handle(const OrderExecutedMessage& message)
{
    return applyExecuted(message.stockLocate, message.orderReferenceNumber, message.executedShares, std::nullopt);
}

bool ItchEngineAdapter::handle(const OrderExecutedWithPriceMessage& message)
{
    return applyExecuted(message.base.stockLocate, message.base.orderReferenceNumber, message.base.executedShares,
                          message.executionPrice);
}

bool ItchEngineAdapter::handle(const OrderCancelMessage& message)
{
    const auto symbolIt = symbols_.find(message.stockLocate);
    if (symbolIt == symbols_.end())
        return false;

    const OrderId id = engineIdFor(message.orderReferenceNumber);
    const std::optional<Order> existing = registry_.findOrder(symbolIt->second, id);
    if (!existing)
        return false;

    const Quantity cancelled = std::min<Quantity>(message.cancelledShares, existing->quantity);
    const Quantity remaining = existing->quantity - cancelled;
    if (remaining == 0)
        return registry_.cancelOrder(symbolIt->second, id).accepted;

    Order reduced = *existing;
    reduced.id = id;
    reduced.quantity = remaining;
    return registry_.cancelReplace(symbolIt->second, id, reduced).accepted;
}

bool ItchEngineAdapter::handle(const OrderDeleteMessage& message)
{
    const auto symbolIt = symbols_.find(message.stockLocate);
    if (symbolIt == symbols_.end())
        return false;

    const OrderId id = engineIdFor(message.orderReferenceNumber);
    return registry_.cancelOrder(symbolIt->second, id).accepted;
}

bool ItchEngineAdapter::handle(const OrderReplaceMessage& message)
{
    const auto symbolIt = symbols_.find(message.stockLocate);
    if (symbolIt == symbols_.end())
        return false;

    const OrderId id = engineIdFor(message.originalOrderReferenceNumber);
    const std::optional<Order> existing = registry_.findOrder(symbolIt->second, id);
    if (!existing)
        return false;

    Order replacement = *existing;
    replacement.id = id;
    replacement.price = static_cast<Price>(message.price);
    replacement.quantity = static_cast<Quantity>(message.shares);

    const bool accepted = registry_.cancelReplace(symbolIt->second, id, replacement).accepted;
    if (accepted)
        engineIdForRef_[message.newOrderReferenceNumber] = id;
    return accepted;
}

bool ItchEngineAdapter::handle(const TimestampSecondsMessage&)
{
    return true;
}

std::optional<std::string> ItchEngineAdapter::symbolForLocate(StockLocate locate) const
{
    const auto it = symbols_.find(locate);
    return it == symbols_.end() ? std::nullopt : std::optional<std::string>(it->second);
}

BookSnapshot ItchEngineAdapter::snapshot(StockLocate locate, size_t depth) const
{
    const auto it = symbols_.find(locate);
    if (it == symbols_.end())
        return BookSnapshot{"", 0, {}, {}};
    return registry_.snapshot(it->second, depth);
}

void ItchEngineAdapter::reset()
{
    registry_.reset();
    symbols_.clear();
    engineIdForRef_.clear();
    nextSyntheticId_ = 1'000'000'000;
    tradeVolume_ = 0;
}

}  // namespace titan
