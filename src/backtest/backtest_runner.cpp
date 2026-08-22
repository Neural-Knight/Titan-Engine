#include "titan/backtest/backtest_runner.hpp"

#include <chrono>
#include <type_traits>
#include <unordered_set>

#include "titan/market_data/publisher.hpp"
#include "titan/replay/event_replayer.hpp"

namespace titan {

BacktestRunner::BacktestRunner(BacktestConfig config) : config_(config) {}

BacktestResult BacktestRunner::replayFile(const std::string& path, IStrategy* strategy) const
{
    InstrumentRegistry registry(config_.backend);
    EventReplayer replayer(registry);

    const auto begin = std::chrono::steady_clock::now();
    const ReplayResult replay = replayer.replayFile(path);
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    BacktestResult result;
    result.messagesDecoded = replay.messagesDecoded;
    result.messagesSkipped = replay.messagesSkipped;
    result.wallTimeNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());

    std::unordered_set<Symbol> symbols;
    for (const Event& event : replay.engineEventLog)
    {
        if (strategy)
            strategy->onEvent(event);

        std::visit(
            [&](const auto& payload) {
                symbols.insert(payload.symbol);
                using T = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<T, OrderSubmitted>)
                    ++result.ordersSubmitted;
                else if constexpr (std::is_same_v<T, OrderRejected>)
                    ++result.ordersRejected;
                else if constexpr (std::is_same_v<T, OrderCancelled>)
                    ++result.ordersCancelled;
                else if constexpr (std::is_same_v<T, OrderReplaced>)
                    ++result.ordersReplaced;
                else if constexpr (std::is_same_v<T, TradeExecuted>)
                {
                    ++result.tradesExecuted;
                    result.totalTradeVolume += payload.quantity;
                }
            },
            event.payload);
    }

    if (strategy)
    {
        EventPublisher publisher;
        for (const Event& event : replay.engineEventLog)
            publisher.process(event);
        for (const ExecutionReport& report : publisher.reports())
            strategy->onExecutionReport(report);
    }

    for (const Symbol& symbol : symbols)
    {
        const BookSnapshot snapshot = registry.snapshot(symbol, config_.bookDepth);
        result.finalBook[symbol] = snapshot;
        if (strategy)
            strategy->onBookUpdate(snapshot);
    }

    return result;
}

}  // namespace titan
