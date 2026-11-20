# D6 — Analyst Report

## Cost model

For operation counts I (insert), X (extract), R (clinical revision), D (departure), and E (step-wise escalation), mean live queue size L, peak P and B's physical heap size M:

Let `q` be the fraction of revisions that strictly improve priority (q = 1 in this trace). For B, let `f = stale/M`, so `M = P/(1-f)` and the compaction rule keeps `f <= 1/2` (hence `M <= 2P + 1`). Let `S` count stale roots discarded and `C_j` be the heap size scanned by compaction j.

Pre-measurement work model:

- A, linear: `W_A = c_build*I + (c_insert*I + c_extract*X + c_revision*q*R + c_delete*D) log2(L+1)`.
- A, step-wise: `W_A_step = W_A_linear + c_escalation*E*log2(L+1)`.
- B: `W_B = c_build*I + (c_insert*I + c_extract*(X+S) + c_revision*q*R) log2(P/(1-f)+1) + c_delete*D + c_compact*sum(C_j)`.
- Oracle: `W_oracle = c_insert*I + c_extract*X*L + (c_revision*q*R + c_delete*D + c_escalation*E)*L/2`; successful ID lookups assume random positions.

The `c` values represent implementation-dependent costs. The formulas were fixed before the measurements. To show predicted milliseconds, this report calibrates one constant per implementation and aging policy using only its 0% revision run, then predicts the other shares. This is a baseline-calibrated model check, not an independent forecast. Operation-level mean and SD (microseconds per operation) are also in `results/d6_summary.csv`.

## Method

The C++ replay covers 30 simulated days, with one arrival every 5 minutes, treatment opportunity every 8 minutes and departure opportunity every 120 minutes. It tests both aging policies and revision shares 0-90% with seeds 12345-12349. The separate 10^6 scale check builds one million live patients, applies 144 revisions and extracts 16 patients at 90% revision share, with seeds 12350-12354. That check isolates large-queue build and operation costs; it is not a million-patient clinical census.

Machine: 13th Gen Intel Core i7-13650HX; 14 cores / 20 threads; 24 MB Intel Smart Cache; max turbo 4.90 GHz; registry reported ~2.8 GHz at capture (dynamic clock; not pinned). [Intel processor specifications](https://www.intel.com/content/www/us/en/products/sku/232101/intel-core-i713650hx-processor-24m-cache-up-to-4-90-ghz/specifications.html). Build: MinGW g++ C++17, `-O2 -DNDEBUG`; timing: Windows QueryPerformanceCounter. Peak process working set includes the queue, trace, simulator maps and runtime. Trace generation is excluded from timed queue operations.

## Primary benchmark: 90% revision share

Mean +/- sample SD across five runs. Memory is mean +/- sample SD of peak process working set. The model constant is calibrated at 0% revisions for each queue and policy.

| Aging | Queue | Measured time (ms) | Predicted time (ms) | Peak memory (MB) | Gap |
|:--|:--|--:|--:|--:|--:|
| Linear | Indexed heap A | 423.40 +/- 13.31 | 2133.33 | 6.78 +/- 0.01 | -80.2% |
| Linear | Lazy heap B | 17.56 +/- 0.55 | 23.65 | 9.02 +/- 0.10 | -25.8% |
| Linear | Array oracle | 33.01 +/- 0.43 | 111.93 | 6.67 +/- 0.01 | -70.5% |
| Stepwise | Indexed heap A | 1254.68 +/- 6.73 | 9819.11 | 6.78 +/- 0.00 | -87.2% |
| Stepwise | Lazy heap B | 53.82 +/- 2.15 | 76.65 | 9.00 +/- 0.17 | -29.8% |
| Stepwise | Array oracle | 112.50 +/- 2.00 | 330.23 | 6.67 +/- 0.01 | -65.9% |

## 10^6-patient scale check

Mean +/- sample SD across five runs at 90% revisions; each run builds one million patients, performs 144 revisions, then extracts 16 minima.

| Queue | Build (ms) | Revisions + extracts (ms) | Total (ms) | Peak memory (MB) |
|:--|--:|--:|--:|--:|
| Indexed heap A | 24588.8 +/- 468.3 | 9.154 +/- 0.161 | 24597.9 +/- 468.4 | 91.8 +/- 0.0 |
| Lazy heap B | 120.2 +/- 4.1 | 0.043 +/- 0.006 | 120.3 +/- 4.1 | 185.5 +/- 0.0 |
| Array oracle | 4.3 +/- 0.1 | 71.773 +/- 7.853 | 76.1 +/- 7.9 | 58.2 +/- 0.0 |

## Findings and limits

At 90% revisions, B was 24.1x faster than A under linear aging and 23.3x under step-wise aging, while using more process memory. This agrees directionally with Chen et al.: on fixed-average-degree G(n,m) graphs, their no-decrease-key Dijkstra implementations ran at least 1.4x faster than decrease-key implementations. The workloads, queue variants and hardware differ, so the ratios are not directly comparable ([TR-07-54](https://www3.cs.stonybrook.edu/~rezaul/papers/TR-07-54.pdf)).

The baseline-calibrated model overpredicts 90%-revision time (A linear: 2133 ms predicted vs 423 ms measured; -80.2%). It charges a full-height sift per update; actual sift depths were not instrumented. At one million live entries, A's build averaged 24589 ms, suggesting high constants in locator/hash work despite its expected O(n) bound. Scale test uses only 16 extracts; timing and process-wide memory are machine- and workload-specific. Results use five seeds.

Raw runs: [d6_measurements.csv](../../results/d6_measurements.csv) and [d6_scale_measurements.csv](../../results/d6_scale_measurements.csv). Summaries: [d6_summary.csv](../../results/d6_summary.csv) and [d6_scale_summary.csv](../../results/d6_scale_summary.csv).
