# Vanta Architecture

## 1. Goal

Vanta separates external I/O from deterministic trading logic. The same engine
processes events whether they came from a live WebSocket or a recorded file. The
same order path is also used whether approved orders go to a simulated broker or
Alpaca paper trading.

Only the system boundaries change between modes:

| Mode | Event source | Broker |
| --- | --- | --- |
| Replay | Recorded `raw.ndjson` | Simulated broker |
| Paper | Alpaca WebSockets | Alpaca paper broker |

Everything between those boundaries stays identical.

## 2. High-level data flow

```text
Market-data socket ----> Network thread ----+
                                             |
Trade-update socket ---> Network thread ----+--> Bounded event queue
                                                      |
                                                      v
                                                Engine thread
                                                      |
                   +----------------------------------+----------------------------------+
                   |                                  |                                  |
                   v                                  v                                  v
                Parser                         Raw recorder                       Market state
                                                                                       |
                                                                                       v
                                                                                Feature engine
                                                                                       |
                                                                                       v
                                                                                   Strategy
                                                                                       |
                                                                                Trade proposal
                                                                                       |
                                                                                       v
                                                                                 Risk manager
                                                                                       |
                                                                         approved / rejected
                                                                                       |
                                                                                       v
                                                                                     Broker
```

## 3. Components

### Event sources

`IEventSource` produces timestamped raw events. Implementations:

- `LiveEventSource`: reads Alpaca market-data and trade-update WebSockets.
- `ReplayEventSource`: reads the same events from `raw.ndjson` in recorded order.

Both produce a `RawFrame` containing a monotonic sequence number, source,
arrival timestamp, and exact payload.

### Network threads

Network threads perform blocking or asynchronous socket I/O without delaying
the trading engine. Their responsibilities are intentionally small:

1. Receive bytes.
2. Attach source, arrival time, and sequence metadata.
3. Append the raw event to durable storage.
4. Push the event into a bounded queue.

They do not calculate features or make trading decisions.

### Bounded event queue

The queue absorbs short bursts and transfers ownership from network threads to
the engine thread. It must have a fixed capacity so overload is visible instead
of consuming memory indefinitely. Queue depth, dropped events, and processing
lag should be measured. If data is lost or becomes stale, trading is disabled.

### Parser

The parser converts external JSON into typed internal events such as `Quote`,
`Trade`, `OrderUpdate`, and `ConnectionState`. Invalid messages are logged and
rejected without crashing the engine.

### Engine thread

One engine thread processes the ordered event stream and owns all mutable
trading state. This avoids locks inside decision logic and makes replay easier
to reproduce. The engine never reads wall-clock time directly; it receives time
through an injected `Clock`.

### Market state

`MarketState` stores the latest valid bid, ask, sizes, trade, connection state,
and timestamps. It marks itself stale after a configured timeout. Strategies
cannot trade while the state is incomplete, crossed, disconnected, or stale.

### Feature engine

The initial feature set includes:

- spread: `ask - bid`
- midpoint: `(bid + ask) / 2`
- quote imbalance: `(bid_size - ask_size) / (bid_size + ask_size)`
- rolling signed trade-flow imbalance

Feature calculations use only events already observed by the engine.

### Strategy

The strategy reads market state and features and may emit a `TradeProposal`.
The proposal describes intent—symbol, side, quantity, price, and reason—but is
not yet an order. The first strategy should be deliberately simple so the
infrastructure can be tested independently of strategy quality.

### Risk manager

Risk has final veto power. Initial checks include:

- maximum order size
- maximum absolute position
- maximum daily loss
- maximum spread
- valid market session
- fresh and valid market data
- duplicate or excessive order rate
- global kill switch

Every rejection records the failed rule and relevant values.

### Brokers

`IBroker` accepts approved orders and produces order updates.

- `SimulatedBroker` creates fills during replay using only market events that
  arrive after the order was submitted.
- `AlpacaPaperBroker` submits orders to Alpaca's paper account and converts
  asynchronous acknowledgements, fills, cancels, and rejections into typed
  events sent back through the queue.

### Logs

Vanta keeps two distinct append-only logs:

- `raw.ndjson`: exact external frames plus arrival metadata. This is the source
  of truth for replay and is never regenerated.
- `decisions.ndjson`: parsed events, features, proposals, risk outcomes, orders,
  fills, positions, and P&L. This can be regenerated from a raw recording.

### Python reports

Python reads `decisions.ndjson` after a run to calculate returns, drawdown,
turnover, rejection counts, latency, and execution-quality metrics. Python does
not reimplement strategy or execution decisions.

## 4. Determinism rules

The same raw input and configuration should produce byte-identical decision
output. To preserve that property:

- store prices and money as scaled integers;
- inject live or replay clocks instead of calling the system clock in logic;
- preserve explicit event sequence numbers;
- avoid unordered iteration when order can influence a decision;
- seed any random component explicitly and log the seed;
- version configuration and log schemas;
- replay a golden fixture twice in CI and compare output hashes.

## 5. Failure behavior

Safety is more important than continuing to trade. Vanta enters a non-trading
state when the queue overflows, a socket disconnects, market data becomes stale,
sequence continuity is lost, risk state is uncertain, or the kill switch is
active. Recording and diagnostics may continue while order submission remains
disabled.

## 6. Suggested source layout

```text
vanta/
├── CMakeLists.txt
├── README.md
├── ARCHITECTURE.md
├── TASKS.md
├── config/
├── include/vanta/
│   ├── events/
│   ├── market/
│   ├── strategy/
│   ├── risk/
│   ├── broker/
│   └── replay/
├── src/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── fixtures/
├── tools/reports/
└── data/
```

Recorded sessions and credentials must not be committed. Small sanitized replay
fixtures may be committed for tests.
