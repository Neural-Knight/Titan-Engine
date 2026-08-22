#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "titan/feed/itch/messages.hpp"
#include "titan/feed/itch/parser.hpp"

namespace titan {

struct TcpFeedConfig {
    std::string host;
    uint16_t port{0};
    size_t recvBufferSize{64 * 1024};
};

// Connects out as a TCP client, feeds received bytes to ItchParser, and
// calls onMessage for each decoded message -- all on one background thread.
class SocketFeedReader {
public:
    using MessageCallback = std::function<void(const ItchMessage&)>;

    SocketFeedReader(TcpFeedConfig config, MessageCallback onMessage);
    ~SocketFeedReader();

    // False if connect() failed; the background thread never starts.
    bool start();
    // Unblocks the recv loop and joins. Safe to call more than once.
    void stop();
    void join();

    bool running() const { return running_.load(); }
    size_t bytesReceived() const { return bytesReceived_.load(); }

    // Only safe to read after stop()/join() -- written from the reader thread.
    size_t messagesDecoded() const { return parser_.messagesDecoded(); }
    size_t messagesSkipped() const { return parser_.messagesSkipped(); }

private:
    void recvLoop();

    TcpFeedConfig config_;
    MessageCallback onMessage_;
    ItchParser parser_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    std::atomic<size_t> bytesReceived_{0};
    std::atomic<int> socketFd_{-1};
};

}  // namespace titan
