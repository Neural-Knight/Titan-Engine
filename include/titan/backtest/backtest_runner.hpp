#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "titan/backtest/strategy.hpp"
#include "titan/core/types.hpp"
#include "titan/exchange/instrument_registry.hpp"
#include "titan/market_data/book_snapshot.hpp"

namespace titan {

struct BacktestConfig {
    MatcherBackend backend{MatcherBackend::Reference};
    size_t bookDepth{10};
};

struct BacktestResult {
    size_t messagesDecoded{0};
    size_t messagesSkipped{0};
    size_t ordersSubmitted{0};
    size_t ordersRejected{0};
    size_t ordersCancelled{0};
    size_t ordersReplaced{0};
    size_t tradesExecuted{0};
    uint64_t totalTradeVolume{0};
    std::unordered_map<Symbol, BookSnapshot> finalBook;
    uint64_t wallTimeNs{0};
};

// Replays one ITCH file through a fresh InstrumentRegistry via EventReplayer,
// then derives every count from the resulting event log -- no separate fill tracking.
class BacktestRunner {
public:
    explicit BacktestRunner(BacktestConfig config = {});

    // `strategy` may be nullptr. Its hooks fire post-replay -- see strategy.hpp.
    BacktestResult replayFile(const std::string& path, IStrategy* strategy = nullptr) const;

private:
    BacktestConfig config_;
};

}  // namespace titan
