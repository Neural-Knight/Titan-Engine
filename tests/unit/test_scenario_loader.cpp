#include <gtest/gtest.h>

#include "titan/bench/scenario.hpp"

using namespace titan;

TEST(ScenarioLoader, ParsesSteadyLimitFlowFile) {
    const Scenario scenario = loadScenario(std::string(TITAN_SCENARIOS_DIR) + "/steady_limit_flow.txt");

    EXPECT_EQ(scenario.name, "steady_limit_flow");
    EXPECT_EQ(scenario.symbolCount, 1u);
    EXPECT_EQ(scenario.orderCount, 20000u);
    EXPECT_EQ(scenario.seed, 42u);
    EXPECT_DOUBLE_EQ(scenario.addWeight, 0.8);
    EXPECT_DOUBLE_EQ(scenario.matchWeight, 0.2);
}

TEST(ScenarioLoader, GenerateOrdersIsDeterministicForSameSeed) {
    Scenario scenario;
    scenario.symbolCount = 4;
    scenario.orderCount = 500;
    scenario.addWeight = 0.5;
    scenario.cancelWeight = 0.2;
    scenario.matchWeight = 0.3;

    const auto first = scenario.generateOrders(7);
    const auto second = scenario.generateOrders(7);

    ASSERT_EQ(first.size(), second.size());
    for (size_t i = 0; i < first.size(); ++i) {
        EXPECT_EQ(first[i].type, second[i].type);
        EXPECT_EQ(first[i].symbolIndex, second[i].symbolIndex);
        EXPECT_EQ(first[i].orderId, second[i].orderId);
        EXPECT_EQ(first[i].side, second[i].side);
        EXPECT_EQ(first[i].price, second[i].price);
        EXPECT_EQ(first[i].quantity, second[i].quantity);
    }
}

TEST(ScenarioLoader, DifferentSeedsProduceDifferentOrders) {
    Scenario scenario;
    scenario.symbolCount = 4;
    scenario.orderCount = 500;
    scenario.addWeight = 0.5;
    scenario.cancelWeight = 0.2;
    scenario.matchWeight = 0.3;

    const auto first = scenario.generateOrders(7);
    const auto second = scenario.generateOrders(8);

    bool anyDifference = false;
    for (size_t i = 0; i < first.size(); ++i) {
        if (first[i].orderId != second[i].orderId || first[i].price != second[i].price) {
            anyDifference = true;
            break;
        }
    }
    EXPECT_TRUE(anyDifference);
}

TEST(ScenarioLoader, GeneratedOrderCountMatchesRequest) {
    Scenario scenario;
    scenario.symbolCount = 1;
    scenario.orderCount = 1000;
    scenario.addWeight = 1.0;

    const auto ops = scenario.generateOrders(1);
    EXPECT_EQ(ops.size(), 1000u);
}
