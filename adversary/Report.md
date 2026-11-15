# Project 10 — D5 Adversary

## 1. Role and objective

D5 is responsible for constructing inputs that expose expensive priority-queue
behaviour and for proving the required comparison lower bound.

The project specification asks for an input that forces
\(\Omega(n\log n)\) total comparisons over \(n\) `extract_min` operations,
a proof based on the comparison-sorting lower bound and the \(O(n)\)
build-heap bound, and a generator that approaches the bound.

This document separates the mathematical existence proof from empirical
claims about the team's implementations.

## 2. Mathematical lower-bound proof

Assume a comparison-based binary min-heap supports:

- `build_heap` in \(O(n)\) comparisons;
- repeated `extract_min`;
- extraction returns the minimum remaining key.

Suppose, for contradiction, that every input of \(n\) distinct keys could be
built into a heap and completely extracted using \(o(n\log n)\) comparisons
in total during extraction.

Then we could sort any array of \(n\) distinct keys as follows:

1. Run `build_heap` on the input. This costs \(O(n)\) comparisons.
2. Call `extract_min` exactly \(n\) times.
3. Output the extracted keys in order. They are sorted.

The total comparison count would be

\[
O(n) + o(n\log n) = o(n\log n).
\]

But comparison sorting has a worst-case lower bound of
\(\Omega(n\log n)\) comparisons. This is a contradiction.

Therefore, for every such comparison-based heap implementation, there exists
an input of \(n\) distinct keys for which the complete sequence of \(n\)
`extract_min` operations performs

\[
\boxed{\Omega(n\log n)}
\]

comparisons in total.

### What the proof does and does not say

- It proves that a hard input exists; it does not identify a universal
  permutation that is worst-case for every heap implementation.
- It is a worst-case statement over inputs, not a claim that every input
  costs \(\Omega(n\log n)\).
- It applies to the heap extraction algorithm under the stated comparison
  model and linear-build assumption.
- It does not establish the same lower bound for the unsorted-array Oracle.
  The Oracle has a direct \(\Theta(n^2)\) extraction cost instead.

## 3. Input generator

`Adversary.cpp` is a standalone C++17 program. It writes CSV rows with the
same three fields as the team's `Patient` model:

`id,severity,arrival_time`

It supports these deterministic candidate families:

| Pattern | Purpose |
|---|---|
| `ascending` | Increasing severity values |
| `descending` | Decreasing severity values |
| `alternating` | Alternates low and high severity ranks |
| `sawtooth` | Repeated local rises and falls |
| `bit-reversal` | Scrambles rank order using reversed binary index bits |
| `random` | Seeded random permutation of distinct severities |
| `equal-reverse-arrival` | Equal severity; comparator tie-breaking uses arrival time |

These are candidate stress patterns, not a proof that any one is worst-case
for Structure A or B. The best pattern must be selected by measuring the
actual implementation.

### Compile and run

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic Adversary.cpp -o adversary

./adversary 1000 descending 12345 > patients.csv
./adversary 1000 random 12345 > random.csv
```

The random pattern is reproducible when the same seed is used.

## 4. Experimental method

For each tested size, for example
\(n \in \{100, 200, 400, 800, 1600, 3200\}\):

1. Generate each candidate input using a fixed seed.
2. Pass the patients to `build_heap`.
3. Reset the comparison counter after build, so extraction comparisons
   are measured separately.
4. Call `extract_min` until the queue is empty.
5. Record total extraction comparisons and peak memory.
6. Repeat for several seeds for the `random` pattern.
7. Compare the measured values for Structure A and Structure B.

Record at least these columns:

```text
structure,pattern,seed,n,build_comparisons,extract_comparisons,
extract_comparisons_per_nlog2n,peak_memory_bytes
```

For the normalized column, calculate

\[
R(n)=\frac{C_{\mathrm{extract}}(n)}{n\log_2 n}.
\]

A nonzero, roughly stable ratio over increasing sizes is consistent with
\(\Theta(n\log n)\) extraction work. A falling ratio may indicate that the
tested family is not approaching the worst case. Measurements support the
proof but do not replace it.

**Important:** Count key comparisons in the shared patient comparator,
not loop iterations, hash-table probes, or ID lookups. Keep those metrics
separate if the project wants them too.

## 5. Oracle baseline

The supplied `OracleQueue::extract_min` scans the unsorted vector and compares
each remaining patient with the current best patient once per scan step.
With \(n\) patients and all \(n\) extracted, the number of calls to
`comes_before` is exactly

\[
(n-1)+(n-2)+\cdots+1
=\frac{n(n-1)}{2}
=\Theta(n^2).
\]

This count assumes the queue starts with \(n\) patients, no inserts or
deletions occur during the extraction-only test, and each invocation of
`comes_before` is counted as one comparator call. The comparator itself may
inspect multiple fields, so distinguish comparator calls from primitive
integer comparisons if reporting both.

The Oracle is a useful correctness and baseline-performance reference, but
it is not the binary-heap lower-bound proof.

## 6. Integration notes and current limitations

- The generator emits valid `Patient`-shaped CSV data but is not yet wired
  into `main.cpp`; the current simulator generates random arrivals internally.
- For a clean extraction experiment, load the generated CSV into a vector,
  call `build_heap`, then extract until empty. Do not mix arrivals,
  abandonment, or aging into this first experiment.
- The supplied interface does not expose a comparison counter. Add
  instrumentation in a shared comparator or test build without changing
  the queue semantics.
- The current Oracle uses a local comparator: severity, then earlier
  arrival time, then smaller patient ID. The heap implementations must use
  the same ordering for fair correctness comparisons.
- The current `main.cpp` has a separate linear-aging expression and
  simulation concerns. Keep D5's static-key extraction experiment separate
  until the Architect confirms the agreed aging semantics.

## 7. Results table template

Fill this table only after running the actual implementations.

| Structure | Pattern | n | Extraction comparisons | \(C/(n\log_2 n)\) | Peak memory |
|---|---|---:|---:|---:|---:|
| A | — | — | — | — | — |
| B | — | — | — | — | — |
| Oracle | — | — | — | — | — |

Do not invent or estimate measured values. State compiler, optimization flags,
machine, and seed in the final report.
