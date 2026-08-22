#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

#include "titan/feed/itch/decoder.hpp"
#include "titan/feed/itch/parser.hpp"

using namespace titan;

namespace {

void pushU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

void pushU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8)
        out.push_back(static_cast<uint8_t>(v >> shift));
}

void pushU48(std::vector<uint8_t>& out, uint64_t v) {
    for (int shift = 40; shift >= 0; shift -= 8)
        out.push_back(static_cast<uint8_t>(v >> shift));
}

void pushU64(std::vector<uint8_t>& out, uint64_t v) {
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<uint8_t>(v >> shift));
}

void pushStr(std::vector<uint8_t>& out, const std::string& s) {
    out.insert(out.end(), s.begin(), s.end());
}

// 2-byte BE length (includes the type byte) + type + payload.
std::vector<uint8_t> framed(char type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    pushU16(out, static_cast<uint16_t>(1 + payload.size()));
    out.push_back(static_cast<uint8_t>(type));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::vector<uint8_t> makeAddOrder(OrderRefNumber ref, uint32_t shares, uint32_t price) {
    std::vector<uint8_t> payload;
    pushU16(payload, 1);
    pushU16(payload, 0);
    pushU48(payload, 1000);
    pushU64(payload, ref);
    payload.push_back('B');
    pushU32(payload, shares);
    pushStr(payload, "AAPL    ");
    pushU32(payload, price);
    return framed('A', payload);
}

std::vector<uint8_t> makeOrderExecuted(OrderRefNumber ref, uint32_t shares, MatchNumber match) {
    std::vector<uint8_t> payload;
    pushU16(payload, 1);
    pushU16(payload, 0);
    pushU48(payload, 2000);
    pushU64(payload, ref);
    pushU32(payload, shares);
    pushU64(payload, match);
    return framed('E', payload);
}

std::vector<uint8_t> makeOrderCancel(OrderRefNumber ref, uint32_t cancelledShares) {
    std::vector<uint8_t> payload;
    pushU16(payload, 1);
    pushU16(payload, 0);
    pushU48(payload, 3000);
    pushU64(payload, ref);
    pushU32(payload, cancelledShares);
    return framed('X', payload);
}

std::vector<uint8_t> makeOrderDelete(OrderRefNumber ref) {
    std::vector<uint8_t> payload;
    pushU16(payload, 1);
    pushU16(payload, 0);
    pushU48(payload, 4000);
    pushU64(payload, ref);
    return framed('D', payload);
}

std::vector<uint8_t> makeOrderReplace(OrderRefNumber oldRef, OrderRefNumber newRef, uint32_t shares, uint32_t price) {
    std::vector<uint8_t> payload;
    pushU16(payload, 1);
    pushU16(payload, 0);
    pushU48(payload, 5000);
    pushU64(payload, oldRef);
    pushU64(payload, newRef);
    pushU32(payload, shares);
    pushU32(payload, price);
    return framed('U', payload);
}

// Full 38-byte ITCH 5.0 Stock Directory payload.
std::vector<uint8_t> makeStockDirectory(const std::string& stock, uint32_t roundLotSize = 100,
                                         char financialStatus = 'N') {
    std::vector<uint8_t> payload;
    pushU16(payload, 1);
    pushU16(payload, 0);
    pushU48(payload, 500);
    pushStr(payload, stock);
    payload.push_back('Q');              // marketCategory
    payload.push_back(financialStatus);
    pushU32(payload, roundLotSize);
    payload.push_back('Y');              // roundLotsOnly
    payload.push_back('C');              // issueClassification
    pushStr(payload, "  ");              // issueSubType
    payload.push_back('P');              // authenticity
    payload.push_back('N');              // shortSaleThreshold
    payload.push_back('N');              // ipoFlag
    payload.push_back('1');              // luldRefPriceTier
    payload.push_back('N');              // etpFlag
    pushU32(payload, 0);                 // etpLeverageFactor
    payload.push_back('N');              // inverseIndicator
    return framed('R', payload);
}

std::vector<uint8_t> makeTimestampSeconds(uint32_t seconds) {
    std::vector<uint8_t> payload;
    pushU32(payload, seconds);
    return framed('T', payload);
}

std::vector<uint8_t> makeSystemEvent(char eventCode) {
    std::vector<uint8_t> payload;
    pushU16(payload, 0);
    pushU16(payload, 0);
    pushU48(payload, 100);
    payload.push_back(static_cast<uint8_t>(eventCode));
    return framed('S', payload);
}

std::vector<uint8_t> makeAddOrderMpid(OrderRefNumber ref, uint32_t shares, uint32_t price, const std::string& mpid) {
    std::vector<uint8_t> payload;
    pushU16(payload, 1);
    pushU16(payload, 0);
    pushU48(payload, 1500);
    pushU64(payload, ref);
    payload.push_back('S');
    pushU32(payload, shares);
    pushStr(payload, "AAPL    ");
    pushU32(payload, price);
    pushStr(payload, mpid);  // exactly 4 chars
    return framed('F', payload);
}

std::vector<uint8_t> makeOrderExecutedWithPrice(OrderRefNumber ref, uint32_t shares, MatchNumber match,
                                                 char printable, uint32_t execPrice) {
    std::vector<uint8_t> payload;
    pushU16(payload, 1);
    pushU16(payload, 0);
    pushU48(payload, 2500);
    pushU64(payload, ref);
    pushU32(payload, shares);
    pushU64(payload, match);
    payload.push_back(static_cast<uint8_t>(printable));
    pushU32(payload, execPrice);
    return framed('C', payload);
}

}  // namespace

