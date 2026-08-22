// Global operator new/delete overrides, scoped to this one executable only
// (titan_tests is a separate binary and is unaffected). Counts total heap
// traffic during a persistent-matcher replay so pool warmup is visible.
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <vector>

#include "reference/order_book.hpp"
#include "titan/bench/harness.hpp"
#include "titan/bench/scenario.hpp"
#include "titan/book/optimized_matcher.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;

namespace {
std::atomic<uint64_t> gAllocCount{0};
std::atomic<uint64_t> gDeallocCount{0};
}  // namespace

void* operator new(size_t size)
{
    ++gAllocCount;
    if (void* ptr = std::malloc(size))
        return ptr;
    throw std::bad_alloc();
}
void operator delete(void* ptr) noexcept
{
    ++gDeallocCount;
    std::free(ptr);
}
void operator delete(void* ptr, size_t) noexcept
{
    ++gDeallocCount;
    std::free(ptr);
}
void* operator new[](size_t size)
{
    ++gAllocCount;
    if (void* ptr = std::malloc(size))
        return ptr;
    throw std::bad_alloc();
}
void operator delete[](void* ptr) noexcept
{
    ++gDeallocCount;
    std::free(ptr);
}
void operator delete[](void* ptr, size_t) noexcept
{
    ++gDeallocCount;
    std::free(ptr);
}

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

// Same op pattern, shifted order ids so a persistent OrderManager doesn't
// reject every Add as a duplicate on the 2nd+ pass.
std::vector<ScenarioOp> offsetIds(const std::vector<ScenarioOp>& ops, uint64_t offset)
{
    std::vector<ScenarioOp> out = ops;
    for (auto& op : out)
        op.orderId += offset;
    return out;
}

struct AllocReport {
    uint64_t allocs;
    uint64_t deallocs;
};

// One matcher instance lives across every pass, so the pool (if any) warms
// up and stays warm -- matching how a real exchange never rebuilds its book.
template <typename MatcherType>
AllocReport measureAllocs(const std::vector<ScenarioOp>& baseOps, int warmupPasses, int measuredPasses)
{
    constexpr uint64_t kIdSpace = 1'000'000;  // > any scenario's orderCount, keeps passes' ids disjoint
    MatcherType matcher;
    OrderManager manager(matcher);

    for (int pass = 0; pass < warmupPasses; ++pass)
        replay(manager, offsetIds(baseOps, static_cast<uint64_t>(pass) * kIdSpace));

    std::vector<std::vector<ScenarioOp>> measuredOps;
    measuredOps.reserve(static_cast<size_t>(measuredPasses));
    for (int pass = 0; pass < measuredPasses; ++pass)
        measuredOps.push_back(offsetIds(baseOps, static_cast<uint64_t>(warmupPasses + pass) * kIdSpace));

    const uint64_t startAlloc = gAllocCount.load();
    const uint64_t startDealloc = gDeallocCount.load();
    for (const auto& ops : measuredOps)
        replay(manager, ops);
    return AllocReport{gAllocCount.load() - startAlloc, gDeallocCount.load() - startDealloc};
}

}  // namespace

int main()
{
    const auto steadyOps = loadOps("steady_limit_flow");
    constexpr int kWarmupPasses = 5;
    constexpr int kMeasuredPasses = 10;
    const uint64_t measuredOpsTotal = steadyOps.size() * static_cast<uint64_t>(kMeasuredPasses);

    const AllocReport refReport = measureAllocs<ReferenceMatcher>(steadyOps, kWarmupPasses, kMeasuredPasses);
    const AllocReport optReport = measureAllocs<OptimizedMatcher>(steadyOps, kWarmupPasses, kMeasuredPasses);

    auto perMillion = [measuredOpsTotal](uint64_t count) {
        return static_cast<double>(count) * 1'000'000.0 / static_cast<double>(measuredOpsTotal);
    };

    std::cout << "scenario=steady_limit_flow ops_per_pass=" << steadyOps.size() << " warmup_passes="
              << kWarmupPasses << " measured_passes=" << kMeasuredPasses << " measured_ops=" << measuredOpsTotal
              << "\n";
    std::cout << "ReferenceMatcher: allocs=" << refReport.allocs << " deallocs=" << refReport.deallocs
              << " allocs_per_1M_ops=" << perMillion(refReport.allocs) << "\n";
    std::cout << "OptimizedMatcher: allocs=" << optReport.allocs << " deallocs=" << optReport.deallocs
              << " allocs_per_1M_ops=" << perMillion(optReport.allocs) << "\n";

    const BenchResult refLatency = run(BenchConfig{5, 30}, [&steadyOps]() {
        ReferenceMatcher matcher;
        OrderManager manager(matcher);
        replay(manager, steadyOps);
    });
    const BenchResult optLatency = run(BenchConfig{5, 30}, [&steadyOps]() {
        OptimizedMatcher matcher;
        OrderManager manager(matcher);
        replay(manager, steadyOps);
    });

    std::cout << "REF_LATENCY_P50_NS=" << refLatency.latency.percentile(50)
              << " REF_LATENCY_P99_NS=" << refLatency.latency.percentile(99) << "\n";
    std::cout << "OPT_LATENCY_P50_NS=" << optLatency.latency.percentile(50)
              << " OPT_LATENCY_P99_NS=" << optLatency.latency.percentile(99) << "\n";
    return 0;
}
