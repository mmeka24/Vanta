# Vanta V1 Build Plan

This file is the source of truth for building Vanta. Complete one chunk at a
time. Do not ask an AI coding assistant to implement the entire project in one
prompt.

## What V1 is

Vanta V1 is a single-symbol, event-driven trading research engine. It records
live AAPL market data from Alpaca, replays that data through the exact same C++
engine, calculates a small set of market features, generates deterministic trade
proposals, applies risk checks, simulates execution, and can later submit tightly
limited orders to an Alpaca paper account.

V1 is successful when one recorded session can be replayed twice and produce
identical decisions, orders, fills, positions, and P&L.

## What V1 is not

- A profitable trading system
- A production high-frequency trading platform
- A real-money trading bot
- An options engine
- A multi-symbol portfolio system
- A full-depth exchange order book
- An LLM with permission to place trades
- A replacement for professional market data

Do not add these features before V1 is finished.

## Fixed technical choices

| Area | V1 choice |
| --- | --- |
| Core language | C++20 |
| Build system | CMake with CMake presets |
| Dependencies | vcpkg |
| Tests | Catch2 |
| JSON | nlohmann/json |
| WebSockets | IXWebSocket |
| HTTP orders | CPR |
| Logging | spdlog |
| Data format | NDJSON |
| Analysis | Python, pandas, matplotlib |
| Market | US equities |
| Symbol | AAPL |
| Broker | Simulated broker, then Alpaca paper |
| Concurrency | Network I/O threads and one state-owning engine thread |
| Price storage | Signed 64-bit fixed-point integers |
| Live trading | Explicitly disabled in V1 |

## Rules for every AI coding session

Give the assistant the base prompt below followed by exactly one chunk prompt.

~~~text
Read README.md, ARCHITECTURE.md, and TASKS.md before changing anything.

Implement only the requested chunk. Do not implement later chunks, redesign the
architecture, or add unrelated abstractions. Preserve existing behavior and
tests. Never place credentials in source code, logs, fixtures, or commits.
Never enable real-money trading.

Before coding, briefly state:
1. what files you expect to change,
2. the interfaces you will introduce,
3. how you will test the work.

After coding:
1. format the changed code,
2. build from a clean build directory,
3. run the complete test suite,
4. report the commands and results,
5. list changed files and any remaining limitations.

Do not mark a TASKS.md checkbox complete unless its acceptance criteria pass.
If an assumption is unclear, stop and ask instead of silently expanding scope.
~~~

Commit after each completed chunk. Suggested commit format:

~~~text
feat(component): short description
test(component): short description
fix(component): short description
~~~

---

## Chunk 0 — Project foundation

### Goal

Create a clean C++ project that is easy to build and test on macOS and in CI.

### Build

- [x] Add the root CMakeLists.txt and require C++20
- [x] Add CMakePresets.json with debug and release presets
- [x] Add vcpkg.json with pinned project dependencies
- [x] Add src/main.cpp that prints the Vanta version and exits successfully
- [x] Add Catch2 with one passing smoke test
- [x] Add .clang-format and strict compiler warnings
- [x] Add AddressSanitizer and UndefinedBehaviorSanitizer in the debug preset
- [x] Add .gitignore for build output, credentials, logs, and recorded data
- [ ] Add GitHub Actions to configure, build, and test (workflow written; unverified
      until the first push runs it)

### Expected structure

~~~text
vanta/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── include/vanta/
├── src/main.cpp
├── tests/
├── config/
├── data/
└── tools/reports/
~~~

### Acceptance criteria

- A clean debug configure and build succeeds
- CTest reports one passing test
- Running the executable prints a version
- No API keys or generated build files are tracked
- CI runs the same build and tests

### Chunk prompt

~~~text
Implement Chunk 0 from TASKS.md. Set up the minimal C++20/CMake/vcpkg project,
one executable, one Catch2 smoke test, debug sanitizers, formatting, warnings,
.gitignore, and GitHub Actions. Keep main.cpp trivial. Do not add trading logic
or Alpaca code.
~~~

