#pragma once

#include <array>
#include <string>

#include "titan/feed/itch/messages.hpp"

// Shared ItchMessage builders for hand-crafted Module 10/11 test sessions.
// `inline` so this header can be included from multiple test translation units.
namespace titan::test {

inline std::array<char, 8> makeStock(const std::string& symbol)
{
    std::array<char, 8> out{};
    out.fill(' ');
    for (size_t i = 0; i < symbol.size() && i < out.size(); ++i)
        out[i] = symbol[i];
    return out;
}

inline StockDirectoryMessage makeDirectory(StockLocate locate, const std::string& symbol)
{
    StockDirectoryMessage msg{};
    msg.stockLocate = locate;
    msg.stock = makeStock(symbol);
    return msg;
}

inline AddOrderMessage makeAdd(StockLocate locate, OrderRefNumber ref, char side, uint32_t shares, uint32_t price)
{
    AddOrderMessage msg{};
    msg.stockLocate = locate;
    msg.orderReferenceNumber = ref;
    msg.buySellIndicator = side;
    msg.shares = shares;
    msg.price = price;
    return msg;
}

inline OrderExecutedMessage makeExecuted(StockLocate locate, OrderRefNumber ref, uint32_t shares)
{
    OrderExecutedMessage msg{};
    msg.stockLocate = locate;
    msg.orderReferenceNumber = ref;
    msg.executedShares = shares;
    return msg;
}

inline OrderExecutedWithPriceMessage makeExecutedWithPrice(StockLocate locate, OrderRefNumber ref, uint32_t shares,
                                                             uint32_t executionPrice)
{
    OrderExecutedWithPriceMessage msg{};
    msg.base = makeExecuted(locate, ref, shares);
    msg.printable = 'Y';
    msg.executionPrice = executionPrice;
    return msg;
}

inline OrderCancelMessage makeCancel(StockLocate locate, OrderRefNumber ref, uint32_t shares)
{
    OrderCancelMessage msg{};
    msg.stockLocate = locate;
    msg.orderReferenceNumber = ref;
    msg.cancelledShares = shares;
    return msg;
}

inline OrderDeleteMessage makeDelete(StockLocate locate, OrderRefNumber ref)
{
    OrderDeleteMessage msg{};
    msg.stockLocate = locate;
    msg.orderReferenceNumber = ref;
    return msg;
}

inline OrderReplaceMessage makeReplace(StockLocate locate, OrderRefNumber oldRef, OrderRefNumber newRef,
                                        uint32_t shares, uint32_t price)
{
    OrderReplaceMessage msg{};
    msg.stockLocate = locate;
    msg.originalOrderReferenceNumber = oldRef;
    msg.newOrderReferenceNumber = newRef;
    msg.shares = shares;
    msg.price = price;
    return msg;
}

}  // namespace titan::test
