Vanta Hedge Architecture

1. Goal

Vanta Hedge helps a user evaluate options-based downside protection for an equity position. The first release accepts a manually entered stock position, retrieves real option quotes from Alpaca, compares protective puts under a user-defined downside scenario and budget, and optionally submits a confirmed order to Alpaca paper trading.

The project reuses Vanta's completed C++ market-data recorder. It does not require the recorder to serve the first product workflow, but the recorded data becomes useful later for replay, historical analysis, simulation and model validation.

Vanta Hedge is not an autonomous trading strategy. The user supplies the position, risk scenario and budget. Vanta calculates and compares possible hedges; order submission is always a separate, explicit action.

2. Architectural principles

Keep raw market data append-only.

Keep external Alpaca models behind adapters.

Keep financial calculations independent of networking and UI code.

Represent every options strategy as one or more reusable legs.

Store the exact inputs, quotes and assumptions behind an analysis.

Use ask prices for purchased options and bid prices for sold options when estimating immediately executable cost.

Separate analysis, order preview and order submission.

Use Alpaca paper trading only.

Build the prototype in Python; retain C++ for recording and later measured performance bottlenecks.

Validate advanced models against simple baselines before using their output.

3. System overview

Vanta contains two cooperating applications.

Hedge application

The Python application owns the user-facing product:

Position input
      |
      v
Alpaca REST market-data adapter
      |
      v
Normalized stock and option models
      |
      v
Candidate generator -> Payoff engine -> Ranking
      |                                  |
      +------------------+---------------+
                         v
                 Streamlit interface
                         |
                  explicit confirmation
                         |
                         v
             Alpaca paper-order adapter

Recording application

The existing C++ application runs independently:

Alpaca WebSocket
      |
      v
IWebSocket / IXWebSocket
      |
      v
AlpacaRecorderClient
      |
      v
RawRecorder -> append-only raw.ndjson

The recorder does not calculate hedge recommendations or submit orders. Later, an offline processing pipeline reads its raw files and produces normalized datasets for simulation.

4. Why the applications are separate

The first prototype only needs current snapshots from Alpaca's REST API. Requiring the C++ recorder, replay engine or a live WebSocket before showing a hedge comparison would unnecessarily delay the product.

Separation also protects completed work:

Recorder networking remains small and testable.

A broken interface cannot corrupt raw recordings.

Hedge calculations can be tested with fixed fixtures and no network.

Python models can evolve without changing transport code.

Recorded data can later be reprocessed when parsing or models improve.

These are separate processes, not microservices. They may share a repository and data directory without requiring deployment infrastructure.

5. Recommended repository layout

The existing C++ layout may remain in place during the prototype. New Python work lives under python/.

Vanta/
├── README.md
├── ARCHITECTURE.md
├── TASKS.md
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── include/vanta/
│   ├── recorder/
│   ├── events/
│   └── networking/
├── src/
│   ├── main.cpp
│   └── recorder_main.cpp
├── tests/
├── python/
│   ├── pyproject.toml
│   ├── app.py
│   ├── vanta_hedge/
│   │   ├── domain/
│   │   ├── calculations/
│   │   ├── services/
│   │   ├── adapters/
│   │   ├── persistence/
│   │   └── ui/
│   └── tests/
├── config/
├── data/
│   ├── raw/
│   └── processed/
└── tools/

Recorded sessions, databases, credentials and generated reports are not committed. Small sanitized fixtures may be committed for tests.

6. Python domain model

The domain layer contains no Alpaca SDK or Streamlit imports.

EquityPosition

symbol
quantity
average_entry_price (optional)

OptionQuote

bid
ask
bid_size
ask_size
timestamp
feed

OptionContract

symbol
underlying
option_type
strike
expiration
multiplier
quote
implied_volatility (optional)
greeks (optional)

OptionLeg

contract
side: buy or sell
quantity

HedgeCandidate

strategy_type
legs[]
premium
scenario_result
quote_warnings[]

Using legs[] from the beginning avoids a redesign when moving from one protective put to a two-leg put spread.

AnalysisRequest

position
horizon or target expiration
downside percentage
maximum premium budget

ScenarioResult

scenario_price
unhedged_pnl
option_payoff
hedged_pnl
loss_avoided
protection_per_dollar

Money and market prices should use Decimal in Python. Float may be used inside statistical models later, but not for order prices or premium accounting.

7. Base services

MarketDataService

Requests current underlying and option data through an adapter.

Normalizes responses into domain objects.

Handles pagination.

Rejects malformed contracts.

Attaches warnings for missing, stale, crossed or unusually wide quotes.

Supports fixture-backed data for deterministic tests.

CandidateGenerator

Produces strategies without ranking or placing them.

Initial implementations:

ProtectivePutGenerator

PutSpreadGenerator in a later milestone

PayoffEngine

Calculates stock P&L under a scenario.

Calculates the expiration payoff of every option leg.

Aggregates stock and option results.

Produces price points for payoff charts.

The payoff engine accepts domain objects only and performs no network calls.

HedgeAnalysisService

Coordinates one analysis:

Validate the request.

Retrieve market data.

Generate candidate hedges.

Calculate candidate results.

Filter by budget and quote validity.

Rank by explicit metrics.

Persist inputs and outputs.

PaperOrderService

Produces an order preview from a selected candidate.

Requires a separate confirmation request.

Enforces the Alpaca paper endpoint.

