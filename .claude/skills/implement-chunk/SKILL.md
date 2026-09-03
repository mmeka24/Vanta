---
name: implement-chunk
description: |
  Implement exactly one chunk of the Vanta V1 build plan in TASKS.md. Reads the chunk,
  states a plan, writes only that chunk's code and tests, builds, runs the full suite,
  and reports pass/fail against every acceptance criterion. Triggered by
  /implement-chunk <N>.
---

# implement-chunk

Build one chunk of Vanta. One chunk. Not the next one, not "a little of the next one."

The argument is the chunk number (`/implement-chunk 0`). If no number was given, look at
TASKS.md, find the lowest chunk whose checkboxes are not all ticked, say which one you
picked and why, and continue with that.

## Step 1 — Load context

Read, in this order, before touching any code:

1. `README.md`
2. `ARCHITECTURE.md`
3. `TASKS.md` — the whole file, then the `## Chunk <N> —` section specifically
4. `CLAUDE.md`

Then look at what already exists: the source tree, the CMake files, the existing tests.
You need to know what the previous chunk actually left behind, not what TASKS.md says it
should have left behind. If a prior chunk's acceptance criteria are visibly unmet and this
chunk depends on them, say so before starting.

## Step 2 — State the plan

Post a short plan before writing code. Four parts, no preamble:

1. **What this chunk does** — one paragraph, plain language.
2. **Files you expect to create or modify** — an actual list of paths.
3. **Interfaces you will introduce** — type and function signatures, one line each.
4. **How you will test it** — name the test cases, mapped to the "Tests" list in the chunk.

Then keep going. This is a plan you state, not a gate you wait at — unless something in the
chunk is genuinely ambiguous in a way that changes the design, in which case ask that one
question and stop.

## Step 3 — Implement

Scope rules, from `CLAUDE.md`, enforced hard:

- Only this chunk. If you notice work that belongs to a later chunk, note it in the report
  and do not write it.
- No unrelated refactors, no speculative abstractions, no "while I'm here" cleanups.
- Preserve existing behavior and existing tests. If an existing test must change, call that
  out explicitly with the reason.
- Ask before materially changing the architecture described in `ARCHITECTURE.md`.

Invariants that apply to every chunk, whether or not the chunk text repeats them:

- Prices and money are fixed-point `int64_t`. Never `float`, never `double`.
- Only the engine thread mutates trading state.
- Strategy proposes; only the risk manager can turn a proposal into an order.
- External time enters through the injected `Clock`. Decision code never calls the system
  clock.
- Replay makes no network calls.
- Simulated fills may only use events that arrived after order submission.
- Real-money trading stays disabled. Paper submission defaults to off.
- No credentials in source, logs, fixtures, config defaults, or commits.
- Malformed external input produces a structured error, never a crash and never a silent
  default.

Write the tests named in the chunk's "Tests" section. Hand-calculated expected values where
the chunk asks for them — a test that recomputes the implementation's own math proves
nothing.

## Step 4 — Verify

Run these and show the real commands and real output. Never describe a build you did not run.

1. **Format** — `clang-format` over changed files (`.clang-format` exists from Chunk 0).
2. **Clean configure and build** — a fresh build directory, debug preset, sanitizers on:
   `cmake --preset debug && cmake --build --preset debug`
3. **Full test suite** — `ctest --preset debug --output-on-failure`. The whole suite, not
   just the new tests.
4. **Determinism check**, once replay exists (Chunk 4+): run the same fixture twice and diff
   the output logs.

If a build or test fails, fix it and re-run. If something cannot be made to pass, that is a
reportable failure, not something to route around by weakening the test.

## Step 5 — Report

Finish with:

**Acceptance criteria** — a table with one row per criterion copied verbatim from the chunk,
each marked PASS or FAIL, each with the specific evidence: the test name, the command, the
log line. "Looks right" is not evidence. A criterion you did not verify is FAIL, not PASS.

**Changed files** — the list, one line of purpose each.

**Limitations** — what is stubbed, deferred, or known-weak, and which later chunk covers it.

**TASKS.md** — tick the chunk's `- [ ]` boxes only for items that are done and verified.
If any acceptance criterion is FAIL, tick nothing and say the chunk is incomplete.

**Commit** — suggest the message in the repo's format (`feat(component): ...`,
`test(component): ...`, `fix(component): ...`). Suggest it; do not commit unless asked.
