#include <gtest/gtest.h>

#include <string>

#include "titan/backtest/backtest_runner.hpp"

using namespace titan;

namespace {

std::string fixturePath()
{
    return std::string(TITAN_FIXTURES_DIR) + "/itch/sample_session.itch";
}

class CountingStrategy : public IStrategy {
public:
    void onEvent(const Event&) override { ++eventCount; }
    void onExecutionReport(const ExecutionReport&) override { ++reportCount; }
    void onBookUpdate(const BookSnapshot&) override { ++bookUpdateCount; }

    int eventCount{0};
    int reportCount{0};
    int bookUpdateCount{0};
};

}  // namespace

TEST(BacktestRunner, FixtureFileProducesExpectedMetrics)
{
    BacktestRunner runner;
    const BacktestResult result = runner.replayFile(fixturePath());

    EXPECT_EQ(result.messagesDecoded, 5u);
    EXPECT_EQ(result.messagesSkipped, 0u);
    EXPECT_EQ(result.totalTradeVolume, 100u);  // matches the known feed/engine trade volume for this fixture
    EXPECT_GT(result.ordersSubmitted, 0u);
    EXPECT_EQ(result.finalBook.size(), 1u);    // single-symbol fixture
}

TEST(BacktestRunner, DeterministicDoubleRun)
{
    BacktestRunner runner;
    const BacktestResult first = runner.replayFile(fixturePath());
    const BacktestResult second = runner.replayFile(fixturePath());

    EXPECT_EQ(first.messagesDecoded, second.messagesDecoded);
    EXPECT_EQ(first.messagesSkipped, second.messagesSkipped);
    EXPECT_EQ(first.ordersSubmitted, second.ordersSubmitted);
    EXPECT_EQ(first.ordersRejected, second.ordersRejected);
    EXPECT_EQ(first.ordersCancelled, second.ordersCancelled);
    EXPECT_EQ(first.ordersReplaced, second.ordersReplaced);
    EXPECT_EQ(first.tradesExecuted, second.tradesExecuted);
    EXPECT_EQ(first.totalTradeVolume, second.totalTradeVolume);
    ASSERT_EQ(first.finalBook.size(), second.finalBook.size());
    for (const auto& [symbol, book] : first.finalBook)
    {
        ASSERT_TRUE(second.finalBook.count(symbol));
        EXPECT_EQ(book.bids, second.finalBook.at(symbol).bids);
        EXPECT_EQ(book.asks, second.finalBook.at(symbol).asks);
    }
}

TEST(BacktestRunner, StrategyCallbackFires)
{
    BacktestRunner runner;
    CountingStrategy strategy;
    runner.replayFile(fixturePath(), &strategy);

    EXPECT_GT(strategy.eventCount, 0);
    EXPECT_GT(strategy.reportCount, 0);
    EXPECT_GT(strategy.bookUpdateCount, 0);
}