Uses unique client order IDs.

Persists submission and status updates.

Never accepts a candidate that was not previously analyzed.

8. V1 calculations

For current stock price S0, decline d, strike K, option ask A, contract multiplier m, stock quantity q and contract count n:

scenario_price = S0 * (1 - d)
stock_pnl = (scenario_price - S0) * q
premium_cost = A * m * n
put_payoff = max(K - scenario_price, 0) * m * n
hedged_pnl = stock_pnl + put_payoff - premium_cost
loss_avoided = hedged_pnl - stock_pnl
protection_per_dollar = loss_avoided / premium_cost

This is an expiration scenario. It is not an estimate of the put's exact value before expiration.

9. Quote-quality rules

A contract is excluded when:

Its ask is missing or non-positive.

Its bid is greater than its ask.

Its expiration has passed.

Its strike, option type or multiplier cannot be parsed.

Its quote is older than the configured hard limit.

A contract remains visible with a warning when:

Its bid is zero.

Its bid-ask spread is unusually wide.

Its size is low or unavailable.

It comes from an indicative feed.

The application records the quote timestamp and feed with each analysis so results remain explainable after prices change.

10. Persistence

The prototype uses SQLite through SQLAlchemy.

analyses

Stores position, spot price, horizon, downside scenario, budget and creation time.

candidates

Stores strategy type, serialized legs, quotes, premium, scenario result, warnings and rank.

paper_orders

Stores preview ID, client order ID, Alpaca order ID, request, response, status and timestamps.

The database is not a source of live market truth. It is an audit record of what the user saw and submitted.

11. Existing C++ recorder integration

The following completed components remain part of Vanta:

RawRecorder

IWebSocket

alpaca_config.hpp

alpaca_protocol.hpp

AlpacaRecorderClient

vanta_recorder

The recorder initially continues recording AAPL equity data. After the protective-put prototype works, its subscription configuration can be generalized to lists of stock and option symbols.

The recorder stores the exact received payload plus receive-time and sequence metadata. It should not fully parse option messages in the network callback. Offline parsing produces normalized files:

data/raw/session.ndjson
        |
        v
offline parser
        |
        v
data/processed/quotes.parquet

Raw files are never rewritten. Processed files may be regenerated from the raw source.

12. Later feature integration

Put debit spreads

Adds a second option leg and a PutSpreadGenerator. The existing PayoffEngine, analysis request, budget filter, persistence and UI remain. Paper orders use Alpaca multi-leg requests.

Alpaca portfolio import

Adds AlpacaPortfolioAdapter behind the position service. Imported positions are converted into the same EquityPosition and later Portfolio objects used by manual entry. The hedge engine does not consume raw Alpaca responses.

Historical bootstrap

Adds a SimulationEngine interface and HistoricalBootstrapEngine. The engine samples historical returns and evaluates existing hedge candidates across many paths. ScenarioResult remains for the simple view; a new RiskResult contains distribution metrics.

Monte Carlo simulation

Adds MonteCarloEngine behind the same simulation interface. The initial vectorized NumPy implementation is retained unless profiling justifies a C++ module through pybind11.

Correlation-aware portfolios

Extends Portfolio to multiple holdings, adds aligned historical returns and a covariance estimator, and changes simulated prices from one vector to a matrix of correlated paths. Candidate and payoff interfaces remain unchanged.

Hedge optimization

Adds an optimizer after candidate generation and simulation. It selects candidates subject to budget, strategy and quote-quality constraints. It does not generate market data, calculate payoffs or place orders.

Regime classification

Adds an offline feature pipeline and interpretable classifier that predicts volatility regimes, not stock direction. A model version supplies parameters to a simulation; it cannot submit an order. Chronological validation determines whether it remains in the project.

Hedge monitoring

Adds position snapshots and lifecycle events after paper execution. It refreshes prices and Greeks, reconciles orders after restart and reports how protection changes over time.

13. Future deployment architecture

Streamlit remains appropriate until the workflow is validated. If the product outgrows it:

React client -> FastAPI -> application services
                         |-> PostgreSQL
                         |-> Alpaca adapters
                         `-> simulation worker

Migration occurs around the existing domain and calculation packages. A queue, worker, PostgreSQL or WebSocket is added only when a demonstrated requirement exists.

14. Safety and failure behavior

Analysis fails closed when required quotes or position information are unavailable. Paper submission is disabled when:

Credentials or account state are missing.

The configured endpoint is not the paper endpoint.

Quotes are stale or invalid.

The preview no longer matches the selected candidate.

The order would exceed the stated budget or quantity.

Alpaca state cannot be reconciled.

The user has not explicitly confirmed the preview.

Logging may continue while submission remains disabled. Credentials and authentication payloads must never enter raw logs, application logs, fixtures or committed files.

15. Determinism and reproducibility

Calculation tests use fixed fixtures.

Every analysis stores source quotes and assumptions.

Simulations store model version, training range, parameters and seed.

Time-series validation uses chronological splits.

Raw event sequence numbers are preserved.

Derived datasets include a schema and parser version.

Re-running a deterministic scenario with identical inputs must produce identical output.

16. Current boundary

The C++ equity recorder is the completed infrastructure checkpoint. The next active work is the Python protective-put analyzer. Do not implement a generic strategy engine, simulated broker, autonomous agent or full portfolio optimizer before the MVP acceptance criteria in TASKS.md pass.

