Vanta Hedge Build Plan

This file is the source of truth for the Vanta Hedge pivot. Complete one chunk at a time. The existing C++ recorder is preserved; new product work begins as a small Python prototype.

What the first release is

Vanta Hedge V1 accepts a manually entered stock position, retrieves current Alpaca option data, compares protective puts under a user-defined downside scenario and budget, displays a payoff comparison, and can submit a separately confirmed Alpaca paper order.

V1 is successful when a user can complete that workflow without depending on a trading strategy, replay engine, Monte Carlo model or live-money account.

What V1 is not

An autonomous trading bot

A stock-price prediction system

A complete portfolio optimizer

A production brokerage platform

A real-money trading application

A Monte Carlo or machine-learning project

A full options-pricing engine

A replacement for professional market data

Fixed choices for the prototype

Area

Choice

Existing ingestion

C++20 recorder

Product and calculations

Python 3.12

Initial UI

Streamlit

Market and order API

Alpaca

Orders

Alpaca paper only

Domain validation

Pydantic

Data calculations

Pandas, NumPy, Decimal

Charts

Plotly

Persistence

SQLite with SQLAlchemy

Python tests

Pytest

Initial strategy

Protective put only

Rules for every coding session

Give the coding assistant this instruction followed by exactly one chunk:

Read README.md, ARCHITECTURE.md and TASKS.md before changing anything.
Implement only the requested chunk. Preserve completed C++ recorder behavior and
tests. Do not add later quantitative features or unrelated abstractions. Never
place credentials in source, logs, fixtures or commits. Never enable live-money
trading.

Before coding, state:
1. files expected to change,
2. interfaces being added or changed,
3. tests that will prove the work.

After coding:
1. format changed code,
2. build affected C++ targets when applicable,
3. run the complete affected test suites,
4. report commands and results,
5. list changed files and remaining limitations.

Do not mark a checkbox complete unless its acceptance criteria pass. Stop and
ask if an assumption would materially change the architecture or scope.

Commit after each completed chunk.

Completed foundation — preserve this work

C++ project foundation

C++20, CMake presets and vcpkg configuration

Catch2 test setup

Strict formatting and compiler warnings

Debug sanitizer configuration

Git ignore rules for credentials, data and build output

Verify the existing GitHub Actions workflow after the first push

Core types and recording

Sequence and timestamp types

Fixed-point price type

Raw frame representation

Typed base event definitions

Bounded blocking queue

Append-only NDJSON RawRecorder

Credential-payload redaction protection

Queue and recorder tests

Current Alpaca recorder

IWebSocket abstraction around IXWebSocket

Environment-based Alpaca configuration

Sanitized missing-configuration errors

Alpaca authentication and subscription message builders

Minimal control, authentication, market-data and error classification

AlpacaRecorderClient

Exponential reconnect backoff with a bounded maximum

Runnable vanta_recorder

Graceful Ctrl-C shutdown

Existing suite passes with 55 tests

Verify the test-stream connection manually

Verify a market-hours AAPL recording manually

Confirm whether reconnect boundaries are represented inside the raw log

Add fsync or document the accepted last-write-loss durability window

Push and verify CI before changing recorder behavior

Recorder checkpoint acceptance

Existing tests continue to pass.

Credentials never appear in logs or fixtures.

The current recorder remains runnable during the product pivot.

New Python work does not require rewriting the recorder.

Chunk 1 — Python product foundation

Goal

Create an independently testable Python package beside the existing C++ application.

Build

Add python/pyproject.toml

Add python/app.py with a placeholder Streamlit page

Add python/vanta_hedge/ package directories

Add Ruff or Black formatting configuration

Add Pytest with one smoke test

Add .env.example containing names but no values

Extend .gitignore for Python caches, .env, SQLite and generated reports

Document Python setup commands

Acceptance criteria

pip install -e . succeeds from python/.

The placeholder application starts.

Pytest reports one passing test.

The existing C++ build and 55 tests still pass.

No new code calls Alpaca.

Chunk prompt

