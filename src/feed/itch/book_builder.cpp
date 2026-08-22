#include "titan/feed/itch/book_builder.hpp"

#include <algorithm>
#include <string_view>
#include <variant>

namespace titan {

namespace {

std::string trimTrailingSpaces(const std::array<char, 8>& stock)
{
    std::string_view view(stock.data(), stock.size());
    const size_t end = view.find_last_not_of(' ');
    return end == std::string_view::npos ? std::string{} : std::string(view.substr(0, end + 1));
}

}  // namespace

void ItchBookBuilder::apply(const ItchMessage& message)
{
    std::visit([this](const auto& msg) { handle(msg); }, message);
}

ItchBookBuilder::SymbolBook* ItchBookBuilder::findBook(StockLocate locate)
{
    const auto it = books_.find(locate);
    return it == books_.end() ? nullptr : &it->second;
}

void ItchBookBuilder::addLevel(SymbolBook& book, Side side, Price price, Quantity qty)
{
    auto& levels = (side == Side::Buy) ? book.bidLevels : book.askLevels;
    levels[price] += qty;
}

void ItchBookBuilder::removeLevel(SymbolBook& book, Side side, Price price, Quantity qty)
{
    auto& levels = (side == Side::Buy) ? book.bidLevels : book.askLevels;
    const auto it = levels.find(price);
    if (it == levels.end())
        return;
    if (qty >= it->second)
        levels.erase(it);
    else
        it->second -= qty;
}

void ItchBookBuilder::addOrder(SymbolBook& book, OrderRefNumber ref, Side side, Quantity shares, Price price)
{
    book.orders[ref] = RestingOrder{side, price, shares};
    addLevel(book, side, price, shares);
}

void ItchBookBuilder::reduceOrder(SymbolBook& book, OrderRefNumber ref, Quantity shares)
{
    const auto it = book.orders.find(ref);
    if (it == book.orders.end())
        return;
    RestingOrder& order = it->second;
    const Quantity reduceBy = std::min(shares, order.remainingShares);
    removeLevel(book, order.side, order.price, reduceBy);
    if (reduceBy >= order.remainingShares)
        book.orders.erase(it);
    else
        order.remainingShares -= reduceBy;
}

void ItchBookBuilder::deleteOrder(SymbolBook& book, OrderRefNumber ref)
{
    const auto it = book.orders.find(ref);
    if (it == book.orders.end())
        return;
    removeLevel(book, it->second.side, it->second.price, it->second.remainingShares);
    book.orders.erase(it);
}

void ItchBookBuilder::handle(const SystemEventMessage& message)
{
    // Real ITCH 5.0 uses 'C' for End of Messages ('E' is End of System Hours);
    // clearing here on 'C' is the spec-correct trigger for session-end teardown.
    if (message.eventCode == 'C')
        reset();
}

void ItchBookBuilder::handle(const StockDirectoryMessage& message)
{
    // Registers locate -> symbol only; never touches resting-order state.
    books_[message.stockLocate].symbol = trimTrailingSpaces(message.stock);
}

void ItchBookBuilder::handle(const AddOrderMessage& message)
{
    SymbolBook* book = findBook(message.stockLocate);
    if (!book)
        return;
    const Side side = (message.buySellIndicator == 'B') ? Side::Buy : Side::Sell;
    addOrder(*book, message.orderReferenceNumber, side, message.shares, message.price);
}

void ItchBookBuilder::handle(const AddOrderMpidMessage& message)
{
    handle(message.base);
}

void ItchBookBuilder::handle(const OrderExecutedMessage& message)
{
    SymbolBook* book = findBook(message.stockLocate);
    if (!book)
        return;
    reduceOrder(*book, message.orderReferenceNumber, message.executedShares);
}

void ItchBookBuilder::handle(const OrderExecutedWithPriceMessage& message)
{
    handle(message.base);
}

void ItchBookBuilder::handle(const OrderCancelMessage& message)
{
    SymbolBook* book = findBook(message.stockLocate);
    if (!book)
        return;
    reduceOrder(*book, message.orderReferenceNumber, message.cancelledShares);
}

void ItchBookBuilder::handle(const OrderDeleteMessage& message)
{
    SymbolBook* book = findBook(message.stockLocate);
    if (!book)
        return;
    deleteOrder(*book, message.orderReferenceNumber);
}

void ItchBookBuilder::handle(const OrderReplaceMessage& message)
{
    SymbolBook* book = findBook(message.stockLocate);
    if (!book)
        return;
    const auto it = book->orders.find(message.originalOrderReferenceNumber);
    if (it == book->orders.end())
        return;
    const Side side = it->second.side;
    deleteOrder(*book, message.originalOrderReferenceNumber);
    addOrder(*book, message.newOrderReferenceNumber, side, message.shares, message.price);
}

void ItchBookBuilder::handle(const TimestampSecondsMessage&)
{
    // No stockLocate on this synthetic message type; nothing to apply.
}

std::optional<BookSnapshot> ItchBookBuilder::snapshot(StockLocate locate, size_t depth) const
{
    const auto it = books_.find(locate);
    if (it == books_.end())
        return std::nullopt;
    const SymbolBook& book = it->second;

    BookSnapshot result;
    result.symbol = book.symbol;
    result.sequenceNumber = book.nextSnapshotSequenceNumber++;

    for (auto rit = book.bidLevels.rbegin(); rit != book.bidLevels.rend() && result.bids.size() < depth; ++rit)
        result.bids.push_back(PriceLevel{rit->first, rit->second});
    for (auto ait = book.askLevels.begin(); ait != book.askLevels.end() && result.asks.size() < depth; ++ait)
        result.asks.push_back(PriceLevel{ait->first, ait->second});

    return result;
}

std::optional<std::string> ItchBookBuilder::symbolForLocate(StockLocate locate) const
{
    const auto it = books_.find(locate);
    return it == books_.end() ? std::nullopt : std::optional<std::string>(it->second.symbol);
}

size_t ItchBookBuilder::orderCount(StockLocate locate) const
{
    const auto it = books_.find(locate);
    return it == books_.end() ? 0 : it->second.orders.size();
}

void ItchBookBuilder::reset()
{
    books_.clear();
}

}  // namespace titan
