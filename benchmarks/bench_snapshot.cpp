#include <benchmark/benchmark.h>

#include <iostream>

#include "titan/bench/harness.hpp"
#include "titan/exchange/instrument_registry.hpp"

using namespace titan;

namespace {

void seedTwentyLevels(InstrumentRegistry& registry, const Symbol& symbol)
{
    OrderId id = 1;
    for (int i = 0; i < 10; ++i, ++id)
        registry.submitOrder(symbol, Order{id, Side::Buy, static_cast<Price>(100 - i), 10});
    for (int i = 0; i < 10; ++i, ++id)
        registry.submitOrder(symbol, Order{id, Side::Sell, static_cast<Price>(110 + i), 10});
}

void BM_SnapshotTop10(benchmark::State& state)
{
    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    seedTwentyLevels(registry, "AAPL");

    for (auto _ : state)
        benchmark::DoNotOptimize(registry.snapshot("AAPL", 10));

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SnapshotTop10);

}  // namespace

int main(int argc, char** argv)
{
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();

    InstrumentRegistry registry;
    registry.createInstrument("AAPL");
    seedTwentyLevels(registry, "AAPL");

    const BenchResult result = run(BenchConfig{1000, 10000}, [&registry]() {
        benchmark::DoNotOptimize(registry.snapshot("AAPL", 10));
    });

    std::cout << "LATENCY_P50_NS=" << result.latency.percentile(50)
              << " LATENCY_P95_NS=" << result.latency.percentile(95)
              << " LATENCY_P99_NS=" << result.latency.percentile(99)
              << " LATENCY_P999_NS=" << result.latency.percentile(99.9) << "\n";
    return 0;
}
