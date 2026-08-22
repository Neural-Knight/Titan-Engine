#include "titan/replay/event_replayer.hpp"

#include <fstream>
#include <iterator>

#include "titan/feed/itch/book_builder.hpp"
#include "titan/replay/itch_adapter.hpp"

namespace titan {

EventReplayer::EventReplayer(InstrumentRegistry& registry, size_t checkpointInterval)
    : registry_(registry), checkpointInterval_(checkpointInterval)
{
}

ReplayResult EventReplayer::replayMessages(const std::vector<ItchParseResult>& results)
{
    ItchBookBuilder builder;
    ItchEngineAdapter adapter(registry_);
    ParityChecker checker(builder, adapter, checkpointInterval_);

    ReplayResult result;
    for (const ItchParseResult& parsed : results)
    {
        if (!parsed.ok)
        {
            ++result.messagesSkipped;
            continue;
        }
        checker.process(parsed.message);
        ++result.messagesDecoded;
    }

    result.parity = checker.report();
    result.engineEventLog = registry_.eventLog();
    return result;
}

ReplayResult EventReplayer::replayFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    ItchParser parser;
    const std::vector<ItchParseResult> results = parser.feed(bytes);

    ReplayResult result = replayMessages(results);
    result.messagesSkipped = parser.messagesSkipped();  // includes framing-level skips, not just decode failures
    return result;
}

bool EventReplayer::eventLogsEqual(const std::vector<Event>& a, const std::vector<Event>& b)
{
    return a == b;
}

}  // namespace titan
