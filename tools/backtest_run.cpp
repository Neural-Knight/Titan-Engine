#include <fstream>
#include <iostream>
#include <string>

#include "titan/backtest/backtest_runner.hpp"

using namespace titan;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: backtest_run <path-to-itch-file> [--matcher reference|optimized] [--symbol SYM]\n";
        return 1;
    }

    const std::string path = argv[1];
    std::ifstream check(path, std::ios::binary);
    if (!check)
    {
        std::cerr << "cannot open file: " << path << "\n";
        return 1;
    }

    BacktestConfig config;
    std::string symbolFilter;
    for (int i = 2; i + 1 < argc; i += 2)
    {
        const std::string flag = argv[i];
        if (flag == "--matcher" && std::string(argv[i + 1]) == "optimized")
            config.backend = MatcherBackend::Optimized;
        else if (flag == "--symbol")
            symbolFilter = argv[i + 1];
    }

    BacktestRunner runner(config);
    const BacktestResult result = runner.replayFile(path);

    std::cout << "decoded: " << result.messagesDecoded << ", skipped: " << result.messagesSkipped << "\n";
    std::cout << "orders submitted=" << result.ordersSubmitted << " rejected=" << result.ordersRejected
               << " cancelled=" << result.ordersCancelled << " replaced=" << result.ordersReplaced << "\n";
    std::cout << "trades executed=" << result.tradesExecuted << " volume=" << result.totalTradeVolume << "\n";
    std::cout << "wall time: " << result.wallTimeNs << " ns\n";

    for (const auto& [symbol, book] : result.finalBook)
    {
        if (!symbolFilter.empty() && symbol != symbolFilter)
            continue;
        std::cout << "symbol " << symbol << ": bestBid=";
        if (!book.bids.empty())
            std::cout << book.bids[0].price << "@" << book.bids[0].quantity;
        else
            std::cout << "none";
        std::cout << " bestAsk=";
        if (!book.asks.empty())
            std::cout << book.asks[0].price << "@" << book.asks[0].quantity;
        else
            std::cout << "none";
        std::cout << "\n";
    }

    return 0;  // rejections are data, not a hard failure -- only an unreadable file is
}
