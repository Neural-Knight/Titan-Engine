#include <gtest/gtest.h>

#include <random>
#include <vector>

#include "generators.hpp"
#include "reference/order_book.hpp"
#include "titan/book/invariants.hpp"
#include "titan/exchange/order_manager.hpp"

using namespace titan;
using namespace titan::fuzz;

namespace {

constexpr unsigned kSeed = 42;
constexpr int kOpCount = 10000;

// Fails on the first violation, tagged with the op index for repro.
void assertNoViolations(const std::vector<InvariantViolation>& violations, int opIndex)
{
    if (violations.empty())
        return;
    FAIL() << "Invariant violated after op #" << opIndex << ": " << violations.front().name
           << " -- " << violations.front().detail
           << " (" << violations.size() << " total violation(s) at this point)";
}

}  // namespace

// Drives ReferenceMatcher directly. CancelReplace has no IMatcher
// equivalent, so it's folded into Add for this target.
TEST(FuzzReferenceMatcher, RandomOperationsPreserveInvariants) {
	ReferenceMatcher matcher;
	std::mt19937 rng(kSeed);
	std::vector<OrderId> restingIds;
	OrderId nextId = 1;

	for (int i = 0; i < kOpCount; ++i) {
		FuzzOp op = randomOp(rng);
		if (op == FuzzOp::CancelReplace)
			op = FuzzOp::Add;

		restingIds.clear();
		for (const auto& entry : matcher.getOrderTable())
			restingIds.push_back(entry.first);

		switch (op) {
			case FuzzOp::Add: {
				const Order order = randomOrder(rng, nextId++);
				if (order.quantity > 0)  // ReferenceMatcher has no validation layer of its own
					matcher.addOrder(order);
				break;
			}
			case FuzzOp::Match: {
				const Order order = randomOrder(rng, nextId++);
				if (order.quantity > 0)
					matcher.matchOrder(order);
				break;
			}
			case FuzzOp::Cancel: {
				const OrderId target =
					restingIds.empty() ? nextId : restingIds[rng() % restingIds.size()];
				matcher.cancelOrder(target);
				break;
			}
			case FuzzOp::CancelReplace:
				break;  // unreachable, remapped to Add above
		}

		assertNoViolations(checkInvariants(matcher), i);
		if (::testing::Test::HasFailure())
			return;
	}
}

// Drives OrderManager through random add/match/cancel/cancelReplace ops.
TEST(FuzzOrderManager, RandomOperationsPreserveInvariants) {
	ReferenceMatcher matcher;
	OrderManager manager(matcher);
	std::mt19937 rng(kSeed);
	std::vector<OrderId> restingIds;
	OrderId nextId = 1;

	for (int i = 0; i < kOpCount; ++i) {
		const FuzzOp op = randomOp(rng);

		restingIds.assign(manager.activeOrderIdsForInvariants().begin(),
		                   manager.activeOrderIdsForInvariants().end());

		switch (op) {
			case FuzzOp::Add: {
				const Order order = randomOrder(rng, nextId++);
				manager.addOrder(order);  // rejections (Market/IOC/invalid) are expected and fine
				break;
			}
			case FuzzOp::Match: {
				const Order order = randomOrder(rng, nextId++);
				manager.matchOrder(order);
				break;
			}
			case FuzzOp::Cancel: {
				const OrderId target =
					restingIds.empty() ? nextId : restingIds[rng() % restingIds.size()];
				manager.cancelOrder(target);
				break;
			}
			case FuzzOp::CancelReplace: {
				if (restingIds.empty())
					break;
				const OrderId oldId = restingIds[rng() % restingIds.size()];
				Order newOrder = randomOrder(rng, oldId);
				newOrder.id = oldId;  // same-id amend only
				manager.cancelReplace(oldId, newOrder);
				break;
			}
		}

		assertNoViolations(checkOrderManagerInvariants(manager, matcher), i);
		if (::testing::Test::HasFailure())
			return;
	}
}
