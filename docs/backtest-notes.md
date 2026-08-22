# Backtesting framework

## `backtest_run` vs `replay_engine`

`replay_engine` is a **parity tool**: it replays a file and reports whether
the engine's book matches the feed-side `ItchBookBuilder` oracle, message
by message.

`backtest_run` is a **metrics tool**: it replays the same kind of file
through `BacktestRunner` and reports event/fill accounting -- orders
submitted/rejected/cancelled/replaced, trades executed, trade volume, and
the final book per symbol. It does not run `ParityChecker`; correctness of
the replay itself is `replay_engine`'s job, not this one's.

`BacktestRunner` (`include/titan/backtest/backtest_runner.hpp`) wraps
`EventReplayer` + `InstrumentRegistry` -- one fresh registry per
`replayFile()` call -- and derives every count directly from
`InstrumentRegistry::eventLog()`, the same event log `EventPublisher`
already consumes for `ExecutionReport`s. There is no separate fill-tracking
logic.

## Example

```
build/tools/backtest_run tests/fixtures/itch/sample_session.itch --matcher optimized
```

```
decoded: 5, skipped: 0
orders submitted=2 rejected=1 cancelled=0 replaced=0
trades executed=1 volume=100
wall time: 699916 ns
symbol AAPL: bestBid=none bestAsk=none
```

`--symbol SYM` filters which symbol's final book line is printed (all
symbols are still counted toward the aggregate metrics above it).

## Strategy hook (`IStrategy`, `include/titan/backtest/strategy.hpp`)

`replayFile(path, strategy)` takes an optional `IStrategy*`. Its hooks fire
**after the full file has replayed**, not live per-message:

- `onEvent` -- once per raw `Event` in `eventLog()`, in log order.
- `onExecutionReport` -- once per `ExecutionReport` an `EventPublisher`
  derives from that same log.
- `onBookUpdate` -- once per symbol, with that symbol's **final** book
  snapshot only (not one snapshot per message).

`IStrategy`'s methods default to no-ops, so it's directly usable as a
do-nothing strategy in tests without a separate subclass.

## Current scope

This is event/fill accounting only -- no PnL, no margin, no multi-account
simulation, no tick-database storage. A fuller backtest harness would also
want per-message (not post-hoc) strategy callbacks and a price oracle for
unrealized PnL; neither exists here yet.
