# ITCH 5.0 parser

Ingestion only: framing + byte-level decode into typed structs. No book
reconstruction (Module 10), no `InstrumentRegistry` wiring (Module 11), no
sockets.

## Message types implemented

`include/titan/feed/itch/messages.hpp`:


| Type | Name                      | Notes                                                                                                                                          |
| ---- | ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `S`  | System Event              | full fields                                                                                                                                    |
| `R`  | Stock Directory           | full ITCH 5.0 field list, 38-byte payload (added `issueSubType` beyond this module's original field list to hit the real byte count) |
| `A`  | Add Order                 | no MPID                                                                                                                                        |
| `F`  | Add Order with MPID       | `AddOrderMessage` + 4-byte attribution                                                                                                         |
| `E`  | Order Executed            |                                                                                                                                                |
| `C`  | Order Executed With Price | `OrderExecutedMessage` + printable flag + price                                                                                                |
| `X`  | Order Cancel              |                                                                                                                                                |
| `D`  | Order Delete              |                                                                                                                                                |
| `U`  | Order Replace             |                                                                                                                                                |
| `T`  | Timestamp (seconds)       | **not part of real ITCH 5.0** -- see below                                                                                                     |


Real ITCH 5.0 has no standalone seconds-timestamp message; every message
carries its own 6-byte nanoseconds-since-midnight field instead. `T` here is
modeled loosely on ITCH 4.1's seconds message, included only because this
module's test list asked for one to exercise timestamp monotonicity. A real
feed handler should track time from each message's own `timestamp` field, not
from a `T` message.

## Framing

Each record: 2-byte big-endian length (covers the 1-byte type + payload),
then the type byte, then `length - 1` payload bytes. `ItchParser::feed()`
takes any chunk size, buffers a trailing partial record across calls, and
returns every complete record found. All multi-byte integer fields are
big-endian; `readU16BE`/`readU32BE`/`readU48BE`/`readU64BE` in
`decoder.cpp` do the byte-order flip by hand (no `struct`-overlay/reinterpret
tricks, since payloads aren't guaranteed alignment and the wire format is
big-endian on what's usually a little-endian host).

Price fields are fixed-point: wire value = real price × 10000.

## Corrupt input handling

`ItchParser` never throws or reads out of bounds:

- length == 0, or length > `kMaxMessageSize` (8192 -- a sanity cap, not a
protocol limit; real messages are all under ~60 bytes): the 2-byte length
field is dropped and scanning resumes from the next byte. Counted in
`messagesSkipped()`, not added to the returned result vector.
- Unknown type byte, or a payload size that doesn't match the known type's
fixed layout: `decodeMessage` returns `false`; the caller gets an
`ItchParseResult{ok=false, rawType, ...}` so it can still see and count
what was skipped. (The raw payload bytes for that record are not retained
in the result -- the task marked exposing them as optional, and doing so
safely would mean copying out of the internal buffer before it's erased.)



## Running `itch_dump`

```
build/tools/itch_dump tests/fixtures/itch/sample_session.itch
```

Prints a decoded-message count per type, then a `total: N decoded, M skipped`
line.

## Getting real ITCH sample data

NASDAQ publishes sample TotalView-ITCH files for testing at
`https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/` (dated `.NASDAQ_ITCH50.gz`
files). Download one, `gunzip` it, and point `itch_dump` at the raw `.bin`.
This module's own tests use only hand-crafted fixtures
(`tests/fixtures/itch/sample_session.itch`, generated for this session) --
no real NASDAQ file is bundled here.

## Module 10: Book Builder

`include/titan/feed/itch/book_builder.hpp` (`ItchBookBuilder`) turns decoded
`ItchMessage`s into per-`stockLocate` resting-order state, feed-side only --
not wired into `InstrumentRegistry`/`ReferenceMatcher` (Module 11).

- Message types that affect book state: `R` (registers locate -> symbol, no
book change), `A`/`F` (insert resting order), `E`/`C` (reduce by executed
shares, remove at zero), `X` (reduce by cancelled shares, remove at zero),
`D` (remove entirely), `U` (remove old ref, insert new ref at the new
price/shares -- side is inherited from the original order, since real ITCH
Order Replace carries no side field of its own).
- `orderReferenceNumber` is the key for all per-order lookups; it is unique
per `stockLocate` for the life of the resting order.
- `A`/`E`/`C`/`X`/`D`/`U` for a `stockLocate` with no prior `R` are ignored
outright (not queued) -- there is no symbol to attribute them to yet.
- `S` with `eventCode == 'C'` (End of Messages, the real ITCH 5.0 code) clears
all book state via `reset()`. The task prompt that requested this loosely
called the trigger "'E' (end of messages)"; real ITCH 5.0 uses `'E'` for End
of System Hours and `'C'` for End of Messages, so `'C'` was used to stay
spec-correct.
- `T` (`TimestampSecondsMessage`) carries no `stockLocate` and is ignored, as
elsewhere in this module.

Deferred to Module 11: feeding `ItchBookBuilder` state into
`InstrumentRegistry` so a real feed can drive the exchange-side book.

## Module 11: Engine Replay

`titan_replay` (`ItchEngineAdapter`, `ParityChecker`, `EventReplayer`) drives
`InstrumentRegistry` from the same decoded messages `ItchBookBuilder` sees, so
the exchange-side book can be checked against the feed-side one.

- `R`/`A`/`F`/`D` map onto `createInstrument`/`submitOrder`/`cancelOrder`
directly, using `orderReferenceNumber` as the `OrderId`.
- `X` (partial cancel) maps to `cancelReplace` at the same price with the
reduced quantity (or `cancelOrder` if it empties the order) -- ITCH cancels
don't lose queue priority, which matches `cancelReplace`'s same-price,
qty-decrease fast path.
- `E`/`C` have no passive-execution API on `OrderManager`, so the adapter
injects a synthetic IOC limit on the opposite side, sized to the executed
shares, at the execution price (`C`) or the resting order's own price (`E`).
Synthetic incoming ids start at 1,000,000,000 to stay clear of ITCH
reference numbers. This assumes the targeted resting order is FIFO-front at
that price -- true for the single-order-per-level sessions this module's
tests use, but not guaranteed in general (see limitations below).
- `U` (Replace) is the one ITCH message `OrderManager::cancelReplace` can't
model directly: ITCH assigns a *new* reference number, but
`cancelReplace` only allows a same-id amend. The adapter keeps the
replaced order under its original engine `OrderId` and maintains its own
`orderReferenceNumber -> OrderId` map, so later messages against the new
ITCH ref resolve to the same underlying engine order.
- `S` with `eventCode == 'C'` resets both the adapter's own maps and the
`InstrumentRegistry` (a new `InstrumentRegistry::reset()`, purely additive),
mirroring `ItchBookBuilder::reset()`.

Run: `build/tools/replay_engine <path> [--checkpoint N]` -- prints
decoded/skipped counts, parity summary, first mismatches, and feed vs. engine
trade volume; exits 1 if any mismatch was found.

### Known limitations

- Only the ITCH types this module's parser implements are handled; anything
else was already dropped at the decode stage.
- Parity compares aggregated top-of-book level quantity, not per-order FIFO
identity within a level -- if two resting orders share a price and an `E`
targets the one behind the front, the synthetic-order approach above would
hit the front order instead. Not exercised by this module's hand-crafted
sessions.

