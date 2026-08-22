#pragma once

#include <string>
#include <vector>

#include "titan/core/events.hpp"
#include "titan/exchange/instrument_registry.hpp"
#include "titan/feed/itch/parser.hpp"
#include "titan/replay/parity_checker.hpp"

namespace titan {

struct ReplayResult {
    ParityReport parity;
    std::vector<Event> engineEventLog;
    size_t messagesDecoded{0};
    size_t messagesSkipped{0};
};

// Orchestrates parser + feed builder + engine adapter + parity checking
// for one ITCH byte stream, against a caller-owned InstrumentRegistry.
class EventReplayer {
public:
    explicit EventReplayer(InstrumentRegistry& registry, size_t checkpointInterval = 1);

    ReplayResult replayFile(const std::string& path);
    ReplayResult replayMessages(const std::vector<ItchParseResult>& results);

    static bool eventLogsEqual(const std::vector<Event>& a, const std::vector<Event>& b);

private:
    InstrumentRegistry& registry_;
    size_t checkpointInterval_;
};

}  // namespace titan
