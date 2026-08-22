#pragma once

namespace titan {

// Self-trade-prevention mode: how OrderManager reacts when matching would
// pair an incoming order against a resting order owned by the same
// accountId.
//
// CancelIncoming (the only mode implemented): as soon as matching would
// hit a self-owned resting order, matching stops entirely for that
// incoming order. It does NOT skip past the self-owned order to reach
// further, non-self liquidity resting behind it at worse prices — from
// the incoming order's point of view, the book effectively "ends" at the
// self-collision. Quantity already filled against OTHER accounts before
// the collision stands; whatever is left unfilled is handled exactly like
// any other unfilled remainder (discarded for Market/IOC, rested for GTC
// Limit). The blocked resting order itself is left completely untouched.
//
// AccountId 0 is treated as "no account assigned" and is exempt from STP
// (two orders with accountId 0 are never considered the same account).
// Every order built without explicitly setting accountId defaults to 0,
// so this keeps unrelated orders that don't care about STP from
// accidentally colliding with each other.
enum class StpMode {
    CancelIncoming
};

}  // namespace titan