Implement Chunk 1 from TASKS.md. Add the minimal Python package, Streamlit entry
point, formatting configuration, environment example and one Pytest smoke test.
Do not add financial calculations, Alpaca integration or modify working C++
recorder behavior.

Chunk 2 — Domain models and protective-put calculations

Goal

Implement the financial core against static fixtures before connecting it to a network.

Build

Add EquityPosition

Add OptionQuote

Add OptionContract

Add OptionLeg

Add HedgeCandidate

Add AnalysisRequest and ScenarioResult

Use Decimal for prices, premiums and order-facing money

Calculate scenario price and unhedged stock P&L

Calculate long-put premium and expiration payoff

Calculate hedged P&L, loss avoided and protection per dollar

Generate chart-ready payoff points

Add a sanitized static option-chain fixture

Tests

Put expires worthless above its strike

Put is in the money below its strike

Premium is multiplied by 100

Multiple contracts and non-100-share positions

Zero or negative invalid inputs

Budget boundary

Scenario at, above and below the strike

Exact Decimal results

Acceptance criteria

All calculations run without network access.

Domain code imports neither Alpaca nor Streamlit.

Results match hand-calculated examples.

Output is explicitly labeled as an expiration scenario.

Chunk prompt

Implement Chunk 2 from TASKS.md. Add the domain models and protective-put
expiration-scenario calculations with a static fixture and hand-calculated
Pytest cases. Represent a candidate as option legs even though V1 uses one leg.
Do not connect to Alpaca or build the final interface.

Chunk 3 — Candidate generation, filtering and quote quality

Goal

Turn an option-chain fixture into safe, ranked protective-put candidates.

Build

Add ProtectivePutGenerator

Determine contract count from protected shares

Filter puts by underlying and expiration window

Filter by maximum premium budget

Exclude missing, non-positive and crossed quotes

Exclude expired contracts

Add configurable quote-age threshold

Warn for zero bids, wide spreads and indicative feeds

Rank by protection per dollar and expose alternate sort metrics

Preserve rejection reasons for excluded contracts

Acceptance criteria

A fixture produces the expected eligible candidates in stable order.

Every exclusion has a deterministic reason.

Purchased puts use the ask for estimated premium.

The generator performs no network call and submits no order.

Chunk prompt

Implement Chunk 3 from TASKS.md. Build protective-put candidate generation,
budget and expiration filtering, quote validation, warnings and stable ranking
against the existing fixture. Keep generation separate from payoff calculation.

Chunk 4 — Alpaca market-data adapter

Goal

Replace the static market-data fixture with current Alpaca snapshots while preserving the same domain interfaces.

Build

Add an IMarketDataAdapter protocol

Add a fixture-backed implementation

Add AlpacaMarketDataAdapter

Retrieve the current underlying snapshot

Retrieve and paginate the option chain

Filter contract type and expiration at the request boundary

Convert Alpaca responses into internal domain models

Record quote timestamp and source feed

Add timeouts, rate-limit handling and sanitized errors

Add fixture-based adapter tests

Add one opt-in sandbox integration test

Acceptance criteria

The application can switch between fixtures and Alpaca through configuration.

Alpaca SDK objects never escape the adapter.

API keys never appear in logs or exceptions.

Missing pages, missing quotes and API failures return structured errors.

Chunk prompt

Implement Chunk 4 from TASKS.md. Add fixture and Alpaca implementations of the
market-data adapter, normalize stock and option snapshots into existing domain
models, handle pagination and failures, and test with recorded responses. Do not
submit orders or change the C++ recorder.

Chunk 5 — End-to-end Streamlit analysis

Goal

Ship the first complete user workflow without order submission.

Build

Add inputs for ticker, shares, horizon, decline and budget

Validate inputs before requesting data

Call HedgeAnalysisService

Display spot price, quote time and analysis assumptions

Display eligible candidates in a comparison table

Display excluded-contract counts and warnings

Allow one candidate to be selected

Plot unhedged and hedged expiration P&L

Display premium, loss avoided and protection per dollar

Handle empty results and service failures clearly

