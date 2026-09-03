# Vanta Project Instructions

## Required context

Before making changes, read:

- README.md
- ARCHITECTURE.md
- TASKS.md

## Scope

- Implement only the requested TASKS.md chunk.
- Do not begin future chunks.
- Avoid unrelated refactors or unnecessary abstractions.
- Ask before materially changing the architecture.
- V1 supports only AAPL equities and Alpaca paper trading.

## Architecture invariants

- Live and replay modes use the same engine logic.
- Only the engine thread may mutate trading state.
- Prices and money use fixed-point integers, never floating point.
- Strategy produces proposals but cannot submit orders.
- Every proposal must pass through the risk manager.
- External time must enter through the Clock interface.
- Replay must never make network requests.
- Simulated fills cannot use events received before an order.
- Deterministic inputs must produce deterministic outputs.

## Trading safety

- Never enable real-money trading.
- Paper order submission must default to disabled.
- Never bypass the risk manager.
- Never place credentials in code, logs, fixtures, or commits.
- Disconnects, stale data, and unknown broker state must disable trading.
- Every rejection, order transition, and fill must be logged.

## Implementation workflow

Before coding:

1. Explain the requested change.
2. List the files you expect to modify.
3. Describe the tests you will add.

After coding:

1. Format all changed code.
2. Build from a clean directory.
3. Run the complete test suite.
4. Run relevant sanitizer checks.
5. Summarize changed files and remaining limitations.

Do not mark a TASKS.md item complete unless its acceptance criteria pass.
