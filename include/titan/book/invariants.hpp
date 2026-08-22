#pragma once

#include <string>
#include <vector>

#include "titan/book/i_matcher.hpp"
#include "titan/exchange/order_manager.hpp"

namespace titan {

struct InvariantViolation {
    std::string name;
    std::string detail;
};

// Matcher-level invariants (full list + rationale in invariants.cpp):
//   - orderTable size matches actual resting-order count
//   - every orderTable entry's cached location really points at that id
//   - every resting order has quantity > 0
//   - every resting order's price matches the level it's stored under
//   - price levels are strictly ordered
//
// NOT checked, deliberately: "no crossed book at rest" (bestBid < bestAsk).
// It looks like an obvious invariant but isn't one here — addOrder() is
// documented as an unconditional rest that never attempts to cross, and
// every existing test relies on exactly that to seed arbitrary book
// states. See invariants.cpp for how fuzzing surfaced this.
//
// All of the above require deeper book introspection that only
// ReferenceMatcher exposes today, so they only run when `matcher` actually
// is one (checked via dynamic_cast) — a future optimized matcher would
// need its own equivalent introspection wired in here to get the same
// depth of checking. checkInvariants(const IMatcher&) still always
// succeeds (returns no violations) for any other implementation; it just
// can't check anything meaningful about it yet.
std::vector<InvariantViolation> checkInvariants(const IMatcher& matcher);

// Runs checkInvariants(matcher), plus OrderManager-specific invariants
// that cross-reference `manager`'s bookkeeping against the real book in
// `matcher` (again, deepest when `matcher` is a ReferenceMatcher):
//   - every id OrderManager considers active has a resting-consistent
//     status (New or PartiallyFilled)
//   - every id OrderManager considers active actually exists in the
//     matcher's order table
//   - OrderManager's shadow FIFO queues match the matcher's real FIFO
//     order at every price level, exactly
std::vector<InvariantViolation> checkOrderManagerInvariants(const OrderManager& manager, const IMatcher& matcher);

}  // namespace titan
