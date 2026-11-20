# D6 — Analyst Report

## Cost model

The benchmark replays 30 simulated days. It adds one patient every 5 minutes, offers treatment every 8 minutes, and offers a departure every 120 minutes when the queue is nonempty. Revision share is

```text
r = R / (I + X + D + R)
```

where `I` is inserts, `X` is treatments, `D` is departures, and `R` is clinical revisions. Step-wise escalations are counted separately as `E`.

For patient `i`, `a_i` is arrival time and `z_i` is the time they leave the waiting queue:

```text
E = sum over patients i of max(0, floor((z_i - a_i) / 30))
```

Let `L` be average live queue size, `P` peak live size, `M` Structure B's physical heap size, `S` stale roots removed during extracts, and `C_j` the heap size scanned by compaction `j`.

- **STRUCTURE A:** build is `O(n)`; insert, extract, revision, and delete are `O(log P)` plus expected `O(1)` locator lookup. Space is `O(P)`.
- **STRUCTURE B:** heap operations are `O(log M)`; stale entries are skipped, and compaction is linear in the heap size. With the stated compaction threshold, `M <= 2P + 1`; operation cost is amortized `O(log P)`. Space is `O(M + P)`.
- **ORACLE:** insert is amortized `O(1)`; extract, revision lookup, and delete scan the live array in `O(P)`. Space is `O(P)`.

The analyst's predicted work, before timing, is:

```text
W_A_linear = c_build*I
           + c_insert*I*log2(L+1)
           + c_extract*X*log2(L+1)
           + c_revision*q*R*log2(L+1)
           + c_delete*D*log2(L+1)

W_A_step = W_A_linear + c_escalation*E*log2(L+1)

W_B = c_build*I
    + c_insert*I*log2(M+1)
    + c_extract*(X+S)*log2(M+1)
    + c_revision*q*R*log2(M+1)
    + c_delete*D
    + c_compact*sum(C_j)

W_oracle = c_extract*X*L + c_revision*R*L + c_delete*D*L + c_insert*I
```

The `c` terms are implementation-dependent costs. `q` is the fraction of revisions that strictly improve priority. These formulas predict scaling, not exact elapsed time.

## Measurements

The C++ benchmark ran both aging policies at revision shares from 0% through 90%, with three seeds (12345–12347): 180 runs total. It used MinGW GCC 6.3.0 with C++17 and `-O2 -DNDEBUG`. Each run's treatment checksum matched across A, B, and the oracle. Times use Windows `QueryPerformanceCounter`; memory is process peak working set, including simulator data structures.

Median total queue-operation time (milliseconds):

| Aging | Revisions | Mean R | Mean E | A | B | Oracle |
|:---|---:|---:|---:|---:|---:|---:|
| Linear | 0% | 0 | 0 | 205.38 | 2.19 | 8.73 |
| Linear | 50% | 14,399 | 0 | 229.42 | 4.06 | 10.82 |
| Linear | 90% | 129,591 | 0 | 416.13 | 16.78 | 32.47 |
| Step-wise | 0% | 0 | 4,200 | 367.39 | 2.72 | 10.28 |
| Step-wise | 50% | 14,399 | 242,406 | 813.29 | 31.07 | 72.57 |
| Step-wise | 90% | 129,591 | 333,941 | 1,213.02 | 52.58 | 112.90 |

Median peak process memory (megabytes):

| Aging | Revisions | A | B | Oracle |
|:---|---:|---:|---:|---:|
| Linear | 0% | 6.78 | 8.79 | 6.68 |
| Linear | 50% | 6.78 | 8.98 | 6.67 |
| Linear | 90% | 6.77 | 9.11 | 6.67 |
| Step-wise | 0% | 6.78 | 8.96 | 6.68 |
| Step-wise | 50% | 6.80 | 8.93 | 6.67 |
| Step-wise | 90% | 6.78 | 8.94 | 6.67 |

Full measurements: [d6_measurements.csv](../../results/d6_measurements.csv). Medians by policy, share, and operation: [d6_summary.csv](../../results/d6_summary.csv).

## Findings and limits

- B was fastest but used about 2 MB more peak memory than A. A was slower than the array oracle, likely due to locator hashing and swap updates.
- Step-wise aging generated about 334,000 escalations at 90% revisions, versus 129,591 clinical revisions.
- Chen et al. also found no-decrease-key queues faster in most tested Dijkstra cases ([TR-07-54](https://www3.cs.stonybrook.edu/~rezaul/papers/TR-07-54.pdf)); the workloads differ, so this is directional context only.

The benchmark uses a synthetic trace, three seeds, and process-wide memory. Structure A's tombstone and ordering fixes should be reviewed by its owner.
