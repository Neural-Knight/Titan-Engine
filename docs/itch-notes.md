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

## Deferred to Module 10 (book builder)

- Turning `AddOrder`/`OrderExecuted`/`OrderCancel`/`OrderDelete`/`OrderReplace`
into actual resting-order state per symbol.
- Using `StockDirectory`'s stock-locate -> symbol mapping to route decoded
messages to the right `InstrumentRegistry` symbol.
- Any notion of session start/end (`S` event codes) driving book lifecycle.

