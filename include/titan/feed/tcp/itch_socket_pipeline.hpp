#pragma once

#include <cstddef>

#include "titan/exchange/instrument_registry.hpp"
#include "titan/feed/itch/book_builder.hpp"
#include "titan/feed/tcp/socket_feed.hpp"
#include "titan/replay/itch_adapter.hpp"
#include "titan/replay/parity_checker.hpp"

namespace titan {

// Socket bytes -> ItchParser (in SocketFeedReader) -> ItchEngineAdapter,
// parity-checked against ItchBookBuilder -- streaming counterpart to EventReplayer.
class ItchSocketPipeline {
public:
    explicit ItchSocketPipeline(TcpFeedConfig config, MatcherBackend backend = MatcherBackend::Optimized,
                                 size_t checkpointInterval = 1);

    bool start();
    void stop();
    void join();
    bool running() const { return reader_.running(); }

    // Only safe to read after stop()/join() -- written from the reader thread.
    size_t messagesDecoded() const { return messagesDecoded_; }
    size_t messagesSkipped() const { return reader_.messagesSkipped(); }
    const ParityReport& parityReport() const { return checker_.report(); }

    InstrumentRegistry& registryForInvariants() { return registry_; }

private:
    void onMessage(const ItchMessage& message);

    InstrumentRegistry registry_;
    ItchBookBuilder builder_;
    ItchEngineAdapter adapter_;
    ParityChecker checker_;
    size_t messagesDecoded_{0};
    SocketFeedReader reader_;
};

}  // namespace titan
