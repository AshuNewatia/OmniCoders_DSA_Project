# Differential queue tests: Oracle versus A and B

## Purpose

Give all three queues exactly the same operations and use the independently
validated oracle's actual responses as the expected results for A and B.
No teammate's queue implementation is changed by this harness.

The driver is compiled three times, once per implementation. This avoids A/B's
conflicting global `LookUp` and `twin` types without renaming or wrapping their
implementation code. Each case uses a fresh process and fresh queue. A/B are
not linked into the same executable.

## Run

From the repository root:

```powershell
python oracle/run_differential.py
```

The runner detects the locally downloaded portable GCC, g++/clang++ on PATH,
or common MSYS2 locations. To select a compiler explicitly:

```powershell
python oracle/run_differential.py --compiler "C:\msys64\ucrt64\bin\g++.exe"
```

With Make and a suitable Python command installed, `make test_differential`
calls the same runner. This is a separate target; existing `run_all` is unchanged.

Return codes: 0 means every A/B comparison passed; 1 means a mismatch, crash
or per-case timeout; 2 means setup, compilation or reference-execution error.
A failed test run must not be described as a passing suite.

## What gets compared

The plain-text driver protocol has one instruction per physical line:

```text
BUILD n id severity arrival key ...
INSERT id severity arrival key
DECREASE id new_key
DELETE id
EXTRACT
SNAPSHOT expected_live_count
```

Each instruction produces one output line. The runner compares:

- Successful operation responses.
- Every field of an extracted patient: ID, stored severity, arrival and key.
- Exception category (`out_of_range`, `invalid_argument`, or unexpected type),
  not exception message wording.
- Full extraction order and contents at snapshot checkpoints, including an
  extra extraction that must throw once all expected patients are removed.

`SNAPSHOT` drains a copy of the queue, leaving the real trace's queue untouched.
It checks observable logical contents, not B's raw physical heap. B may retain
stale entries. Copy/drain work is testing overhead, never a benchmark.

## Cases

The default run has **38 cases**, each compared against both A and B:

- **18 targeted traces:** empty errors; ordinary extraction; both tie-breaks;
  decreases with unchanged medical fields; equal-key updates; invalid increases;
  missing IDs; arbitrary-delete upward repair; deletion positions; replacement
  and empty builds; removed IDs; `long long` limits; duplicate insert/build;
  1,000 tie-heavy patients; repeated revisions; stale entries below current
  minima; 1,500 insert/delete churn cycles; resizing to 1,500 live patients;
  and a supplied-key sequence resembling step-aging updates. Several of these
  categories are combined into individual traces.
- **20 random traces:** seeds 1 through 20, 2,000 requested operations each,
  for 40,000 requested operations per implementation. Every random operation
  is followed by a full-state snapshot checkpoint.

Random generation maintains a Python record dictionary to choose valid IDs,
keys and snapshot counts. It uses tuple-based minimum selection to update that
generation state after a planned extraction. Verdicts are obtained by actually
running the C++ oracle, not by comparing A/B with a Python prediction alone.
Unexpected changes to oracle behavior would still require independent oracle
validation; agreement among implementations is not a proof of correctness.

IDs are positive and not reused within a case. Replacement builds use fresh
IDs in random traces. The invalid duplicate-build operation ends its case:
the shared contract specifies an exception but does not explicitly require a
particular post-failure state for A/B. The harness therefore does not impose
the oracle's stronger unchanged-state guarantee on B after invalid build.

## Output and reproduction

Results live in `results/differential/`:

```text
latest.json                         # run metadata, hashes, commands and verdicts
summary.log                         # compact PASS/FAIL listing
oracle_build.log / A_build.log / B_build.log
<case>/input.trace                  # exact common input
<case>/oracle.log / A.log / B.log    # one response per trace instruction
<case>/*.stderr.log                 # process diagnostics
<case>/A_first_difference.json       # if A disagrees
<case>/A_failing_prefix.trace        # input through A's first differing response
```

B failures get analogous B files. Prefix traces reproduce the first observed
mismatch; they are not claimed to be minimal counterexamples. A timeout is
bounded by `--timeout` (default 20 seconds per executable per case).

Generate or refresh the readable per-case report from the saved run:

```powershell
python oracle/write_differential_report.py
```

This validates the saved trace hashes and writes `results/differential/REPORT.md`.
It does not execute additional queue operations or manufacture test outcomes.

Re-run one named case:

```powershell
python oracle/run_differential.py --case duplicate_live_insert
```

Run one selected random seed:

```powershell
python oracle/run_differential.py --seed 7 --steps 2000 --case random_seed_7
```

Replay an exact saved failure prefix:

```powershell
python oracle/run_differential.py --replay results/differential/duplicate_live_insert/A_failing_prefix.trace
```

`latest.json` and `summary.log` describe the most recent invocation, including
subset/replay runs. Other case files from earlier invocations can remain;
consult the current report's case list and source hashes.

## Actual initial result

With GCC 14.2.0, C++17, `-O2 -Wall -Wextra -pedantic`, the first full run gave:

| Implementation | Passed | Failed |
| --- | ---: | ---: |
| A versus oracle | 36 | 2 |
| B versus oracle | 38 | 0 |
| Total comparisons | 74 | 2 |

A accepts duplicate IDs in `insert` and `build_heap` where the interface and
oracle require `std::invalid_argument`. The first mismatches are the second
trace instruction in both cases. Full records, expected error category and
actual responses are saved for the A owner. No A/B fixes were made here.

## What this does not establish

- These finite traces are not a proof that A or B is correct for every input.
- A snapshot checks logical output; it does not inspect the heap/locator or
  enforce B's stale/live compaction threshold. A logically correct B output can
  still violate its internal size bound. Internal invariant checkers remain
  a separate task.
- Each executable uses one queue instance per case. A's global locator capacity
  interacting across multiple simultaneously live A instances is not covered.
- Step-like keys test queue updates only; they do not exercise the simulator's
  actual aging formula, event scheduler, random generation or thirty-day clock.
- The runner uses the lower-case `src/structB/structureB.cpp`. The repository's
  case-only B filename collision still needs resolution by the team; source
  hashes in the report identify the actual file bytes used on this machine.
- No elapsed-time or peak-memory performance claims are made.

Run `python oracle/run_validation.py` separately after oracle changes. Keep
oracle validation passing before using it to judge the other implementations.
