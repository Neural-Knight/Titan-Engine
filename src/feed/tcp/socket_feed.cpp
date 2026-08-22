// Design: parsing + decoding stays on this one reader thread (ItchParser
// isn't thread-safe) -- recv, feed(), and onMessage() all run sequentially here.
#include "titan/feed/tcp/socket_feed.hpp"

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstring>
#include <utility>
#include <vector>

namespace titan {

namespace {
int connectSocket(const std::string& host, uint16_t port)
{
    ::signal(SIGPIPE, SIG_IGN);  // a write to a closed peer must not kill the process

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &results) != 0)
        return -1;

    int fd = -1;
    for (addrinfo* it = results; it != nullptr; it = it->ai_next)
    {
        fd = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0)
            continue;
        if (::connect(fd, it->ai_addr, it->ai_addrlen) == 0)
            break;
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(results);
    return fd;
}
}  // namespace

SocketFeedReader::SocketFeedReader(TcpFeedConfig config, MessageCallback onMessage)
    : config_(std::move(config)), onMessage_(std::move(onMessage))
{
}

SocketFeedReader::~SocketFeedReader()
{
    if (thread_.joinable())
        stop();
}

bool SocketFeedReader::start()
{
    const int fd = connectSocket(config_.host, config_.port);
    if (fd < 0)
        return false;
    socketFd_.store(fd);
    running_.store(true);
    thread_ = std::thread(&SocketFeedReader::recvLoop, this);
    return true;
}

void SocketFeedReader::recvLoop()
{
    std::vector<uint8_t> buffer(config_.recvBufferSize);
    const int fd = socketFd_.load();
    while (!stop_.load())
    {
        pollfd pfd{fd, POLLIN, 0};
        const int polled = ::poll(&pfd, 1, 200);  // 200ms so stop_ is rechecked even if idle
        if (polled < 0)
            break;
        if (polled == 0)
            continue;
        if (pfd.revents & (POLLHUP | POLLERR))
            break;

        const ssize_t n = ::recv(fd, buffer.data(), buffer.size(), 0);
        if (n <= 0)
            break;  // EOF or error
        bytesReceived_.fetch_add(static_cast<size_t>(n));
        for (const ItchParseResult& result : parser_.feed({buffer.data(), static_cast<size_t>(n)}))
            if (result.ok && onMessage_)
                onMessage_(result.message);
    }
    ::close(fd);
    socketFd_.store(-1);
    running_.store(false);
}

void SocketFeedReader::stop()
{
    stop_.store(true);
    const int fd = socketFd_.load();
    if (fd >= 0)
        ::shutdown(fd, SHUT_RDWR);  // unblocks a pending recv/poll immediately
    join();
}

void SocketFeedReader::join()
{
    if (thread_.joinable())
        thread_.join();
}

}  // namespace titan