TEST(ItchParser, DecodesAddOrder) {
    const auto bytes = makeAddOrder(555, 100, 1005000);
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<AddOrderMessage>(results[0].message);
    EXPECT_EQ(msg.stockLocate, 1u);
    EXPECT_EQ(msg.orderReferenceNumber, 555u);
    EXPECT_EQ(msg.buySellIndicator, 'B');
    EXPECT_EQ(msg.shares, 100u);
    EXPECT_EQ(msg.price, 1005000u);
    EXPECT_EQ(std::string(msg.stock.begin(), msg.stock.end()), "AAPL    ");
}

TEST(ItchParser, DecodesOrderExecuted) {
    const auto bytes = makeOrderExecuted(555, 40, 9001);
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<OrderExecutedMessage>(results[0].message);
    EXPECT_EQ(msg.orderReferenceNumber, 555u);
    EXPECT_EQ(msg.executedShares, 40u);
    EXPECT_EQ(msg.matchNumber, 9001u);
}

TEST(ItchParser, DecodesOrderCancel) {
    const auto bytes = makeOrderCancel(555, 30);
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<OrderCancelMessage>(results[0].message);
    EXPECT_EQ(msg.orderReferenceNumber, 555u);
    EXPECT_EQ(msg.cancelledShares, 30u);
}

TEST(ItchParser, DecodesOrderDelete) {
    const auto bytes = makeOrderDelete(555);
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<OrderDeleteMessage>(results[0].message);
    EXPECT_EQ(msg.orderReferenceNumber, 555u);
}

TEST(ItchParser, DecodesOrderReplace) {
    const auto bytes = makeOrderReplace(555, 556, 80, 1010000);
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<OrderReplaceMessage>(results[0].message);
    EXPECT_EQ(msg.originalOrderReferenceNumber, 555u);
    EXPECT_EQ(msg.newOrderReferenceNumber, 556u);
    EXPECT_EQ(msg.shares, 80u);
    EXPECT_EQ(msg.price, 1010000u);
}

TEST(ItchParser, DecodesStockDirectoryPaddedSymbol) {
    const auto bytes = makeStockDirectory("AAPL    ", 100, 'N');
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<StockDirectoryMessage>(results[0].message);
    EXPECT_EQ(std::string(msg.stock.begin(), msg.stock.end()), "AAPL    ");
    EXPECT_EQ(msg.marketCategory, 'Q');
    EXPECT_EQ(msg.financialStatus, 'N');
    EXPECT_EQ(msg.roundLotSize, 100u);
}

TEST(ItchParser, DecodesSystemEvent) {
    const auto bytes = makeSystemEvent('O');
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<SystemEventMessage>(results[0].message);
    EXPECT_EQ(msg.eventCode, 'O');
}

TEST(ItchParser, DecodesAddOrderMpidAttribution) {
    const auto bytes = makeAddOrderMpid(700, 50, 999000, "NSDQ");
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<AddOrderMpidMessage>(results[0].message);
    EXPECT_EQ(msg.base.orderReferenceNumber, 700u);
    EXPECT_EQ(msg.base.shares, 50u);
    EXPECT_EQ(std::string(msg.attribution.begin(), msg.attribution.end()), "NSDQ");
}

TEST(ItchParser, DecodesOrderExecutedWithPrice) {
    const auto bytes = makeOrderExecutedWithPrice(555, 20, 4242, 'Y', 1006000);
    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].ok);
    const auto& msg = std::get<OrderExecutedWithPriceMessage>(results[0].message);
    EXPECT_EQ(msg.base.orderReferenceNumber, 555u);
    EXPECT_EQ(msg.base.executedShares, 20u);
    EXPECT_EQ(msg.printable, 'Y');
    EXPECT_EQ(msg.executionPrice, 1006000u);
}

