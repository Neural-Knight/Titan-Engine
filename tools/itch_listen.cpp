#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "titan/exchange/instrument_registry.hpp"
#include "titan/feed/tcp/itch_socket_pipeline.hpp"

using namespace titan;

namespace {
std::atomic<bool> gInterrupted{false};
void onSignal(int) { gInterrupted.store(true); }
}  // namespace

int main(int argc, char** argv)
{
    std::string host;
    uint16_t port = 0;
    MatcherBackend backend = MatcherBackend::Reference;
    size_t checkpoint = 1;

    for (int i = 1; i + 1 < argc; i += 2)
    {
        const std::string flag = argv[i];
        if (flag == "--host")
            host = argv[i + 1];
        else if (flag == "--port")
            port = static_cast<uint16_t>(std::atoi(argv[i + 1]));
        else if (flag == "--matcher" && std::string(argv[i + 1]) == "optimized")
            backend = MatcherBackend::Optimized;
        else if (flag == "--checkpoint")
            checkpoint = static_cast<size_t>(std::atoi(argv[i + 1]));
    }

    if (host.empty() || port == 0)
    {
        std::cerr << "usage: itch_listen --host H --port P [--matcher reference|optimized] [--checkpoint N]\n";
        std::cerr << "connects as a TCP client to H:P -- run a mock/local server first, not a real feed yet\n";
        return 1;
    }

    std::signal(SIGINT, onSignal);

    ItchSocketPipeline pipeline(TcpFeedConfig{host, port}, backend, checkpoint);
    if (!pipeline.start())
    {
        std::cerr << "failed to connect to " << host << ":" << port << "\n";
        return 1;
    }

    while (pipeline.running() && !gInterrupted.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pipeline.stop();

    std::cout << "decoded: " << pipeline.messagesDecoded() << ", skipped: " << pipeline.messagesSkipped() << "\n";
    std::cout << "checkpoints compared: " << pipeline.parityReport().checkpointsCompared << "\n";
    std::cout << "mismatches: " << pipeline.parityReport().mismatchCount << "\n";
    std::cout << "trade volume: feed=" << pipeline.parityReport().feedTradeVolume
               << " engine=" << pipeline.parityReport().engineTradeVolume << "\n";

    return pipeline.parityReport().mismatchCount == 0 ? 0 : 1;
}
