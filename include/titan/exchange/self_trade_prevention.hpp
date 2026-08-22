#pragma once

namespace titan {

// CancelIncoming: stops entirely at the first same-account collision,
// cancelling any remainder that would still cross. accountId 0 never collides.
enum class StpMode {
    CancelIncoming
};

}  // namespace titan
