---
name: teach-chunk
description: |
  Teach the Vanta code that was just written — the problem it solves, how data enters and
  leaves, the important types and functions, thread ownership, failure cases, what the tests
  actually prove, and the interview questions it invites. Triggered by /teach-chunk
  [<N> | <path>].
---

# teach-chunk

The user just had code built for them and now wants to actually understand it — well enough
to defend it in an interview, and well enough to notice when it is wrong.

## What to teach

- **No argument:** the code written in this conversation. If nothing was written here, use
  `git diff HEAD~1` / the most recent commit and say which one you picked.
- **A chunk number** (`/teach-chunk 2`): the code belonging to that TASKS.md chunk.
- **A path** (`/teach-chunk src/market/market_state.cpp`): that file or directory.

Read the actual code before writing a word of explanation. Read the tests too. Do not teach
from TASKS.md, from `ARCHITECTURE.md`, or from what you remember writing — those describe
intent, and the point of this skill is to explain what is really there. If the code diverges
from the documented design, that divergence is one of the most useful things you can point
out.

## Structure

Work through these seven sections in order, with these headings.

### 1. The problem this solves

Why does this module exist? What breaks or gets harder without it? Lead with the concrete
failure it prevents, not the abstraction it provides. One paragraph.

### 2. How data enters and leaves

Trace one real value end to end. Name the actual entry point, the actual types it becomes,
and the actual exit. For example: bytes off the socket → `RawFrame` → parser → `Quote` →
`MarketState`. Say what owns the data at each hop and where it gets copied versus moved.
If there is a queue in the path, say who pushes and who pops.

### 3. The important types and functions

Not every symbol — the four or five that carry the design. For each: its one job, its key
invariant, and the signature. Point at the file and line. Explicitly name what is *not*
important so the reader knows where not to spend attention.

### 4. Thread ownership

Which thread touches which state. Vanta's whole determinism story rests on this, so be
precise:

- What runs on a network thread, what runs on the engine thread.
- Which data crosses the boundary, and through what (the bounded queue).
- What is protected by a mutex or atomic, and what is protected by the stronger claim that
  only one thread ever touches it.
- Where a race *would* appear if someone added a well-meaning accessor later.

If this module is single-threaded, say so plainly and say what would break if it were called
from two threads.

### 5. Failure cases

What actually goes wrong in production, and what the code does about it:

- Malformed or truncated external input.
- Disconnects, stale data, sequence gaps.
- Queue full; engine falling behind.
- Numeric overflow, negative or zero prices, crossed or locked quotes.
- Partial writes and truncated final log lines.

For each: does it return an error, disable trading, or silently continue? Silent continuation
is a finding — say so. Also name the failure cases this code does *not* handle yet and which
chunk is supposed to.

### 6. What the tests actually prove

Go test by test through the real test file. For each meaningful test: the property it pins
down, and — honestly — the property it does not. Call out tests that would still pass if the
implementation were subtly wrong, tests that assert against the implementation's own output
rather than a hand-calculated value, and behaviors in the chunk's acceptance criteria that
have no test at all. This section is more valuable when it is critical than when it is
reassuring.

### 7. Interview questions this invites

Five to eight questions an interviewer would actually ask after reading this code, weighted
toward the ones that are hard. For each, a short answer grounded in *this* code — file, type,
line — not a generic textbook answer. Favor the questions that probe the design decisions:
why fixed-point instead of double, why a bounded queue instead of unbounded, why one engine
thread instead of a lock, how you know replay is deterministic, how a look-ahead fill would
be caught.

## How to write it

Senior engineer explaining their own code to a sharp junior, at a whiteboard. Concrete over
abstract. Real identifiers, real file paths, real numbers. Short sentences.

Skip the flattery and the summary-of-the-summary. If part of the code is ugly, unfinished, or
only correct by accident, say that — the user is going to be asked about this code by someone
who did not write it.
