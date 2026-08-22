#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "titan/feed/itch/messages.hpp"

namespace titan {

// ok=false means `rawType` was unknown or its payload size didn't match.
struct ItchParseResult {
    bool ok;
    uint8_t rawType;
    ItchMessage message;
};

// Incremental byte-stream framing: 2-byte BE length (includes the type byte),
// 1-byte type, then length-1 payload bytes. Tail bytes carry across feed() calls.
class ItchParser {
public:
    std::vector<ItchParseResult> feed(std::span<const uint8_t> data);
    void reset();

    size_t messagesDecoded() const { return messagesDecoded_; }
    size_t messagesSkipped() const { return messagesSkipped_; }

private:
    // Sanity cap, not the protocol max (a real ITCH message never exceeds ~60 bytes).
    static constexpr size_t kMaxMessageSize = 8192;

    std::vector<uint8_t> buffer_;
    size_t messagesDecoded_{0};
    size_t messagesSkipped_{0};
};

}  // namespace titan