Acceptance criteria

A user can complete the full analysis with fixture data.

The same workflow works with configured Alpaca data.

The UI contains no duplicated payoff calculations.

No button or code path can submit an order.

Chunk prompt

Implement Chunk 5 from TASKS.md. Build the complete Streamlit protective-put
analysis workflow using existing services. Keep calculations outside UI code.
Show assumptions, quote timestamps and warnings. Do not add order submission.

Chunk 6 — Persistence and reproducibility

Goal

Save enough information to reproduce what the user saw after market prices change.

Build

Add SQLAlchemy and SQLite configuration

Add analyses, candidates and paper_orders tables

Store the input position, scenario and budget

Store source quotes, timestamps and feed

Store candidate metrics and warnings

Add an analysis-history page

Add schema migration support

Keep credentials and account identifiers out of analysis records

Acceptance criteria

Reloading a saved analysis displays its original inputs and results.

A historical analysis does not silently refresh using current prices.

Database files are ignored by Git.

Persistence failures do not submit or duplicate an order.

Chunk 7 — Paper-order preview and submission

Goal

Allow a user to paper trade a selected protective put with a separate confirmation step.

Build

Add IPaperTradingAdapter

Add AlpacaPaperTradingAdapter

Reject any non-paper base URL in code

Create an order preview from a saved candidate

Revalidate quote age and budget before confirmation

Require a separate explicit confirmation

Submit a limit order, not an uncontrolled market order

Generate an idempotent client order ID

Persist request, response and status

Prevent duplicate confirmation

Support cancellation and status refresh

Start in preview-only mode

Acceptance criteria

Live endpoints are rejected.

Analysis cannot directly submit an order.

Repeated confirmation cannot duplicate an order.

Stale or changed quotes require a new preview.

One controlled one-contract paper order can be submitted and cancelled manually.

V1 completion checklist

Manual stock position can be entered

Current stock and put data can be retrieved

Invalid and stale quotes are handled

Protective puts are filtered by budget and expiration

Scenario calculations have hand-verified tests

At least three candidates can be compared when available

Hedged and unhedged payoff lines are displayed

Inputs, quotes and results can be saved

Order preview is separate from submission

Paper endpoint enforcement is tested

No live-money endpoint or secret is committed

Existing C++ recorder tests continue to pass

Setup and demo steps work from a clean checkout

Do not begin quantitative extensions until this checklist passes.

Extension 1 — Put debit spreads

Purpose

Compare a cheaper hedge that caps protection below the short strike with a standalone protective put.

Build

Add PutSpreadGenerator

Pair puts with the same underlying and expiration

Buy the higher strike and sell the lower strike

Calculate executable net debit from long ask minus short bid

Calculate maximum payoff, maximum loss and scenario P&L

Display both legs and capped protection clearly

Add Alpaca multi-leg paper-order preview and submission

Acceptance criteria

Payoff is correct below, between and above the strikes.

Maximum payoff equals strike width times multiplier.

Maximum loss equals net debit.

The spread respects the same budget and quote-quality pipeline as protective puts.

Extension 2 — Alpaca portfolio import

Purpose

Replace repetitive manual entry with analysis of actual paper-account positions.

Build

Add Portfolio and PositionSnapshot

Add AlpacaPortfolioAdapter

Normalize equity and option positions

Let the user select one imported position

Save the portfolio snapshot with the analysis

Keep manual entry available

Acceptance criteria

Imported values reconcile with the Alpaca paper account.

Unknown asset types fail safely.

Secrets and private account details do not enter logs.

Imported and manual positions use the same analysis service.

Extension 3 — Record option market data

Purpose

Build a replayable options dataset and reuse the existing C++ recorder.

Build

Generalize subscription configuration from one symbol to lists

Support underlying and OCC option symbols

Record option quote and trade frames without changing raw payloads

Mark reconnect and gap boundaries

Add sanitized option-message fixtures

Build an offline option parser

Write normalized processed data separately from raw data

Acceptance criteria

Existing AAPL equity recording remains compatible.