---

## Chunk 1 — Core event and money types

### Goal

Define the internal language used by every later component without connecting to
a network.

### Build

- [ ] Add SequenceNumber and Timestamp types
- [ ] Add a fixed-point Price type backed by int64_t
- [ ] Parse decimal prices without using floating point
- [ ] Format Price back to its canonical decimal representation
- [ ] Define RawFrame with sequence, source, arrival timestamp, and payload
- [ ] Define Quote, Trade, ConnectionState, and OrderUpdate event types
- [ ] Define one Event variant containing all typed events
- [ ] Add equality and serialization helpers needed for deterministic tests

Use one documented scale for prices, such as 1 unit = 0.0001 USD. Overflow and
invalid decimal input must return errors rather than silently rounding.

### Tests

- Valid positive price parsing and formatting
- Minimum tick precision
- Too many decimal places
- Negative values where disallowed
- int64 overflow
- Equality for event values
- Stable serialization output

### Acceptance criteria

- Domain types contain no Alpaca-specific networking code
- Price calculations do not use float or double
- All edge-case tests pass under sanitizers

### Chunk prompt

~~~text
Implement Chunk 1 from TASKS.md. Create the fixed-point Price type, RawFrame,
typed market events, and the Event variant with focused unit tests. Use explicit
error handling for malformed prices and overflow. Do not add queues, sockets,
market state, strategies, or brokers.
~~~

---

## Chunk 2 — Bounded queue and raw recorder

### Goal

Move raw frames safely between producer threads and a consumer while preserving
the exact external payload.

### Build

- [ ] Implement a generic bounded blocking queue
- [ ] Support push, pop, close, capacity, and current size
- [ ] Define explicit behavior when closed
- [ ] Implement an append-only NDJSON RawRecorder
- [ ] Store sequence, source, arrival timestamp, and exact payload
- [ ] Flush cleanly during shutdown
- [ ] Never record API keys or authentication messages
- [ ] Add a small queue-depth metric

### Tests

- FIFO order
- Producer blocks when full and continues when space appears
- Consumer wakes when data arrives
- Waiting threads wake when the queue closes
- Recorder round-trips payloads containing escaped JSON
- Multiple frames preserve sequence order
- Closing and flushing does not lose accepted frames

### Acceptance criteria

- ThreadSanitizer or an equivalent concurrency test reports no data race
- A generated raw log can be parsed one line at a time
- The stored payload is byte-for-byte equal to the input payload

### Chunk prompt

~~~text
Implement Chunk 2 from TASKS.md. Add a bounded blocking queue and append-only
NDJSON RawRecorder with concurrency and round-trip tests. Define clean close and
shutdown semantics. Do not connect to Alpaca or parse market messages yet.
~~~

---

## Chunk 3 — Alpaca market-data recorder

### Goal

Record real AAPL quote and trade frames without making any trading decisions.

### Build

- [ ] Load the feed URL, key ID, secret, feed, and symbol from environment
- [ ] Fail safely when required configuration is missing
- [ ] Connect to the Alpaca market-data WebSocket
- [ ] Authenticate without logging credentials
- [ ] Subscribe only to AAPL quotes and trades
- [ ] Put received frames into the bounded queue
- [ ] Record raw frames through RawRecorder
- [ ] Add connection, authentication, subscription, and disconnect logs
- [ ] Handle SIGINT with a graceful queue close and recorder flush
- [ ] Add exponential reconnect backoff with a maximum delay
- [ ] Mark gaps and reconnections explicitly in the raw log

### Manual verification

First use Alpaca's always-available test stream. Then run against the paper
market-data stream during market hours.

### Acceptance criteria

- Missing credentials produce a clear error and no crash
- Credentials never appear in console or data logs
- The process records AAPL frames and exits cleanly after Ctrl-C
- A disconnect cannot silently continue as if data were current
- This chunk contains no strategy or order-submission code

