#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "titan/core/types.hpp"

namespace titan {

enum class ScenarioOpType {
    Add,
    Cancel,
    Match,
    Snapshot
};

// symbolIndex is 0-based; caller maps it onto its own symbol names.
struct ScenarioOp {
    ScenarioOpType type;
    uint32_t symbolIndex;
    OrderId orderId;
    Side side;
    Price price;
    Quantity quantity;
};

struct Scenario {
    std::string name;
    uint32_t symbolCount{1};
    uint64_t orderCount{0};
    uint32_t seed{0};
    double addWeight{1.0};
    double cancelWeight{0.0};
    double matchWeight{0.0};
    double snapshotWeight{0.0};

    // Deterministic given the same seed: mt19937 only, no time/random_device.
    std::vector<ScenarioOp> generateOrders(uint32_t seedOverride) const;
};

// Parses the simple `key=value` text format used under benchmarks/scenarios/.
Scenario loadScenario(const std::string& path);

}  // namespace titan
