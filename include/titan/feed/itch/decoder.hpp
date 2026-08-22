#pragma once

#include <cstdint>
#include <span>

#include "titan/feed/itch/messages.hpp"

namespace titan {

// `payload` excludes the 1-byte type. False (out untouched) on an unknown
// type or a payload size that doesn't match that type's fixed layout.
bool decodeMessage(uint8_t type, std::span<const uint8_t> payload, ItchMessage& out);

}  // namespace titan
