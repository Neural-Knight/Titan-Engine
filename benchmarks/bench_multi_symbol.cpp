#include <benchmark/benchmark.h>

#include <iostream>
#include <string>
#include <vector>

#include "titan/bench/harness.hpp"
#include "titan/bench/scenario.hpp"
#include "titan/exchange/instrument_registry.hpp"

using namespace titan;

namespace {

std::vector<Symbol> makeSymbols(uint32_t count)
{
    std::vector<Symbol> symbols;
    symbols.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        symbols.push_back("SYM" + std::to_string(i));
    return symbols;
}

std::vector<ScenarioOp> loadMultiSymbolOps()
{
    const Scenario scenario = loadScenario(std::string(TITAN_SCENARIOS_DIR) + "/multi_symbol.txt");
    return scenario.generateOrders(scenario.seed);
}

// symbolIndex from the scenario is already scoped per-symbol, so ids never collide across symbols.
void replay(InstrumentRegistry& registry, const std::vector<Symbol>& symbols, const std::vector<ScenarioOp>& ops)
{
    for (const auto& op : ops)
    {
        const Symbol& symbol = symbols[op.symbolIndex % symbols.size()];
        switch (op.type)
        {
            case ScenarioOpType::Add:
                registry.submitOrder(symbol, Order{op.orderId, op.side, op.price, op.quantity});
                break;
            case ScenarioOpType::Cancel:
                registry.cancelOrder(symbol, op.orderId);
                break;
            case ScenarioOpType::Match:
                registry.matchOrder(symbol, Order{op.orderId, op.side, op.price, op.quantity});
                break;
            case ScenarioOpType::Snapshot:
                registry.snapshot(symbol, 10);
                break;
        }
    }
}

void BM_MultiSymbolMix(benchmark::State& state)
{
    const auto ops = loadMultiSymbolOps();
    const auto symbols = makeSymbols(100);
    for (auto _ : state)
    {
        InstrumentRegistry registry;
        for (const auto& symbol : symbols)
            registry.createInstrument(symbol);
        replay(registry, symbols, ops);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(ops.size()));
}
BENCHMARK(BM_MultiSymbolMix);

}  // namespace

int main(int argc, char** argv)
{
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();

    const auto ops = loadMultiSymbolOps();
    const auto symbols = makeSymbols(100);
    const BenchResult result = run(BenchConfig{5, 30}, [&]() {
        InstrumentRegistry registry;
        for (const auto& symbol : symbols)
            registry.createInstrument(symbol);
        replay(registry, symbols, ops);
    });

    std::cout << "LATENCY_P50_NS=" << result.latency.percentile(50)
              << " LATENCY_P95_NS=" << result.latency.percentile(95)
              << " LATENCY_P99_NS=" << result.latency.percentile(99)
              << " LATENCY_P999_NS=" << result.latency.percentile(99.9) << "\n";
    return 0;
}
