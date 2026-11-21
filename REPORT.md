# Project 10: Emergency Triage Queue

**Team:** OmniCoders  
**Course:** CS 253 / MA 253 — Data Structures and Algorithms Laboratory  
**Semester:** 3
**Repository:** https://github.com/AshuNewatia/OmniCoders_DSA_Project  


## 1. Team and contributions

| Member | Role | Contribution | Evidence |
|---|---|---|---|
| Ashu Newatia | Architect | Common interface and integration, simulation, priority-key and aging design, D3 derivation | [Architect integration PR #4](https://github.com/AshuNewatia/OmniCoders_DSA_Project/pull/4); [dashboard PR #8](https://github.com/AshuNewatia/OmniCoders_DSA_Project/pull/8) |
| Adityamoni | Structure A owner | Indexed binary min-heap and locator | Structure A implementation and locator commits; [TEAM: insert exact commit URLs] |
| Anuj Kumar | Structure B owner | Heap with lazy invalidation and version lookup | LazyTriageQueue implementation commit; [TEAM: insert exact commit URL] |
| Rohit Jhajhra | Oracle owner | Reference implementation and correctness tests | [Oracle PR #5](https://github.com/AshuNewatia/OmniCoders_DSA_Project/pull/5); [validation PR #11](https://github.com/AshuNewatia/OmniCoders_DSA_Project/pull/11); [derivation PR #13](https://github.com/AshuNewatia/OmniCoders_DSA_Project/pull/13) |
| Kambhampaty Krupalini Paul | Adversary | Adversarial inputs and D5 experiments | [Adversary PR #6](https://github.com/AshuNewatia/OmniCoders_DSA_Project/pull/6); [PR #10](https://github.com/AshuNewatia/OmniCoders_DSA_Project/pull/10) |
| Ritik Sarawag | Analyst | Benchmarking, cost model and results analysis | [Analyst PR #7](https://github.com/AshuNewatia/OmniCoders_DSA_Project/pull/7) |

The linked pull requests are taken from the supplied GitHub commit-history screenshots; their exact contents and final test status require verification against the release. [TEAM: confirm names, specific contributions, roll numbers and derivation assignments.]

## 2. Problem and scope

A casualty ward receives patients with different severity levels. While waiting, a patient's medical severity can increase, the patient's priority can rise because of waiting time, or the patient can leave before treatment. When the doctor becomes available, the system selects a live patient with highest priority. The application therefore requires insertion, extraction, priority updates and arbitrary deletion.

We compare an indexed binary min-heap (Structure A) with a binary heap using lazy invalidation (Structure B). Both expose the same operations to a 30-day simulated application. An unsorted-array Oracle provides a simpler reference for correctness checks. Synthetic adversarial inputs and benchmarks explore operation costs. This is an algorithmic simulation and does not make claims about real clinical decisions.

In the reviewed `app/main.cpp`, the simulation clock advances in one-minute increments for 43,200 minutes. Initial severity is sampled from 1–50, doctor treatment duration from 5–10 minutes, and arrivals, revisions and departures are generated using pseudorandom draws. The step-wise interval is 30 minutes and the step amount is 5. These are model parameters, not measured hospital data. 

## 3. Interface and invariants

### 3.1 Patient and priority

The shared `Patient` type in `app/ITriageQueue.h` contains an integer ID, severity, arrival time and a `long long` priority key. A greater severity indicates greater clinical urgency. Queue ordering uses the priority key: smaller keys are served earlier. The documented tie-breakers are earlier arrival time, then smaller ID.

`ITriageQueue` declares five operations:

| Operation | Intended behavior |
|---|---|
| `build_heap(patients)` | Replace queue contents using the supplied patients. |
| `insert(patient)` | Add a patient with a unique live ID. |
| `extract_min()` | Remove and return the highest-priority live patient. |
| `decrease_key(id, new_key)` | Update a live patient's key without increasing it. |
| `delete_patient(id)` | Remove a live patient by ID. |

The simulator calculates aging and calls the queue; the queue does not derive priority from time. `decrease_key` changes the stored key rather than the stored severity. The simulator's `waiting_room` holds current severity and is consulted using the returned patient ID.

### 3.2 Common and structural invariants

The intended contract is that each live ID is unique; each live patient has one current logical key; extraction returns the minimum under the shared ordering; and deleted or extracted patients are not returned later. Duplicate live IDs and invalid operations are handled according to the contract in `INTERFACE.md`.

For A, every parent precedes or equals its children under the ordering, and `locator[heap[i].id] = i` for every valid heap index. Each live patient must occur exactly once in both structures; every heap swap updates the two corresponding locator entries.

For B, the heap can contain current and stale entries. Its lookup table identifies the current version of a live patient; stale entries must not be returned. The reviewed implementation invokes compaction when its stale count exceeds its live count, retaining current entries and rebuilding the heap.


### 3.3 Event order

For events in the same simulated minute, the reviewed simulator processes arrival, step-wise escalation, severity revision, departure and treatment in that order. Thus, an escalation at the exact time of treatment or departure is processed first. This convention affects both the event trace and the number of escalation calls.

## 4. Structure A: indexed binary min-heap

Structure A stores patient records in an array-backed binary min-heap. A separate locator maps IDs to heap indices. The locator in the reviewed snapshot is implemented from scratch using open addressing, linear probing and deletion handling; it hashes the decimal representation of an ID using a polynomial-style hash. Insertions and deletions can trigger rebuilding or resizing of the locator.

To insert, A appends the patient, updates the locator and sifts the patient upward. To extract the minimum, it removes the root, moves the last record to the root and sifts downward. For a key decrease, the locator identifies the record and the heap repairs upward. Arbitrary deletion finds the record, replaces it with the last heap record and repairs in the appropriate direction. Heap movements must update the locator after each swap. Bottom-up heap construction is linear in the number of records for the heap-building part.

For `n` live patients, the expected model is `O(log n)` for insertion, extraction, key decrease and arbitrary deletion, assuming expected `O(1)` hash lookup; bottom-up heap building costs `O(n)` plus locator construction. Hash collisions, tombstones and rehashing affect actual cost and worst-case behavior. A uses `O(n)` heap and locator space under normal resizing assumptions.

The design trades locator maintenance for direct access to an existing heap entry. A plain heap without a locator would require a search to update an arbitrary patient.

**Correctness issue to verify:** A later presentation reported duplicate-ID acceptance in A's insertion and build. That presentation is not a substitute for the final code and test results. 

## 5. Structure B: lazy-invalidation heap

Structure B stores heap nodes containing a patient and a version. A lookup table identifies the current version and current patient data. Inserting creates a live entry. A priority decrease creates a new version, inserts it into the heap and makes the previous version stale. Extraction removes heap roots until a current entry is found; stale roots are discarded. Deletion invalidates the logical patient, although old physical entries may remain until discarded or compacted.

Let `H` be the number of physical heap entries, including stale entries, and `n` the number of live patients. Heap insertion costs `O(log H)`. A single extraction can discard several stale roots and need more than one heap removal. Compaction scans physical entries, retains current records and rebuilds the heap; its cost must be included in total or amortized measurements. The lookup table's hashing costs and the compaction implementation also affect runtime. Space depends on both physical heap entries and live lookup records.

B trades direct heap-index maintenance for stale entries and later cleanup. Its essential correctness condition is that a stale or deleted entry is never returned. ID reuse requires particular care: old versions must not become valid again accidentally. [TEAM: state the actual ID-reuse contract and verify it with tests.]

## 6. Aging derivation and requirement beyond the lecture

Let `s` be current severity, `a` arrival time, `t` current time, and `C` a common positive linear aging rate. Effective urgency is `s + C(t − a)`. Negating it gives the min-priority key `−s − C(t − a) = −s − Ct + Ca`. The `−Ct` term is common to all patients at a fixed comparison time and cancels. Thus the queue can compare the static key `−s + Ca`; linear aging alone requires no periodic key update while patients share the same rate. A clinical severity increase still requires a key update. Different rates invalidate the common-term argument.

With step-wise aging and 30-minute interval, the key is `−s − d × floor((t − a)/30)`. Since patients reach boundaries at different times, their order can change; the simulator must issue an update at the relevant boundary. If escalation at departure time is processed before departure, a patient waiting `W` minutes receives `floor(W/30)` escalation calls. D3 contains the derivation and counterexample.


## 7. Correctness and verification

### 7.1 Oracle

The Oracle keeps an unsorted patient collection and scans it when selecting the minimum according to the shared ordering. Its simpler representation makes it a useful reference, but its own behavior must be tested independently.

In the reviewed snapshot, `oracle/oracle_tests.cpp` includes tests for insertion, decrease and deletion; tie-breaking on arrival and ID; rebuilding; equal-key update; empty extraction; missing IDs; and rejection of an increased key. It expects `All oracle tests passed.` on successful completion. The reviewed 20 November test file did not establish complete coverage of every contract case, including duplicate-ID tests. A 21 November commit-history screenshot subsequently shows “Fix duplicate patient ID validation in oracle queue” and an additional validation-test commit; inspect and run the newer code to establish current coverage. 



## 8. Adversarial inputs

The D5 components in the reviewed snapshot include `adversary/Adversary.cpp`, `adversary/D5_HeapBenchmark.cpp` and `adversary/run_experiments.py`. The project explores input arrangements including ascending, descending and random patterns. Such patterns can cause different numbers of heap comparisons or movements depending on whether a benchmark builds, inserts or extracts; the worst case must be stated for the exact operation, key convention and generator.


## 9. Predicted cost model

The following is a working model for discussion, not evidence that it was committed before measurement. Let `I`, `X`, `D`, `R` and `E` be insertion, extraction, deletion, clinical revision and step-escalation counts; let `L` be a characteristic live queue size, `H` a characteristic physical heap size, `S` stale roots removed, and `C_j` the physical size scanned by compaction `j`. Implementation-dependent constants are omitted.

```text
A ≈ O((I + X + D + R + E) log(L + 1)) + hash-table costs
B ≈ O((I + X + R + E + S) log(H + 1) + D + Σ C_j)
Oracle ≈ O(I + (X + R + E + D) × L)
```

These expressions describe an operation-level model, not the whole simulator. In particular, the reviewed step-wise simulator scans the waiting room each minute, so application time includes scanning and logging outside queue operations. Actual costs depend on queue sizes over time, resizing, ties and the operations measured. Building an initial heap can add a separate linear term.


## 10. Experimental setup

The reviewed `docs/derivations/D6_Analyst.md` describes a separate synthetic benchmark with one arrival every 5 minutes, a treatment opportunity every 8 minutes and a departure opportunity every 120 minutes. It reports revision-share experiments under both aging policies, using seeds 12345–12347, MinGW GCC 6.3.0 and `-O2 -DNDEBUG`. It also describes 180 runs and a treatment-checksum comparison. This description is from that file; it has not been independently rerun or validated for this report.

The guidelines require at least five runs for each plotted point and a measure of spread. The analyst draft reports three seeds; verify whether more repetitions exist. If not, collect and retain the missing runs before claiming the figures satisfy this requirement.

## 11. Results and Discussion

We compared Structures A and B with the unsorted-array Oracle under
linear and step-wise aging. The benchmark used a synthetic workload
representing 30 simulated days and varied the proportion of clinical
severity revisions.

The measurements discussed below are from the analyst's recorded
benchmark results in:

- `results/d6_measurements.csv`
- `results/d6_summary.csv`
- `docs/derivations/D6_Analyst.md`

The analyst report describes three seeds: 12345, 12346, and 12347.
The timing values below are reported as medians. These measurements
should be verified against the final code and benchmark configuration
before submission.

### 11.1 Queue-Operation Time

The table below shows the reported median total queue-operation time
for selected revision shares.

| Aging policy | Revision share | Structure A (ms) | Structure B (ms) | Oracle (ms) |
|---|---:|---:|---:|---:|
| Linear | 0% | 205.38 | 2.19 | 8.73 |
| Linear | 50% | 229.42 | 4.06 | 10.82 |
| Linear | 90% | 416.13 | 16.78 | 32.47 |
| Step-wise | 0% | 367.39 | 2.72 | 10.28 |
| Step-wise | 50% | 813.29 | 31.07 | 72.57 |
| Step-wise | 90% | 1,213.02 | 52.58 | 112.90 |

In these reported runs, Structure B had the lowest median
queue-operation time among the three implementations at the selected
revision shares. Structure A was slower than the Oracle in this
particular benchmark, even though an indexed heap has a better
asymptotic extraction cost than an unsorted-array scan.

This does not mean that Structure B is always faster or that an
unsorted array is always faster than an indexed heap. Actual running
time depends on the workload, queue size, implementation details,
hash-table operations, memory access, and constant factors. In
particular, Structure A maintains locator entries during heap swaps,
while the Oracle uses a simple array scan.

The measurements are for the benchmark's synthetic workload and should
not be treated as results for all possible workloads.

### 11.2 Effect of Revision Share

For both aging policies, the reported total queue-operation time
increased as the revision share increased from 0% to 90%.

For example, under linear aging, Structure B's reported median
queue-operation time increased from 2.19 ms at 0% revisions to
16.78 ms at 90% revisions. Under step-wise aging, the corresponding
values increased from 2.72 ms to 52.58 ms.

A higher revision share causes more priority-update operations. The
effect differs between implementations because Structure A updates an
existing heap entry, whereas Structure B inserts a new entry and
invalidates the previous version.

The revision-share experiments change the number of operations as well
as their mix. Therefore, total time alone should not be interpreted as
the cost of one individual revision. A per-operation measurement and
the recorded operation counts are needed for that comparison.

### 11.3 Linear Aging and Step-Wise Escalation

The reported number of clinical revisions and step-wise escalations
for selected workloads is shown below.

| Aging policy | Revision share | Mean clinical revisions | Mean step-wise escalations |
|---|---:|---:|---:|
| Linear | 0% | 0 | 0 |
| Linear | 50% | 14,399 | 0 |
| Linear | 90% | 129,591 | 0 |
| Step-wise | 0% | 0 | 4,200 |
| Step-wise | 50% | 14,399 | 242,406 |
| Step-wise | 90% | 129,591 | 333,941 |

Linear aging requires no periodic key updates for aging alone because
the common time-dependent term cancels when priorities are compared.
Clinical severity revisions can still require `decrease_key`.

Step-wise aging generates an escalation when a waiting patient reaches
an interval boundary. In the 90% revision workload, the analyst report
records a mean of 333,941 step-wise escalations in addition to 129,591
clinical revisions. These extra updates help explain why total
queue-operation time was higher in the reported step-wise runs.

The dashboard simulation and the analyst benchmark use different
workload-generation procedures. Their numbers should therefore be
interpreted in the context of the specific program that produced them.

### 11.4 Memory Usage

The analyst report also records median peak process memory.

| Aging policy | Revision share | Structure A (MB) | Structure B (MB) | Oracle (MB) |
|---|---:|---:|---:|---:|
| Linear | 0% | 6.78 | 8.79 | 6.68 |
| Linear | 50% | 6.78 | 8.98 | 6.67 |
| Linear | 90% | 6.77 | 9.11 | 6.67 |
| Step-wise | 0% | 6.78 | 8.96 | 6.68 |
| Step-wise | 50% | 6.80 | 8.93 | 6.67 |
| Step-wise | 90% | 6.78 | 8.94 | 6.67 |

Structure B's reported peak process memory was higher than Structure
A's in these selected runs. Lazy invalidation can retain stale heap
entries until they are discarded or removed during compaction.

However, these values represent **peak memory for the whole process**,
including simulator data structures. They do not isolate the memory
used by the queue alone. The results therefore do not directly measure
the exact per-structure memory overhead.

### 11.5 Correctness Comparison

The analyst report states that the treatment checksum matched across
Structure A, Structure B, and the Oracle for its recorded benchmark
runs. This is useful evidence that the runs produced consistent
treatment checksums for that workload.

A checksum match does not prove that all returned patient fields,
exceptions, edge cases, or intermediate states were identical.
Correctness should also be assessed using the Oracle's validation
tests, differential tests, and structural invariant checks described
in Section 7.

### 11.6 Predicted and Measured Costs

The predicted model associates heap operations with logarithmic heap
work, while the Oracle scans its patient array for operations such as
extracting the minimum. Structure A additionally maintains its
locator, while Structure B may perform extra work to remove stale
entries and compact its heap.

The recorded results show that asymptotic complexity alone does not
determine which implementation is fastest for a particular workload.
Constant factors, queue sizes, hashing, stale-entry cleanup, and the
number and type of operations also affect the measurements.

The reported figures should be compared with the pre-measurement cost
model in Section 9. Any disagreement should be explained using the
actual workload and implementation rather than treated as a failure
of the experiment.

### 11.7 Measurement Limitations

The results presented here have the following limitations:

- The analyst report lists three seeds. The submission guidelines
  require at least five runs per plotted point, with spread shown as
  error bars or a shaded band.
- The benchmark uses a synthetic workload rather than real hospital
  data.
- Peak memory is measured for the entire process, not only the queue.
- The cost of a step-wise simulation may include work outside the
  queue, such as scanning the waiting room and writing logs. The
  measurement scope must be stated clearly.
- The benchmark results and checksums must be rerun or verified against
  the final submission commit.
- Each final figure should identify its generating script, raw data
  file, seeds, number of runs, units, and aggregation method.

## 12. Limitations and future work

- The application is a simplified simulation, not a validated real-world clinical system.
- A 30-day finite simulation cannot alone establish that no patient can starve. Any starvation guarantee needs explicit assumptions about arrival rates, service capacity, severity bounds and aging.
- Step-wise aging in the reviewed application scans the waiting room per simulated minute; scheduled escalation events are a possible improvement.
- A fixed seed supports repeatability under the same conditions but does not replace replaying a recorded common operation trace for rigorous cross-implementation comparisons.
- Results in the analyst draft report three seeds; figure requirements and independent provenance must be verified.

## 13. References

1. Project 10 specification and CS 253 / MA 253 Lab Project Submission Guidelines.
2. Team. `INTERFACE.md`, common interface and structural invariants. 
3. Ashu Newatia. `docs/derivations/D3.pdf`, aging policies and priority-key dynamics. 
