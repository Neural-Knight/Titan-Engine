#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "titan/feed/tcp/itch_socket_pipeline.hpp"

using namespace titan;

namespace {

std::vector<uint8_t> readFixture()
{
    std::ifstream file(std::string(TITAN_FIXTURES_DIR) + "/itch/sample_session.itch", std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// Minimal loopback TCP server: binds an ephemeral port, accepts one client.
class LoopbackServer {
public:
    LoopbackServer()
    {
        ::signal(SIGPIPE, SIG_IGN);  // a send() after the client disconnects must not kill the test binary
        listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        ::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::listen(listenFd_, 1);

        sockaddr_in bound{};
        socklen_t boundLen = sizeof(bound);
        ::getsockname(listenFd_, reinterpret_cast<sockaddr*>(&bound), &boundLen);
        port_ = ntohs(bound.sin_port);
    }

    ~LoopbackServer()
    {
        if (serverThread_.joinable())
            serverThread_.join();
        if (listenFd_ >= 0)
            ::close(listenFd_);
    }

    uint16_t port() const { return port_; }

    // Sends `data` split into `numChunks` writes a few ms apart, then closes.
    void serveChunked(std::vector<uint8_t> data, int numChunks)
    {
        serverThread_ = std::thread([this, data = std::move(data), numChunks]() {
            const int clientFd = ::accept(listenFd_, nullptr, nullptr);
            if (clientFd < 0)
                return;
            const size_t chunkSize = data.empty() ? 1 : (data.size() + numChunks - 1) / static_cast<size_t>(numChunks);
            for (size_t offset = 0; offset < data.size(); offset += chunkSize)
            {
                const size_t n = std::min(chunkSize, data.size() - offset);
                ::send(clientFd, data.data() + offset, n, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            ::close(clientFd);
        });
    }

    // Accepts and holds the connection open without sending -- exercises stop()'s unblock path.
    void acceptAndHold()
    {
        serverThread_ = std::thread([this]() {
            const int clientFd = ::accept(listenFd_, nullptr, nullptr);
            if (clientFd < 0)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));  // outlives the test's own stop() call
            ::close(clientFd);
        });
    }

private:
    int listenFd_{-1};
    uint16_t port_{0};
    std::thread serverThread_;
};

}  // namespace

TEST(SocketFeed, LoopbackFixtureMatchesFileParity)
{
    LoopbackServer server;
    server.serveChunked(readFixture(), 1);

    ItchSocketPipeline pipeline(TcpFeedConfig{"127.0.0.1", server.port()});
    ASSERT_TRUE(pipeline.start());
    pipeline.join();  // server closes after sending -> EOF -> reader loop exits

    EXPECT_EQ(pipeline.messagesDecoded(), 5u);
    EXPECT_EQ(pipeline.messagesSkipped(), 0u);
    EXPECT_EQ(pipeline.parityReport().mismatchCount, 0u);
}

TEST(SocketFeed, PartialSendsAcrossMultipleChunksStillFrameCorrectly)
{
    LoopbackServer server;
    server.serveChunked(readFixture(), 3);

    ItchSocketPipeline pipeline(TcpFeedConfig{"127.0.0.1", server.port()});
    ASSERT_TRUE(pipeline.start());
    pipeline.join();

    EXPECT_EQ(pipeline.messagesDecoded(), 5u);
    EXPECT_EQ(pipeline.messagesSkipped(), 0u);
    EXPECT_EQ(pipeline.parityReport().mismatchCount, 0u);
}

TEST(SocketFeed, StopDoesNotHangWhenConnectionIsIdle)
{
    LoopbackServer server;
    server.acceptAndHold();

    ItchSocketPipeline pipeline(TcpFeedConfig{"127.0.0.1", server.port()});
    ASSERT_TRUE(pipeline.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // let it connect and settle into recv/poll

    const auto begin = std::chrono::steady_clock::now();
    pipeline.stop();
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    EXPECT_LT(elapsed, std::chrono::seconds(1));
}
