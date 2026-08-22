// Direct InstrumentRegistry replay vs the same ops via StagedProcessor.
// Latency: per-op wall time (direct) or enqueue-to-applied time (pipeline).
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "titan/bench/latency_histogram.hpp"
#include "titan/bench/scenario.hpp"
#include "titan/exchange/instrument_registry.hpp"
#include "titan/pipeline/staged_processor.hpp"

using namespace titan;

namespace {

std::vector<ScenarioOp> loadOps(const std::string& name)
{
    const Scenario scenario = loadScenario(std::string(TITAN_SCENARIOS_DIR) + "/" + name + ".txt");
    return scenario.generateOrders(scenario.seed);
}

uint64_t nowNanos()
{
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

struct RunSummary {
    double throughputOpsPerSec;
    LatencyHistogram latency;
};

RunSummary runDirect(const std::vector<ScenarioOp>& ops, const Symbol& symbol)
{
    InstrumentRegistry registry(MatcherBackend::Optimized);
    registry.createInstrument(symbol);

    LatencyHistogram latency;
    const uint64_t start = nowNanos();
    for (const auto& op : ops)
    {
        const uint64_t opStart = nowNanos();
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
                break;
        }
        latency.record(nowNanos() - opStart);
    }
    const double seconds = static_cast<double>(nowNanos() - start) / 1e9;
    return RunSummary{static_cast<double>(ops.size()) / seconds, std::move(latency)};
}

RunSummary runPipeline(const std::vector<ScenarioOp>& ops, const Symbol& symbol)
{
    StagedProcessor processor(MatcherBackend::Optimized);
    LatencyHistogram latency;
    // Callback runs on the consumer thread only; read after producer.join()+stop() below.
    processor.setApplyCallback(
        [&latency](uint64_t, const std::vector<Trade>&, uint64_t latencyNs) { latency.record(latencyNs); });
    processor.createInstrument(symbol);
    processor.start();

    const uint64_t start = nowNanos();
    std::thread producer([&]() {
        uint64_t sequence = 0;
        for (const auto& op : ops)
        {
            switch (op.type)
            {
                case ScenarioOpType::Add:
                    processor.enqueueSubmit(symbol, Order{op.orderId, op.side, op.price, op.quantity}, sequence++);
                    break;
                case ScenarioOpType::Cancel:
                    processor.enqueueCancel(symbol, op.orderId, sequence++);
                    break;
                case ScenarioOpType::Match:
                    processor.enqueueMatch(symbol, Order{op.orderId, op.side, op.price, op.quantity}, sequence++);
                    break;
                case ScenarioOpType::Snapshot:
                    break;
            }
        }
    });
    producer.join();
    processor.stop();
    const double seconds = static_cast<double>(nowNanos() - start) / 1e9;
    return RunSummary{static_cast<double>(ops.size()) / seconds, std::move(latency)};
}

}  // namespace

int main()
{
    const auto ops = loadOps("steady_limit_flow");
    const Symbol symbol = "SYM0";

    const RunSummary direct = runDirect(ops, symbol);
    const RunSummary pipeline = runPipeline(ops, symbol);

    std::cout << "DIRECT_THROUGHPUT_OPS_PER_SEC=" << direct.throughputOpsPerSec
              << " DIRECT_P50_NS=" << direct.latency.percentile(50)
              << " DIRECT_P99_NS=" << direct.latency.percentile(99)
              << " DIRECT_P999_NS=" << direct.latency.percentile(99.9) << "\n";
    std::cout << "PIPELINE_THROUGHPUT_OPS_PER_SEC=" << pipeline.throughputOpsPerSec
              << " PIPELINE_P50_NS=" << pipeline.latency.percentile(50)
              << " PIPELINE_P99_NS=" << pipeline.latency.percentile(99)
              << " PIPELINE_P999_NS=" << pipeline.latency.percentile(99.9) << "\n";
    return 0;
}
