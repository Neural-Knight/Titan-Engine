#pragma once

#include <array>
#include <cstdint>
#include <variant>

namespace titan {

constexpr char kItchSystemEvent = 'S';
constexpr char kItchStockDirectory = 'R';
constexpr char kItchAddOrder = 'A';
constexpr char kItchAddOrderMpid = 'F';
constexpr char kItchOrderExecuted = 'E';
constexpr char kItchOrderExecutedWithPrice = 'C';
constexpr char kItchOrderCancel = 'X';
constexpr char kItchOrderDelete = 'D';
constexpr char kItchOrderReplace = 'U';
constexpr char kItchTimestampSeconds = 'T';

using StockLocate = uint16_t;
using TrackingNumber = uint16_t;
using ItchTimestampNs = uint64_t;  // wire field is 6 bytes, nanoseconds since midnight
using OrderRefNumber = uint64_t;
using MatchNumber = uint64_t;

struct SystemEventMessage {
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    ItchTimestampNs timestamp;
    char eventCode;
};

// Full ITCH 5.0 field list, 38-byte payload. See itch-notes.md.
struct StockDirectoryMessage {
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    ItchTimestampNs timestamp;
    std::array<char, 8> stock;  // space-padded
    char marketCategory;
    char financialStatus;
    uint32_t roundLotSize;
    char roundLotsOnly;
    char issueClassification;
    std::array<char, 2> issueSubType;
    char authenticity;
    char shortSaleThreshold;
    char ipoFlag;
    char luldRefPriceTier;
    char etpFlag;
    uint32_t etpLeverageFactor;
    char inverseIndicator;
};

// price is fixed-point: real price * 10000, per ITCH convention.
struct AddOrderMessage {
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    ItchTimestampNs timestamp;
    OrderRefNumber orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    std::array<char, 8> stock;
    uint32_t price;
};

struct AddOrderMpidMessage {
    AddOrderMessage base;
    std::array<char, 4> attribution;
};

struct OrderExecutedMessage {
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    ItchTimestampNs timestamp;
    OrderRefNumber orderReferenceNumber;
    uint32_t executedShares;
    MatchNumber matchNumber;
};

struct OrderExecutedWithPriceMessage {
    OrderExecutedMessage base;
    char printable;
    uint32_t executionPrice;
};

struct OrderCancelMessage {
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    ItchTimestampNs timestamp;
    OrderRefNumber orderReferenceNumber;
    uint32_t cancelledShares;
};

struct OrderDeleteMessage {
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    ItchTimestampNs timestamp;
    OrderRefNumber orderReferenceNumber;
};

struct OrderReplaceMessage {
    StockLocate stockLocate;
    TrackingNumber trackingNumber;
    ItchTimestampNs timestamp;
    OrderRefNumber originalOrderReferenceNumber;
    OrderRefNumber newOrderReferenceNumber;
    uint32_t shares;
    uint32_t price;
};

// Not real ITCH 5.0 -- nanosecond time lives in every message's own 6-byte
// header field instead. Modeled on ITCH 4.1's seconds message; see itch-notes.md.
struct TimestampSecondsMessage {
    uint32_t seconds;
};

using ItchMessage = std::variant<
    SystemEventMessage,
    StockDirectoryMessage,
    AddOrderMessage,
    AddOrderMpidMessage,
    OrderExecutedMessage,
    OrderExecutedWithPriceMessage,
    OrderCancelMessage,
    OrderDeleteMessage,
    OrderReplaceMessage,
    TimestampSecondsMessage
>;

}  // namespace titan