### Chunk prompt

~~~text
Implement Chunk 3 from TASKS.md. Build a recorder-only Alpaca WebSocket client
that loads secrets from environment variables, authenticates, subscribes to AAPL
quotes and trades, pushes frames through the existing queue, and writes raw
NDJSON. Add graceful shutdown and bounded reconnect backoff. Never submit an
order and never log credentials.
~~~

---

## Chunk 4 — Parser and deterministic replay source

### Goal

Turn recorded raw data into typed events and make a file behave like a live event
source.

### Build

- [ ] Define IEventSource with a single ordered event interface
- [ ] Wrap live input behind LiveEventSource
- [ ] Implement ReplayEventSource for raw.ndjson
- [ ] Parse Alpaca quote messages into Quote
- [ ] Parse Alpaca trade messages into Trade
- [ ] Parse connection and status messages
- [ ] Return structured parse errors for malformed or unsupported input
- [ ] Preserve original sequence numbers and recorded timestamps
- [ ] Add sanitized fixtures captured from the real recorder
- [ ] Add a replay speed mode: immediate or recorded timing

Recorded timing is for demonstrations only. Decision logic must use event time,
not wall-clock delays.

### Tests

- Valid quote and trade fixtures
- Missing fields
- Wrong field types
- Unknown message types
- Empty and truncated log lines
- Out-of-order or duplicate sequences
- Replaying the same fixture returns identical typed events

### Acceptance criteria

- Live and replay sources produce the same internal event types
- Parser failures are observable and cannot crash the process
- Replay performs no network calls

### Chunk prompt

~~~text
Implement Chunk 4 from TASKS.md. Add IEventSource, LiveEventSource,
ReplayEventSource, and strict parsing for the recorded Alpaca message types.
Preserve sequence numbers and timestamps and add sanitized real-frame fixtures.
Do not calculate features or submit orders.
~~~

---

## Chunk 5 — Engine, clock, market state, and features

### Goal

Process all state changes on one engine thread and calculate features using only
information already observed.

### Build

- [ ] Add an injectable Clock interface with live and replay implementations
- [ ] Add the single-threaded Engine event loop
- [ ] Make the engine the sole owner of mutable trading state
- [ ] Track latest bid, ask, sizes, last trade, and update timestamps
- [ ] Reject incomplete, crossed, locked, or invalid quotes as configured
- [ ] Mark market state stale after a configured timeout
- [ ] Calculate spread and midpoint
- [ ] Calculate quote imbalance
- [ ] Implement documented trade signing
- [ ] Calculate rolling signed trade-flow imbalance
- [ ] Write deterministic feature records to decisions.ndjson
- [ ] Record source sequence numbers for every computed feature

### Tests

Use hand-calculated examples for every feature. Verify that future events cannot
affect previous outputs and that stale state becomes non-tradable using a fake
clock.

### Acceptance criteria

- Only the engine thread mutates MarketState
- Decision code never calls the system clock directly
- A fixture replay produces the expected feature values
- Replaying twice produces byte-identical feature logs

### Chunk prompt

~~~text
Implement Chunk 5 from TASKS.md. Add the injected clocks, single-threaded engine,
MarketState, initial feature calculations, staleness handling, and deterministic
feature logging. Prove the math with hand-calculated tests. Do not add a trading
strategy or broker.
~~~

---

## Chunk 6 — Baseline strategy and risk gate

### Goal

Generate explainable trade proposals and ensure no proposal can become an order
without passing risk.

### Build

- [ ] Define TradeProposal separately from OrderRequest
- [ ] Define IStrategy and IRiskManager interfaces
- [ ] Implement one deterministic threshold-based baseline strategy
- [ ] Make strategy parameters configuration values
- [ ] Add maximum order quantity
- [ ] Add maximum absolute position
- [ ] Add maximum spread
- [ ] Add market-hours check
- [ ] Reject stale, disconnected, or invalid market state
- [ ] Add maximum daily loss
- [ ] Add maximum order rate
- [ ] Add a global kill switch defaulting to safe
- [ ] Log proposal inputs, approval, and every rejection reason

