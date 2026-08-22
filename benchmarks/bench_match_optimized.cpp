#include <benchmark/benchmark.h>

#include <iostream>
#include <string>
#include <vector>

#include "titan/bench/harness.hpp"
#include "titan/bench/scenario.hpp"
#include "titan/book/optimized_matcher.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

namespace {

std::vector<ScenarioOp> loadOps(const std::string& name)
{
    const Scenario scenario = loadScenario(std::string(TITAN_SCENARIOS_DIR) + "/" + name + ".txt");
    return scenario.generateOrders(scenario.seed);
}

void replay(OrderManager& manager, const std::vector<ScenarioOp>& ops)
{
    for (const auto& op : ops)
    {
        switch (op.type)
        {
            case ScenarioOpType::Add:
                manager.addOrder(Order{op.orderId, op.side, op.price, op.quantity});
                break;
            case ScenarioOpType::Cancel:
                manager.cancelOrder(op.orderId);
                break;
            case ScenarioOpType::Match:
                manager.matchOrder(Order{op.orderId, op.side, op.price, op.quantity});
                break;
            case ScenarioOpType::Snapshot:
                break;
        }
    }
}

void BM_SteadyLimitFlowOptimized(benchmark::State& state)
{
    const auto ops = loadOps("steady_limit_flow");
    for (auto _ : state)
    {
        OptimizedMatcher matcher;
        OrderManager manager(matcher);
        replay(manager, ops);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(ops.size()));
}
BENCHMARK(BM_SteadyLimitFlowOptimized);

void BM_BurstCrossingOptimized(benchmark::State& state)
{
    const auto ops = loadOps("burst_crossing");
    for (auto _ : state)
    {
        OptimizedMatcher matcher;
        OrderManager manager(matcher);
        replay(manager, ops);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(ops.size()));
}
BENCHMARK(BM_BurstCrossingOptimized);

}  // namespace

// Same replay-latency convention as bench_match.cpp: per full replay, not per op.
int main(int argc, char** argv)
{
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();

    const auto ops = loadOps("steady_limit_flow");
    const BenchResult result = run(BenchConfig{5, 30}, [&ops]() {
        OptimizedMatcher matcher;
        OrderManager manager(matcher);
        replay(manager, ops);
    });

    std::cout << "LATENCY_P50_NS=" << result.latency.percentile(50)
              << " LATENCY_P95_NS=" << result.latency.percentile(95)
              << " LATENCY_P99_NS=" << result.latency.percentile(99)
              << " LATENCY_P999_NS=" << result.latency.percentile(99.9) << "\n";
    return 0;
}
