#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

#include "titan/feed/itch/parser.hpp"

using namespace titan;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: itch_dump <path-to-binary-file>\n";
        return 1;
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
    const auto results = parser.feed(bytes);

    std::map<char, size_t> perType;
    for (const auto& result : results)
        if (result.ok)
            ++perType[static_cast<char>(result.rawType)];

    for (const auto& [type, count] : perType)
        std::cout << type << ": " << count << "\n";

    std::cout << "total: " << parser.messagesDecoded() << " decoded, "
               << parser.messagesSkipped() << " skipped\n";
    return 0;
}