The baseline strategy exists to exercise the infrastructure. Do not claim it has
predictive edge.

### Tests

Create table-driven tests where each risk rule independently approves or rejects.
Also test multiple simultaneous failures and verify stable rejection ordering.

### Acceptance criteria

- Strategy code cannot call a broker
- Risk is the only path from TradeProposal to OrderRequest
- Every rejection is visible and reproducible
- Kill switch and stale data always prevent new orders

### Chunk prompt

~~~text
Implement Chunk 6 from TASKS.md. Add a simple deterministic baseline strategy,
TradeProposal, OrderRequest, and a composable risk gate with every listed rule.
Add table-driven tests and deterministic decision logging. The strategy must not
access any broker, and the kill switch must default to safe.
~~~

---

## Chunk 7 — Simulated broker and portfolio accounting

### Goal

Complete an end-to-end replay without contacting Alpaca's trading API.

### Build

- [ ] Define IBroker
- [ ] Define order states and legal state transitions
- [ ] Implement SimulatedBroker
- [ ] Forbid fills from events at or before order submission
- [ ] Support acknowledgements, rejections, partial fills, fills, and cancels
- [ ] Model configurable latency, slippage, and transaction costs
- [ ] Track cash, position, average cost, realized P&L, and unrealized P&L
- [ ] Log orders, transitions, fills, and portfolio snapshots
- [ ] Reject impossible or duplicate state transitions

### Tests

- Market and limit order behavior
- No look-ahead fills
- Partial fills
- Cancel before fill
- Fill/cancel race ordering
- Duplicate updates
- Long-to-flat and long-to-short accounting
- Short-to-flat and short-to-long accounting
- Realized plus unrealized P&L reconciliation

### Acceptance criteria

- A recorded session runs through the full engine offline
- No network access occurs during replay
- Portfolio accounting reconciles after every fill
- Two identical replays produce byte-identical decision logs

### Chunk prompt

~~~text
Implement Chunk 7 from TASKS.md. Add IBroker, the tested order state machine,
SimulatedBroker, no-look-ahead fill logic, and portfolio/P&L accounting. Run a
complete offline replay. Do not add Alpaca order submission yet.
~~~

---

## Chunk 8 — Python research report

### Goal

Turn one decisions log into an honest, readable evaluation of engine behavior.

### Build

- [ ] Add a Python requirements file or pyproject
- [ ] Read decisions.ndjson without reproducing C++ strategy logic
- [ ] Report total return and realized/unrealized P&L
- [ ] Report maximum drawdown and exposure
- [ ] Report turnover, number of trades, and win rate
- [ ] Report fill rate, slippage, and decision-to-fill latency
- [ ] Summarize risk rejection counts
- [ ] Plot P&L, position, drawdown, and trade markers
- [ ] Print data-quality warnings and session configuration

### Tests

Use a tiny known decisions fixture and verify exact summary metrics.

### Acceptance criteria

- One command produces a summary file and plots
- Python only analyzes C++ output
- Empty sessions and rejected-only sessions work
- Reports clearly label results as simulated

### Chunk prompt

~~~text
Implement Chunk 8 from TASKS.md. Build a small Python reporting tool for
decisions.ndjson with tested metrics and plots. Python must not contain a copy of
the strategy, risk logic, or fill model. Label all results as simulated.
~~~

---

## Chunk 9 — Alpaca paper broker

### Goal

Replace only the broker boundary so the tested engine can place paper orders.

### Build

- [ ] Implement AlpacaPaperBroker behind IBroker
- [ ] Use the paper endpoint only and fail closed for any other endpoint
- [ ] Submit and cancel paper orders
- [ ] Receive asynchronous trade updates
- [ ] Map Alpaca states into the internal order state machine
- [ ] Use client order IDs for idempotency
- [ ] Reconcile open orders, positions, and cash after reconnect
- [ ] Disable submission until market and account state are fresh
- [ ] Add strict maximum quantity and notional limits
- [ ] Add a second runtime arming flag in addition to the kill switch
- [ ] Start with shadow mode where orders are logged but never sent

