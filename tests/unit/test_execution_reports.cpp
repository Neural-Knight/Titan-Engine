#include <gtest/gtest.h>

#include "titan/exchange/instrument_registry.hpp"
#include "titan/market_data/publisher.hpp"

using namespace titan;

namespace {

const ExecutionReport* findLast(const std::vector<ExecutionReport>& reports, OrderId id, ExecType type) {
    const ExecutionReport* found = nullptr;
    for (const auto& report : reports)
        if (report.orderId == id && report.execType == type)
            found = &report;
    return found;
}

}  // namespace

TEST(ExecutionReports, PartialFillProducesCorrectTradeReport) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Sell, 50, 100});
    registry.matchOrder("AAPL", Order{2, Side::Buy, 50, 40});

    EventPublisher publisher;
    for (const Event& event : registry.eventLog())
        publisher.process(event);

    const ExecutionReport* incoming = findLast(publisher.reports(), 2, ExecType::Trade);
    ASSERT_NE(incoming, nullptr);
    EXPECT_EQ(incoming->lastQty, 40u);
    EXPECT_EQ(incoming->cumQty, 40u);
    EXPECT_EQ(incoming->leavesQty, 0u);
    EXPECT_EQ(incoming->avgPx, 50u);

    const ExecutionReport* resting = findLast(publisher.reports(), 1, ExecType::Trade);
    ASSERT_NE(resting, nullptr);
    EXPECT_EQ(resting->lastQty, 40u);
    EXPECT_EQ(resting->cumQty, 40u);
    EXPECT_EQ(resting->leavesQty, 60u);
    EXPECT_EQ(resting->avgPx, 50u);
}

TEST(ExecutionReports, FullFillLeavesZeroOnBothSides) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Sell, 50, 100});
    registry.matchOrder("AAPL", Order{2, Side::Buy, 50, 100});

    EventPublisher publisher;
    for (const Event& event : registry.eventLog())
        publisher.process(event);

    const ExecutionReport* incoming = findLast(publisher.reports(), 2, ExecType::Trade);
    ASSERT_NE(incoming, nullptr);
    EXPECT_EQ(incoming->cumQty, 100u);
    EXPECT_EQ(incoming->leavesQty, 0u);

    const ExecutionReport* resting = findLast(publisher.reports(), 1, ExecType::Trade);
    ASSERT_NE(resting, nullptr);
    EXPECT_EQ(resting->cumQty, 100u);
    EXPECT_EQ(resting->leavesQty, 0u);
}

TEST(ExecutionReports, CancelProducesCancelledReport) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    registry.cancelOrder("AAPL", 1);

    EventPublisher publisher;
    for (const Event& event : registry.eventLog())
        publisher.process(event);

    const ExecutionReport* cancelled = findLast(publisher.reports(), 1, ExecType::Cancelled);
    ASSERT_NE(cancelled, nullptr);
    EXPECT_EQ(cancelled->leavesQty, 0u);
}

TEST(ExecutionReports, UnknownSymbolProducesRejectedReport) {
    InstrumentRegistry registry;

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});

    EventPublisher publisher;
    for (const Event& event : registry.eventLog())
        publisher.process(event);

    const ExecutionReport* rejected = findLast(publisher.reports(), 1, ExecType::Rejected);
    ASSERT_NE(rejected, nullptr);
    EXPECT_EQ(rejected->rejectReason, RejectReason::UnknownSymbol);
}

TEST(ExecutionReports, PerSymbolSequenceNumbersAreIndependent) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    registry.createInstrument("GOOG");

    registry.submitOrder("AAPL", Order{1, Side::Buy, 100, 10});
    registry.submitOrder("GOOG", Order{2, Side::Buy, 200, 10});
    registry.submitOrder("AAPL", Order{3, Side::Buy, 101, 10});

    EventPublisher publisher;
    for (const Event& event : registry.eventLog())
        publisher.process(event);

    const auto& reports = publisher.reports();
    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].sequenceNumber, 0u);  // AAPL #1
    EXPECT_EQ(reports[1].sequenceNumber, 0u);  // GOOG #1, independent counter
    EXPECT_EQ(reports[2].sequenceNumber, 1u);  // AAPL #2
}

TEST(ExecutionReports, ReportsAreDerivableFromEventLogReplay) {
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");

    registry.submitOrder("AAPL", Order{1, Side::Sell, 50, 100});
    registry.matchOrder("AAPL", Order{2, Side::Buy, 50, 40});
    registry.cancelReplace("AAPL", 1, Order{1, Side::Sell, 55, 60});

    EventPublisher publisher;
    for (const Event& event : registry.eventLog())
        publisher.process(event);

    const auto& reports = publisher.reports();
    // New(1), New(2), Trade(2), Trade(1), Replaced(1)
    ASSERT_EQ(reports.size(), 5u);
    EXPECT_EQ(reports[0].execType, ExecType::New);
    EXPECT_EQ(reports[1].execType, ExecType::New);
    EXPECT_EQ(reports[2].execType, ExecType::Trade);
    EXPECT_EQ(reports[3].execType, ExecType::Trade);
    EXPECT_EQ(reports[4].execType, ExecType::Replaced);
    EXPECT_EQ(reports[4].orderId, 1u);
    EXPECT_EQ(reports[4].leavesQty, 60u);
    EXPECT_EQ(reports[4].lastPx, 55u);
}
