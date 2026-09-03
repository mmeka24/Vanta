# Vanta

Vanta is an event-driven trading research engine built to study how market-data,
strategy, risk, and execution systems work together. It will consume live Alpaca
quote and trade events, record them for deterministic replay, calculate
market-microstructure features, produce trade proposals, apply pre-trade risk
checks, and route approved orders to a simulated or paper-trading broker.

The initial version trades only `AAPL` using paper money. The purpose is to build
and validate reliable trading infrastructure—not to promise profitable returns.

## Core principles

- C++ runs the complete live and replay pipeline.
- Live and replay use the same strategy, feature, and risk code.
- Prices use fixed-point integers rather than floating-point values.
- One engine thread owns mutable trading state.
- Raw market data is append-only and can be replayed later.
- Every proposal, rejection, order, and fill is logged.
- Python reads engine output only for analysis and reporting.
- Real-money execution is out of scope until paper trading is proven safe.

## Planned pipeline

```text
Alpaca WebSockets -> Network threads -> Event queue -> Trading engine
                                                        |
                    Parser -> Market state -> Features -> Strategy
                                                        |
                                   Decision log <- Risk checks
                                                        |
                                      Simulated or paper broker
```

## Planned technology

- C++20 and CMake
- Alpaca market-data and paper-trading APIs
- WebSockets
- NDJSON logs
- Catch2 for C++ tests
- Python for reports and charts

## Repository documents

- [`ARCHITECTURE.md`](ARCHITECTURE.md) explains how the system is designed.
- [`TASKS.md`](TASKS.md) is the build checklist and source of truth for progress.

## Current status

Planning and project setup. Start with the first unchecked item in `TASKS.md`.

## Safety

Vanta is an educational research project. Paper-trading results do not predict
real fills or future profitability. Live trading should remain disabled until
the engine has deterministic replay, tested risk limits, stale-data protection,
reconnection handling, and a manual kill switch.
