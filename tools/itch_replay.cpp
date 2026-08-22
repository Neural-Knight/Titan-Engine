#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "titan/feed/itch/book_builder.hpp"
#include "titan/feed/itch/parser.hpp"

using namespace titan;

namespace {

void printTopOfBook(const ItchBookBuilder& builder, StockLocate locate)
{
    const auto sym = builder.symbolForLocate(locate);
    const auto book = builder.snapshot(locate, 1);
    if (!sym || !book)
        return;

    std::cout << "  " << *sym << ": bid=";
    if (!book->bids.empty())
        std::cout << book->bids[0].quantity << "@" << book->bids[0].price;
    else
        std::cout << "-";
    std::cout << " ask=";
    if (!book->asks.empty())
        std::cout << book->asks[0].quantity << "@" << book->asks[0].price;
    else
        std::cout << "-";
    std::cout << "\n";
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: itch_replay <path-to-itch-file> [--depth N] [--every M]\n";
        return 1;
    }

    size_t depth = 1;
    size_t every = 1000;
    for (int i = 2; i + 1 < argc; i += 2)
    {
        const std::string flag = argv[i];
        if (flag == "--depth")
            depth = static_cast<size_t>(std::atoi(argv[i + 1]));
        else if (flag == "--every")
            every = static_cast<size_t>(std::atoi(argv[i + 1]));
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file)
    {
        std::cerr << "cannot open: " << argv[1] << "\n";
        return 1;
    }

    const std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    ItchParser parser;
    ItchBookBuilder builder;
    const auto results = parser.feed(bytes);

    std::set<StockLocate> symbolsSeen;
    size_t applied = 0;
    for (size_t i = 0; i < results.size(); ++i)
    {
        const auto& result = results[i];
        if (!result.ok)
            continue;
        builder.apply(result.message);
        ++applied;

        if (const auto* add = std::get_if<AddOrderMessage>(&result.message))
            symbolsSeen.insert(add->stockLocate);
        else if (const auto* dir = std::get_if<StockDirectoryMessage>(&result.message))
            symbolsSeen.insert(dir->stockLocate);

        if (applied % every == 0)
        {
            std::cout << "-- after " << applied << " messages --\n";
            for (StockLocate locate : symbolsSeen)
                printTopOfBook(builder, locate);
        }
    }

    std::cout << "-- final --\n";
    for (StockLocate locate : symbolsSeen)
        printTopOfBook(builder, locate);

    std::cout << "symbols seen: " << symbolsSeen.size() << ", messages applied: " << applied
               << ", skipped: " << parser.messagesSkipped() << "\n";
    return 0;
}