TEST(ItchParser, PartialMessageAcrossTwoFeedsDecodesOnce) {
    const auto bytes = makeAddOrder(555, 100, 1005000);
    const size_t half = bytes.size() / 2;

    ItchParser parser;
    const auto firstHalf = parser.feed(std::span<const uint8_t>(bytes.data(), half));
    EXPECT_TRUE(firstHalf.empty());

    const auto secondHalf = parser.feed(std::span<const uint8_t>(bytes.data() + half, bytes.size() - half));
    ASSERT_EQ(secondHalf.size(), 1u);
    EXPECT_TRUE(secondHalf[0].ok);
}

TEST(ItchParser, MultipleMessagesInOneBufferDecodeInOrder) {
    std::vector<uint8_t> bytes;
    const auto add = makeAddOrder(1, 10, 100);
    const auto exec = makeOrderExecuted(1, 10, 1);
    const auto del = makeOrderDelete(1);
    bytes.insert(bytes.end(), add.begin(), add.end());
    bytes.insert(bytes.end(), exec.begin(), exec.end());
    bytes.insert(bytes.end(), del.begin(), del.end());

    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<AddOrderMessage>(results[0].message));
    EXPECT_TRUE(std::holds_alternative<OrderExecutedMessage>(results[1].message));
    EXPECT_TRUE(std::holds_alternative<OrderDeleteMessage>(results[2].message));
}

TEST(ItchParser, ZeroLengthIsSkippedWithoutCrashing) {
    std::vector<uint8_t> bytes = {0x00, 0x00};  // corrupt: length field is 0
    const auto trailing = makeOrderDelete(1);
    bytes.insert(bytes.end(), trailing.begin(), trailing.end());

    ItchParser parser;
    const auto results = parser.feed(bytes);

    EXPECT_EQ(parser.messagesSkipped(), 1u);  // the corrupt 0-length record itself isn't a "result"
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].ok);
}

TEST(ItchParser, OversizedLengthIsSkippedWithoutCrashing) {
    std::vector<uint8_t> bytes;
    pushU16(bytes, 0xFFFF);  // far larger than any real message, over kMaxMessageSize guard

    ItchParser parser;
    const auto results = parser.feed(bytes);

    EXPECT_TRUE(results.empty());  // corrupt length record produces no result, just a skip count
    EXPECT_EQ(parser.messagesSkipped(), 1u);
}

TEST(ItchParser, UnknownMessageTypeIsSkippedGracefully) {
    std::vector<uint8_t> payload(5, 0);
    const auto bytes = framed('Z', payload);

    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(results[0].ok);
    EXPECT_EQ(results[0].rawType, static_cast<uint8_t>('Z'));
    EXPECT_EQ(parser.messagesSkipped(), 1u);
}

TEST(ItchParser, WrongPayloadSizeForKnownTypeIsRejected) {
    ItchMessage out;
    std::vector<uint8_t> tooShort(10, 0);
    EXPECT_FALSE(decodeMessage(static_cast<uint8_t>('A'), tooShort, out));
}

TEST(ItchParser, TimestampSecondsMonotonicAcrossSyntheticSession) {
    std::vector<uint8_t> bytes;
    for (uint32_t seconds : {34200u, 34260u, 34320u}) {
        const auto msg = makeTimestampSeconds(seconds);
        bytes.insert(bytes.end(), msg.begin(), msg.end());
    }

    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 3u);
    uint32_t previous = 0;
    for (const auto& result : results) {
        ASSERT_TRUE(result.ok);
        const auto& msg = std::get<TimestampSecondsMessage>(result.message);
        EXPECT_GT(msg.seconds, previous);
        previous = msg.seconds;
    }
}

TEST(ItchParser, SampleFixtureFileDecodesExpectedCounts) {
    std::ifstream file(std::string(TITAN_FIXTURES_DIR) + "/itch/sample_session.itch", std::ios::binary);
    ASSERT_TRUE(file.is_open());
    const std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    ItchParser parser;
    const auto results = parser.feed(bytes);

    ASSERT_EQ(results.size(), 5u);
    EXPECT_EQ(parser.messagesDecoded(), 5u);
    EXPECT_EQ(parser.messagesSkipped(), 0u);
    EXPECT_TRUE(std::holds_alternative<SystemEventMessage>(results[0].message));
    EXPECT_TRUE(std::holds_alternative<StockDirectoryMessage>(results[1].message));
    EXPECT_TRUE(std::holds_alternative<AddOrderMessage>(results[2].message));
    EXPECT_TRUE(std::holds_alternative<OrderExecutedMessage>(results[3].message));
    EXPECT_TRUE(std::holds_alternative<OrderDeleteMessage>(results[4].message));
}
