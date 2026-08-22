#include "titan/feed/itch/parser.hpp"

#include "titan/feed/itch/decoder.hpp"

namespace titan {

std::vector<ItchParseResult> ItchParser::feed(std::span<const uint8_t> data)
{
    buffer_.insert(buffer_.end(), data.begin(), data.end());

    std::vector<ItchParseResult> results;
    size_t offset = 0;
    while (buffer_.size() - offset >= 2)
    {
        const uint16_t length = static_cast<uint16_t>(
            static_cast<uint16_t>(buffer_[offset]) << 8 | buffer_[offset + 1]);

        if (length == 0 || length > kMaxMessageSize)
        {
            // Corrupt length: drop it and resync from the next byte instead of crashing.
            ++messagesSkipped_;
            offset += 2;
            continue;
        }

        const size_t totalSize = 2 + length;
        if (buffer_.size() - offset < totalSize)
            break;  // incomplete message, wait for more bytes

        const uint8_t type = buffer_[offset + 2];
        const std::span<const uint8_t> payload(buffer_.data() + offset + 3, length - 1);

        ItchParseResult result{};
        result.rawType = type;
        result.ok = decodeMessage(type, payload, result.message);
        if (result.ok)
            ++messagesDecoded_;
        else
            ++messagesSkipped_;
        results.push_back(result);

        offset += totalSize;
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(offset));
    return results;
}

void ItchParser::reset()
{
    buffer_.clear();
    messagesDecoded_ = 0;
    messagesSkipped_ = 0;
}

}  // namespace titan
