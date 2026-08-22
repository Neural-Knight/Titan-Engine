# ITCH feed notes

Covers ITCH 5.0 framing/decoding, the feed-side book builder, engine replay,
and TCP ingestion.

## Parser

`include/titan/feed/itch/messages.hpp` defines the decoded message types;
`ItchParser` (`parser.hpp`/`decoder.cpp`) handles framing and byte decode.

| Type | Name                      | Notes                                                                                                                                          |
| ---- | ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `S`  | System Event              | full fields                                                                                                                                    |
| `R`  | Stock Directory           | full ITCH 5.0 field list, 38-byte payload (includes `issueSubType` to match the real byte count) |
| `A`  | Add Order                 | no MPID                                                                                                                                        |
| `F`  | Add Order with MPID       | `AddOrderMessage` + 4-byte attribution                                                                                                         |
| `E`  | Order Executed            |                                                                                                                                                |
| `C`  | Order Executed With Price | `OrderExecutedMessage` + printable flag + price                                                                                                |
| `X`  | Order Cancel              |                                                                                                                                                |
| `D`  | Order Delete              |                                                                                                                                                |
| `U`  | Order Replace             |                                                                                                                                                |
| `T`  | Timestamp (seconds)       | **not part of real ITCH 5.0** -- see below                                                                                                     |

Real ITCH 5.0 has no standalone seconds-timestamp message; every message
carries its own 6-byte nanoseconds-since-midnight field instead. `T` is
modeled loosely on ITCH 4.1's seconds message, kept only to exercise
timestamp monotonicity in tests. A real feed handler should track time from
each message's own `timestamp` field, not from a `T` message.

### Framing

Each record: 2-byte big-endian length (covers the 1-byte type + payload),
then the type byte, then `length - 1` payload bytes. `ItchParser::feed()`
takes any chunk size, buffers a trailing partial record across calls, and
returns every complete record found. All multi-byte integer fields are
big-endian; `readU16BE`/`readU32BE`/`readU48BE`/`readU64BE` in
`decoder.cpp` do the byte-order flip by hand (no `struct`-overlay/reinterpret
tricks, since payloads aren't guaranteed alignment and the wire format is
big-endian on what's usually a little-endian host).

Price fields are fixed-point: wire value = real price × 10000.

### Corrupt input handling

`ItchParser` never throws or reads out of bounds:

- length == 0, or length > `kMaxMessageSize` (8192 -- a sanity cap, not a
protocol limit; real messages are all under ~60 bytes): the 2-byte length
field is dropped and scanning resumes from the next byte. Counted in
`messagesSkipped()`, not added to the returned result vector.
- Unknown type byte, or a payload size that doesn't match the known type's
fixed layout: `decodeMessage` returns `false`; the caller gets an
`ItchParseResult{ok=false, rawType, ...}` so it can still see and count
what was skipped. The raw payload bytes for that record are not retained in
the result -- exposing them would mean copying out of the internal buffer
before it's erased.

## Book builder

`include/titan/feed/itch/book_builder.hpp` (`ItchBookBuilder`) turns decoded
`ItchMessage`s into per-`stockLocate` resting-order state. It's feed-side
only -- a standalone oracle used for parity checks, not wired into
`InstrumentRegistry`/`ReferenceMatcher` directly (see Engine replay below).

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
- `S` with `eventCode == 'C'` (End of Messages, the real ITCH 5.0 code)
clears all book state via `reset()`. Real ITCH 5.0 uses `'E'` for End of
System Hours and `'C'` for End of Messages; `'C'` is used here to stay
spec-correct.
- `T` (`TimestampSecondsMessage`) carries no `stockLocate` and is ignored.

## Engine replay

`titan_replay` (`ItchEngineAdapter`, `ParityChecker`, `EventReplayer`) drives
`InstrumentRegistry` from the same decoded messages `ItchBookBuilder` sees,
so the exchange-side book can be checked against the feed-side one.

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
that price -- true for the single-order-per-level sessions the tests use,
but not guaranteed in general (see limitations below).
- `U` (Replace) is the one ITCH message `OrderManager::cancelReplace` can't
model directly: ITCH assigns a *new* reference number, but `cancelReplace`
only allows a same-id amend. The adapter keeps the replaced order under its
original engine `OrderId` and maintains its own
`orderReferenceNumber -> OrderId` map, so later messages against the new
ITCH ref resolve to the same underlying engine order.
- `S` with `eventCode == 'C'` resets both the adapter's own maps and the
`InstrumentRegistry` (`InstrumentRegistry::reset()`), mirroring
`ItchBookBuilder::reset()`.

Run: `build/tools/replay_engine <path> [--checkpoint N] [--matcher reference|optimized]`
-- prints decoded/skipped counts, parity summary, first mismatches, and feed
vs. engine trade volume; exits 1 if any mismatch was found. Parity is
verified against both `InstrumentRegistry` matcher backends.

### Known limitations

- Only the ITCH types the parser implements are handled; anything else was
already dropped at the decode stage.
- Parity compares aggregated top-of-book level quantity, not per-order FIFO
identity within a level -- if two resting orders share a price and an `E`
targets the one behind the front, the synthetic-order approach above would
hit the front order instead. Not exercised by the hand-crafted test sessions.

## TCP feed

`titan/feed/tcp/socket_feed.hpp` (`SocketFeedReader`) connects out as a TCP
*client* to `host:port` and runs recv + `ItchParser::feed()` + a message
callback on one dedicated thread -- parsing stays single-threaded since
`ItchParser` isn't thread-safe, so there's no second internal queue hop for
the byte-to-message step. `titan/feed/tcp/itch_socket_pipeline.hpp`
(`ItchSocketPipeline`) wires that callback into the same
`ItchBookBuilder` + `ItchEngineAdapter` + `ParityChecker` combination
`EventReplayer` uses for files, kept alive across the whole TCP session
instead of one-shot per call. Neither `ItchParser` nor `ItchEngineAdapter`
was changed for this -- it only adds a byte source.

Run: `build/tools/itch_listen --host H --port P [--matcher reference|optimized] [--checkpoint N]`
-- connects as a client, prints the same decoded/skipped/parity/trade-volume
summary shape as `replay_engine`. This is a client for a real NASDAQ-style
feed (the exchange is the server); for local testing, run any TCP server
that writes ITCH-framed bytes first (see
`tests/integration/test_socket_feed.cpp`'s `LoopbackServer` for a minimal one).

Sockets use plain POSIX (`socket`/`connect`/`poll`/`recv`/`shutdown`) --
identical on macOS and Linux, no `#ifdef` needed for the default path.
`stop()` unblocks a pending `recv`/`poll` via `shutdown(fd, SHUT_RDWR)` from
another thread, with a 200ms `poll()` timeout as a backstop either way.
Linux-only fast paths (`epoll`, `SO_BUSY_POLL`) were considered and
deliberately not implemented -- `poll()` covers the current scope and keeps
macOS and Linux on one code path. Not a production NASDAQ connectivity
claim -- local/mock servers only, proven via loopback tests.

## Tools

`build/tools/itch_dump <path>` decodes a file and prints a per-type message
count, then a `total: N decoded, M skipped` line.

NASDAQ publishes sample TotalView-ITCH files for testing at
`https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/` (dated `.NASDAQ_ITCH50.gz`
files). Download one, `gunzip` it, and point `itch_dump` at the raw `.bin`.
The tests here use only a hand-crafted fixture
(`tests/fixtures/itch/sample_session.itch`) -- no real NASDAQ file is
bundled in this repo.
