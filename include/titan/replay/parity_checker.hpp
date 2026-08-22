#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "titan/feed/itch/book_builder.hpp"
#include "titan/replay/itch_adapter.hpp"

namespace titan {

struct ParityMismatch {
    size_t messageIndex;
    BookSnapshot feedSnapshot;
    BookSnapshot engineSnapshot;
};

struct ParityReport {
    size_t messagesProcessed{0};
    size_t checkpointsCompared{0};
    size_t mismatchCount{0};
    std::vector<ParityMismatch> firstMismatches;
    uint64_t feedTradeVolume{0};
    uint64_t engineTradeVolume{0};
};

// Feeds decoded ITCH messages into a feed-oracle builder and an engine
// adapter in lockstep, comparing top-of-book every `checkpointInterval` messages.
class ParityChecker {
public:
    ParityChecker(ItchBookBuilder& builder, ItchEngineAdapter& adapter, size_t checkpointInterval = 1);

    void process(const ItchMessage& message);

    const ParityReport& report() const { return report_; }

private:
    static constexpr size_t kMaxStoredMismatches = 5;

    void compareAllLocates();

    ItchBookBuilder& builder_;
    ItchEngineAdapter& adapter_;
    size_t checkpointInterval_;
    std::unordered_set<StockLocate> knownLocates_;
    ParityReport report_;
};

}  // namespace titan
