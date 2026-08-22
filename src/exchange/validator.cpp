#include "titan/exchange/validator.hpp"

namespace titan {

RejectReason validateNewOrder(const Order& order, const std::unordered_set<OrderId>& existingIds)
{
    if (order.quantity == 0)
        return RejectReason::ZeroQuantity;

    if (order.type == OrderType::Limit && order.price == 0)
        return RejectReason::ZeroPrice;

    if (existingIds.count(order.id) > 0)
        return RejectReason::DuplicateOrderId;

    return RejectReason::None;
}

RejectReason validateCancel(OrderId id, const std::unordered_set<OrderId>& existingIds)
{
    if (existingIds.count(id) == 0)
        return RejectReason::UnknownOrder;

    return RejectReason::None;
}

}  // namespace titan
