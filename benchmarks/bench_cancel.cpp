#include <benchmark/benchmark.h>

#include <iostream>
#include <string>
#include <vector>

#include "reference/order_book.hpp"
#include "titan/bench/harness.hpp"
#include "titan/bench/scenario.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

namespace {

constexpr uint64_t kSeedOrders = 5000;
constexpr uint64_t kIdOffset = kSeedOrders + 1;  // keeps scenario ids off the seeded range

void seedBook(OrderManager& manager)
{
    for (uint64_t i = 0; i < kSeedOrders; ++i)
    {
        const Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        const Price price = (side == Side::Buy) ? (90 + i % 10) : (110 + i % 10);
        manager.addOrder(Order{i + 1, side, price, 10});
    }
}

std::vector<ScenarioOp> loadCancelHeavyOps()
{
    const Scenario scenario = loadScenario(std::string(TITAN_SCENARIOS_DIR) + "/cancel_heavy.txt");
    return scenario.generateOrders(scenario.seed);
}

// Cancels mostly hit ids from the same replay (offset above the seeded range);
// a few land on seeded orders, which is fine -- cancelOrder no-ops on a miss.
void replayOnSeededBook(OrderManager& manager, const std::vector<ScenarioOp>& ops)
{
    for (const auto& op : ops)
    {
        const OrderId id = op.orderId + kIdOffset;
        if (op.type == ScenarioOpType::Add)
            manager.addOrder(Order{id, op.side, op.price, op.quantity});
        else if (op.type == ScenarioOpType::Cancel)
            manager.cancelOrder(id);
    }
}

void BM_CancelHeavy(benchmark::State& state)
{
    const auto ops = loadCancelHeavyOps();
    for (auto _ : state)
    {
        ReferenceMatcher matcher;
        OrderManager manager(matcher);
        seedBook(manager);
        replayOnSeededBook(manager, ops);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(ops.size()));
}
BENCHMARK(BM_CancelHeavy);

}  // namespace

int main(int argc, char** argv)
{
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();

    const auto ops = loadCancelHeavyOps();
    const BenchResult result = run(BenchConfig{5, 30}, [&ops]() {
        ReferenceMatcher matcher;
        OrderManager manager(matcher);
        seedBook(manager);
        replayOnSeededBook(manager, ops);
    });

    std::cout << "LATENCY_P50_NS=" << result.latency.percentile(50)
              << " LATENCY_P95_NS=" << result.latency.percentile(95)
              << " LATENCY_P99_NS=" << result.latency.percentile(99)
              << " LATENCY_P999_NS=" << result.latency.percentile(99.9) << "\n";
    return 0;
}
