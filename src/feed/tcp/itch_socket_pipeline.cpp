#include "titan/feed/tcp/itch_socket_pipeline.hpp"

#include <utility>

namespace titan {

ItchSocketPipeline::ItchSocketPipeline(TcpFeedConfig config, MatcherBackend backend, size_t checkpointInterval)
    : registry_(backend),
      adapter_(registry_),
      checker_(builder_, adapter_, checkpointInterval),
      reader_(std::move(config), [this](const ItchMessage& message) { onMessage(message); })
{
}

void ItchSocketPipeline::onMessage(const ItchMessage& message)
{
    checker_.process(message);
    ++messagesDecoded_;
}

bool ItchSocketPipeline::start()
{
    return reader_.start();
}

void ItchSocketPipeline::stop()
{
    reader_.stop();
}

void ItchSocketPipeline::join()
{
    reader_.join();
}

}  // namespace titan