Raw files remain append-only.

Authentication frames and credentials cannot be recorded.

The same recording can be parsed repeatedly into identical normalized output.

Extension 4 — Historical bootstrap risk

Purpose

Replace one downside scenario with a distribution built from observed historical returns.

Build

Add SimulationEngine

Add HistoricalBootstrapEngine

Store training range, sample count and random seed

Calculate probability of loss, VaR and expected shortfall

Compare hedged and unhedged distributions

Add chronological walk-forward evaluation

Acceptance criteria

Fixed seeds produce reproducible tests.

No future observations enter a historical run.

Results are compared with the deterministic scenario baseline.

Prediction-interval coverage is reported.

Extension 5 — Monte Carlo simulation

Purpose

Test hedge performance across generated price paths and explicit volatility assumptions.

Build

Implement vectorized NumPy geometric Brownian motion baseline

Display drift, volatility, horizon and path-count assumptions

Evaluate every existing candidate on identical paths

Compare Monte Carlo with historical bootstrap results

Benchmark before considering C++ acceleration

Add pybind11 C++ computation only if profiling justifies it

Acceptance criteria

Simulated moments match configured parameters within tolerance.

Fixed seeds are reproducible.

Outputs are labeled as model estimates, not predictions.

C++ is not added without a measured bottleneck and benchmark.

Extension 6 — Correlation-aware portfolios

Purpose

Evaluate a multi-stock portfolio and determine whether an index or individual option provides effective protection.

Build

Align historical returns across holdings

Estimate and validate covariance

Generate correlated paths

Reprice all holdings and hedge legs per path

Compare SPY, QQQ and single-name hedge basis risk

Add correlation stress controls

Acceptance criteria

Simulated correlations match target correlations within tolerance.

Missing market dates are handled deterministically.

Invalid covariance matrices are repaired or rejected visibly.

Stress tests show the effect of increasing correlation.

Extension 7 — Constrained hedge optimization

Purpose

Find cost-effective hedges after Vanta can evaluate many candidates across a loss distribution.

Build

Add HedgeOptimizer after candidate generation

Minimize expected shortfall subject to budget and strategy constraints

Reject low-quality quotes

Compare against fixed-strike baseline hedges

Display several cost-versus-protection frontier choices

Preserve manual selection and explicit order confirmation

Acceptance criteria

No returned hedge violates its constraints.

Infeasible requests return a clear explanation.

Optimized results are compared out of sample with simple baselines.

Optimization cannot place an order.

Extension 8 — Volatility-regime classifier

Purpose

Test whether calm, ordinary and stressed environments require different simulation parameters.

Build

Create a versioned feature pipeline

Begin with a rule-based volatility baseline

Train an interpretable decision tree for the next volatility regime

Use chronological train, validation and test periods

Display the tree's decision path

Feed the predicted regime into simulation parameters

Compare calibration with and without conditioning

Acceptance criteria

The model predicts volatility regime, not exact stock direction.

No random time-series split or future leakage is used.

The tree beats documented naive baselines out of sample.

Remove the feature if it does not improve calibration.

Extension 9 — Hedge monitoring and lifecycle

Purpose

Track whether a purchased hedge continues to provide the protection originally modeled.

Build

Add hedge-position and lifecycle models

Refresh underlying and option values

Display current Greeks and remaining protection

Reconcile order and position state after restart

Track close, expiration, exercise and assignment events

Add quote-staleness and expiration warnings

Acceptance criteria

Duplicate or out-of-order events cannot corrupt state.

Local state reconciles with Alpaca after restart.

Submitted does not mean filled.

Unknown state disables further order actions.

Deferred migration — FastAPI and React

Do this only if Streamlit limits the validated product.

Wrap existing application services in FastAPI

Add a React/TypeScript client incrementally

Migrate SQLite to PostgreSQL

Add background workers for slow simulations only

Add WebSockets for live order status only

Preserve domain, calculation and adapter tests during migration

The next task

First push and verify the completed recorder checkpoint. Then implement only Chunk 1 — Python product foundation.

