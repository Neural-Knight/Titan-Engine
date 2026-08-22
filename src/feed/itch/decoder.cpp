#include "titan/feed/itch/decoder.hpp"

#include <algorithm>

namespace titan {

namespace {

uint16_t readU16BE(const uint8_t* p)
{
    return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) << 8 | p[1]);
}

uint32_t readU32BE(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
           static_cast<uint32_t>(p[2]) << 8 | static_cast<uint32_t>(p[3]);
}

uint64_t readU48BE(const uint8_t* p)
{
    uint64_t v = 0;
    for (int i = 0; i < 6; ++i)
        v = (v << 8) | p[i];
    return v;
}

uint64_t readU64BE(const uint8_t* p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v = (v << 8) | p[i];
    return v;
}

}  // namespace

bool decodeMessage(uint8_t type, std::span<const uint8_t> payload, ItchMessage& out)
{
    const uint8_t* p = payload.data();

    switch (type)
    {
        case kItchSystemEvent:
        {
            if (payload.size() != 11)
                return false;
            out = SystemEventMessage{
                readU16BE(p), readU16BE(p + 2), readU48BE(p + 4), static_cast<char>(p[10])};
            return true;
        }
        case kItchStockDirectory:
        {
            if (payload.size() != 38)
                return false;
            StockDirectoryMessage msg{};
            msg.stockLocate = readU16BE(p);
            msg.trackingNumber = readU16BE(p + 2);
            msg.timestamp = readU48BE(p + 4);
            std::copy(p + 10, p + 18, msg.stock.begin());
            msg.marketCategory = static_cast<char>(p[18]);
            msg.financialStatus = static_cast<char>(p[19]);
            msg.roundLotSize = readU32BE(p + 20);
            msg.roundLotsOnly = static_cast<char>(p[24]);
            msg.issueClassification = static_cast<char>(p[25]);
            std::copy(p + 26, p + 28, msg.issueSubType.begin());
            msg.authenticity = static_cast<char>(p[28]);
            msg.shortSaleThreshold = static_cast<char>(p[29]);
            msg.ipoFlag = static_cast<char>(p[30]);
            msg.luldRefPriceTier = static_cast<char>(p[31]);
            msg.etpFlag = static_cast<char>(p[32]);
            msg.etpLeverageFactor = readU32BE(p + 33);
            msg.inverseIndicator = static_cast<char>(p[37]);
            out = msg;
            return true;
        }
        case kItchAddOrder:
        {
            if (payload.size() != 35)
                return false;
            AddOrderMessage msg{};
            msg.stockLocate = readU16BE(p);
            msg.trackingNumber = readU16BE(p + 2);
            msg.timestamp = readU48BE(p + 4);
            msg.orderReferenceNumber = readU64BE(p + 10);
            msg.buySellIndicator = static_cast<char>(p[18]);
            msg.shares = readU32BE(p + 19);
            std::copy(p + 23, p + 31, msg.stock.begin());
            msg.price = readU32BE(p + 31);
            out = msg;
            return true;
        }
        case kItchAddOrderMpid:
        {
            if (payload.size() != 39)
                return false;
            ItchMessage base;
            if (!decodeMessage(kItchAddOrder, payload.subspan(0, 35), base))
                return false;
            AddOrderMpidMessage msg{};
            msg.base = std::get<AddOrderMessage>(base);
            std::copy(p + 35, p + 39, msg.attribution.begin());
            out = msg;
            return true;
        }
        case kItchOrderExecuted:
        {
            if (payload.size() != 30)
                return false;
            OrderExecutedMessage msg{};
            msg.stockLocate = readU16BE(p);
            msg.trackingNumber = readU16BE(p + 2);
            msg.timestamp = readU48BE(p + 4);
            msg.orderReferenceNumber = readU64BE(p + 10);
            msg.executedShares = readU32BE(p + 18);
            msg.matchNumber = readU64BE(p + 22);
            out = msg;
            return true;
        }
        case kItchOrderExecutedWithPrice:
        {
            if (payload.size() != 35)
                return false;
            ItchMessage base;
            if (!decodeMessage(kItchOrderExecuted, payload.subspan(0, 30), base))
                return false;
            OrderExecutedWithPriceMessage msg{};
            msg.base = std::get<OrderExecutedMessage>(base);
            msg.printable = static_cast<char>(p[30]);
            msg.executionPrice = readU32BE(p + 31);
            out = msg;
            return true;
        }
        case kItchOrderCancel:
        {
            if (payload.size() != 22)
                return false;
            OrderCancelMessage msg{};
            msg.stockLocate = readU16BE(p);
            msg.trackingNumber = readU16BE(p + 2);
            msg.timestamp = readU48BE(p + 4);
            msg.orderReferenceNumber = readU64BE(p + 10);
            msg.cancelledShares = readU32BE(p + 18);
            out = msg;
            return true;
        }
        case kItchOrderDelete:
        {
            if (payload.size() != 18)
                return false;
            OrderDeleteMessage msg{};
            msg.stockLocate = readU16BE(p);
            msg.trackingNumber = readU16BE(p + 2);
            msg.timestamp = readU48BE(p + 4);
            msg.orderReferenceNumber = readU64BE(p + 10);
            out = msg;
            return true;
        }
        case kItchOrderReplace:
        {
            if (payload.size() != 34)
                return false;
            OrderReplaceMessage msg{};
            msg.stockLocate = readU16BE(p);
            msg.trackingNumber = readU16BE(p + 2);
            msg.timestamp = readU48BE(p + 4);
            msg.originalOrderReferenceNumber = readU64BE(p + 10);
            msg.newOrderReferenceNumber = readU64BE(p + 18);
            msg.shares = readU32BE(p + 26);
            msg.price = readU32BE(p + 30);
            out = msg;
            return true;
        }
        case kItchTimestampSeconds:
        {
            if (payload.size() != 4)
                return false;
            out = TimestampSecondsMessage{readU32BE(p)};
            return true;
        }
        default:
            return false;
    }
}

}  // namespace titan
