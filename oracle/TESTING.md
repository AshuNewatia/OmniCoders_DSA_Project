# Oracle validation tests

This suite checks the existing oracle without changing its implementation.
It uses only the C++ standard library and Python's standard library.

## Run from the repository root

```powershell
python oracle/run_validation.py
```

The runner also detects a portable compiler at
`tmp/toolchains/mingw64/bin/g++.exe`, if one has been downloaded locally.
That toolchain directory is ignored by Git; do not upload compiler binaries.

If your compiler is not on PATH:

```powershell
python oracle/run_validation.py --compiler "C:\msys64\ucrt64\bin\g++.exe"
```

The runner compiles only `OracleQueue.cpp` and `validation_tests.cpp`. It does
not build the simulator, A, B, dashboard or benchmarks. Existing tests remain
unchanged and can still be run separately.

Results are saved in `results/oracle_validation/`:

- `latest.json`: status, commands, commit, working-tree status and named results.
- `build.log`: compiler diagnostics, when compilation was attempted.
- `run.log`: PASS/FAIL lines and summary, when execution completed.
- `failure_random_seed_*.txt`: the operation history up to a random-test failure.
- `failure_exhaustive_4374_small_builds.txt`: failing small-state description.

Return codes: 0 = all tests passed; 1 = test failure/invalid output;
2 = compiler unavailable, compilation failure, timeout or execution/setup error.
`NOT_RUN` is not a pass. Logs from previous runs can remain; `latest.json`
identifies the current run and which stages actually executed.

## What is checked

The default suite has **47 named tests**:

- 25 targeted operation/edge-case tests, including duplicate insert/build rejection.
- One exhaustive-small test: all 729 combinations of three keys and three
  arrival times for three unique IDs, in all six input orders (4,374 builds).
- One batch of 2,000 patients with many key/arrival ties.
- 20 reproducible random seeds, 2,000 requested operations each (40,000 total).

Targeted cases cover empty and singleton queues; selection by key rather than
severity; both tie-breaks; replacement/empty build; copying input records;
decreases and unchanged keys; missing IDs; increasing-key rejection;
deleting the first, middle, final and only stored patient; removed IDs;
`long long` key limits; and large positive IDs.

Each random sequence includes inserts, replacement builds, decreases, deletions,
extractions, missing-ID errors, invalid key increases and observations. Successful
new records use unique positive IDs; the suite does not require ID reuse, which
the shared interface restricts. Keys and arrival times have small ranges to
exercise ties often. Live occupancy is bounded to keep full-state checks cheap.

## How expected answers are obtained

Hand-written cases have explicit expected IDs or full records. Generated cases
use an independent `std::map<int, Patient>` as the logical record store, then
sort a copy with `std::tie(priority_key, arrival_time, id)`. They do not call
the oracle's comparator. After every random operation, the suite drains a COPY
of the oracle and compares every field of every record with that sorted list.
The actual random sequence's queue is not drained by this observation.

This detects incorrect updates/removals even when the next minimum still looks
right. Rejected missing-ID and increasing-key operations are followed by the
same state checks. Exhaustive-small testing covers the stated bounded domain,
not every possible integer input or operation sequence.

## Reproduce one seed

```powershell
python oracle/run_validation.py --seed 7 --steps 2000
```

This still runs all targeted/exhaustive/batch tests but selects one random seed
instead of 20. The generated trace is reproducible for the same source, seed
and step count; it uses `mt19937_64` output directly with modulo selection.

## Compile manually

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic oracle/OracleQueue.cpp oracle/validation_tests.cpp -o oracle/validation_tests.exe
.\oracle\validation_tests.exe
```

The manual executable writes failure traces in the working directory. The
Python runner puts them in the results directory and preserves output for sharing.

## Duplicate-ID regression tests

`INTERFACE.md` requires duplicate live IDs to be rejected with
`std::invalid_argument`. The suite originally found that both operations accepted
duplicates. Insert now checks the current array before appending; build validates
the entire input using a temporary ID set before replacing the array. The tests
also check that rejected input leaves the prior queue unchanged. They remain
in the suite to detect regressions; duplicate priority keys remain valid.

## Scope and limitations

Passing tests is evidence, not a proof of correctness. Retain the scan invariant
and state-preservation argument. This suite tests the oracle only; it does not
test A/B or verify the simulator's aging scheduler/formulas. Those are separate
integration suites. It does not claim performance, peak-memory or hidden-test
coverage. Queue assignment/copy is used only for observation and is not timed.

Share `latest.json`, `build.log` and `run.log` after running. Include any failure
trace when a generated test fails. Do not commit executables or temporary builds.
