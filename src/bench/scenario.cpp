#include "titan/bench/scenario.hpp"

#include <fstream>
#include <random>
#include <stdexcept>

namespace titan {

Scenario loadScenario(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("cannot open scenario file: " + path);

    Scenario scenario;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if (key == "name")
            scenario.name = value;
        else if (key == "symbolCount")
            scenario.symbolCount = static_cast<uint32_t>(std::stoul(value));
        else if (key == "orderCount")
            scenario.orderCount = std::stoull(value);
        else if (key == "seed")
            scenario.seed = static_cast<uint32_t>(std::stoul(value));
        else if (key == "addWeight")
            scenario.addWeight = std::stod(value);
        else if (key == "cancelWeight")
            scenario.cancelWeight = std::stod(value);
        else if (key == "matchWeight")
            scenario.matchWeight = std::stod(value);
        else if (key == "snapshotWeight")
            scenario.snapshotWeight = std::stod(value);
    }
    return scenario;
}

std::vector<ScenarioOp> Scenario::generateOrders(uint32_t seedOverride) const
{
    std::mt19937 rng(seedOverride);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_int_distribution<uint64_t> qtyDist(1, 100);
    std::uniform_int_distribution<uint64_t> priceDist(90, 110);

    const double total = addWeight + cancelWeight + matchWeight + snapshotWeight;
    const uint32_t symbols = (symbolCount > 0) ? symbolCount : 1;

    std::vector<std::vector<OrderId>> activeIdsPerSymbol(symbols);
    std::vector<ScenarioOp> ops;
    ops.reserve(orderCount);

    OrderId nextId = 1;
    for (uint64_t i = 0; i < orderCount; ++i)
    {
        const uint32_t symbolIndex = static_cast<uint32_t>(rng() % symbols);
        auto& activeIds = activeIdsPerSymbol[symbolIndex];

        const double roll = (total > 0.0) ? unit(rng) * total : 0.0;
        double cumulative = addWeight;
        ScenarioOpType type = ScenarioOpType::Add;
        if (roll < cumulative)
        {
            type = ScenarioOpType::Add;
        }
        else if (roll < (cumulative += cancelWeight) && !activeIds.empty())
        {
            type = ScenarioOpType::Cancel;
        }
        else if (roll < (cumulative += matchWeight))
        {
            type = ScenarioOpType::Match;
        }
        else
        {
            type = ScenarioOpType::Snapshot;
        }

        ScenarioOp op{};
        op.type = type;
        op.symbolIndex = symbolIndex;
        op.side = (rng() % 2 == 0) ? Side::Buy : Side::Sell;
        op.price = priceDist(rng);
        op.quantity = qtyDist(rng);

        if (type == ScenarioOpType::Cancel && !activeIds.empty())
        {
            op.orderId = activeIds.back();
            activeIds.pop_back();
        }
        else
        {
            op.orderId = nextId++;
            if (type == ScenarioOpType::Add)
                activeIds.push_back(op.orderId);
        }

        ops.push_back(op);
    }
    return ops;
}

}  // namespace titan