### Verification sequence

1. Run shadow mode for a complete session.
2. Confirm proposed orders and risk decisions in the log.
3. Restart and verify account reconciliation.
4. Arm paper submission with quantity limited to one share.
5. Submit, observe, and cancel one controlled paper order.
6. Disable submission again.
7. Replay the recorded session and compare decisions.

### Acceptance criteria

- Live-money endpoints are rejected by code
- Restarting cannot duplicate an earlier order
- Unknown broker state disables further submission
- Paper fills flow through the same accounting path as simulated fills
- Shadow and paper modes are explicit in every log

### Chunk prompt

~~~text
Implement Chunk 9 from TASKS.md. Add AlpacaPaperBroker behind IBroker with
paper-endpoint enforcement, idempotent client order IDs, asynchronous order
updates, reconciliation, shadow mode, and two-step arming. Default to no order
submission. Never add or enable a live-money endpoint.
~~~

---

## Chunk 10 — Reliability and final proof

### Goal

Make failures visible and produce evidence that the architecture works as
claimed.

### Build

- [ ] Commit a small sanitized golden raw replay fixture
- [ ] Store the expected decisions hash
- [ ] Replay twice in CI and fail if hashes differ
- [ ] Fuzz the external JSON parser
- [ ] Test queue overflow and a deliberately slow engine
- [ ] Test disconnect, stale state, and reconnect recovery
- [ ] Test interrupted log writes and truncated final lines
- [ ] Expose queue depth, processing lag, parse failures, and reconnect count
- [ ] Add a session manifest containing configuration and code version
- [ ] Document setup, replay, paper mode, and emergency shutdown
- [ ] Record a short demo showing live capture followed by identical replay

### Acceptance criteria

- CI automatically detects nondeterminism
- Overload, stale data, and disconnects disable trading
- A clean checkout can reproduce the golden replay
- README instructions work without undocumented steps
- The demo supports every technical claim made on the resume

### Chunk prompt

~~~text
Implement Chunk 10 from TASKS.md. Add the golden replay determinism test,
failure-mode tests, operational metrics, session manifest, final documentation,
and demo instructions. Verify all existing tests under sanitizers. Do not expand
the product scope.
~~~

---

## V1 completion checklist

V1 is complete only when all of these are true:

- [ ] AAPL market frames can be recorded from Alpaca
- [ ] The raw log is append-only and preserves exact received payloads
- [ ] Live and replay feed the same engine interface
- [ ] Prices use deterministic fixed-point arithmetic
- [ ] One engine thread owns all trading state
- [ ] Features have hand-calculated tests
- [ ] Every proposal passes through risk
- [ ] Simulated fills cannot use future information
- [ ] Positions and P&L reconcile
- [ ] Paper trading requires explicit two-step arming
- [ ] Disconnects and stale data disable submission
- [ ] Golden replay output is identical across repeated runs
- [ ] Python produces an evaluation report
- [ ] No secret or live-money endpoint is committed

## After V1: market-making extension

Do not begin this until the V1 completion checklist passes.

1. Add a two-sided QuoteProposal containing bid and ask orders.
2. Add resting-order tracking and cancel/replace behavior.
3. Add inventory limits and inventory-based quote skew.
4. Add a realistic queue-position and latency model.
5. Measure spread capture, fill rate, adverse selection, and inventory P&L.
6. Test on replay and paper trading before considering any live experiment.

Full-depth order-book data and realistic exchange queue modeling may require a
different market-data provider. This extension remains a research project, not a
promise of profit.

## The first thing to do now

Start only with Chunk 0. Once its acceptance criteria pass, commit it and check
off its tasks before prompting for Chunk 1.
