#include <cstdlib>
#include <iostream>
#include <string>

#include "titan/exchange/instrument_registry.hpp"
#include "titan/replay/event_replayer.hpp"

using namespace titan;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: replay_engine <path-to-itch-file> [--checkpoint N] [--matcher reference|optimized]\n";
        return 1;
    }

    size_t checkpoint = 1;
    MatcherBackend backend = MatcherBackend::Reference;
    for (int i = 2; i + 1 < argc; i += 2)
    {
        const std::string flag = argv[i];
        if (flag == "--checkpoint")
            checkpoint = static_cast<size_t>(std::atoi(argv[i + 1]));
        else if (flag == "--matcher" && std::string(argv[i + 1]) == "optimized")
            backend = MatcherBackend::Optimized;
    }

    InstrumentRegistry registry(backend);
    EventReplayer replayer(registry, checkpoint);
    const ReplayResult result = replayer.replayFile(argv[1]);

    std::cout << "decoded: " << result.messagesDecoded << ", skipped: " << result.messagesSkipped << "\n";
    std::cout << "checkpoints compared: " << result.parity.checkpointsCompared << "\n";
    std::cout << "mismatches: " << result.parity.mismatchCount << "\n";

    for (const auto& mismatch : result.parity.firstMismatches)
    {
        std::cout << "  mismatch at message " << mismatch.messageIndex << " symbol '" << mismatch.feedSnapshot.symbol
                   << "'\n";
        std::cout << "    feed:   bids=" << mismatch.feedSnapshot.bids.size()
                   << " asks=" << mismatch.feedSnapshot.asks.size() << "\n";
        std::cout << "    engine: bids=" << mismatch.engineSnapshot.bids.size()
                   << " asks=" << mismatch.engineSnapshot.asks.size() << "\n";
    }

    std::cout << "trade volume: feed=" << result.parity.feedTradeVolume
               << " engine=" << result.parity.engineTradeVolume << "\n";

    return result.parity.mismatchCount == 0 ? 0 : 1;
}
