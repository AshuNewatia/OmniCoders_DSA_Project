# Historical A/B differential test report

> This is the earlier successful execution of the harness, not a rerun on latest main. See [current verification status](REPORT.md).

**Run time:** 21 November 2026, 02:47:23 IST

**Repository HEAD:** `5f846f7976f39441af6665df81b118bdc3084dec`

**Compiler:** g++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r3) 14.2.0

**Build flags:** `-std=c++17 -O2 -Wall -Wextra -pedantic`

## 1. Result

| Implementation | Passed | Failed | Timeouts | Crashes / driver errors |
| --- | ---: | ---: | ---: | ---: |
| Structure A versus oracle | 36 / 38 | 2 | 0 | 0 |
| Structure B versus oracle | 38 / 38 | 0 | 0 | 0 |

**Overall:** 76 A/B comparisons: 74 passed and 2 failed. The complete suite is **FAIL**, because A violates duplicate-ID rejection in two cases. B passed every functional case in this run.

All three executables compiled successfully. The oracle completed all 38 reference traces. No candidate timed out or crashed. This report uses the saved execution results; it does not infer results merely from source inspection.

**Snapshot qualification:** HEAD identifies the base commit, but the working tree was not clean: the oracle duplicate-ID fix and this harness were local changes. Exact tested source SHA-256 hashes are recorded below and in `latest.json`. These results are not a claim about the unmodified GitHub commit alone.

## 2. How the comparison worked

- Each case was saved as one input trace and replayed unchanged against the C++ oracle, A and B.
- A and B ran in separate executables to avoid conflicting helper-type names in their sources.
- Successful results and exception categories were compared with actual oracle responses. Exception message wording was ignored.
- Extracted records were compared on all four fields: ID, stored medical severity, arrival time and priority key.
- At checkpoints, a copy of each queue was drained in order and compared, followed by an extra extraction that must report an empty queue. The real trace queue was left intact.
- Random traces contained 20 seeds with 2,000 requested operations each: **40,000 requested operations per implementation**, plus snapshot checks. They are queue operations, not simulated hospital minutes.

## 3. Every test and its result

Click a case name for its input. Click PASS or FAIL for that implementation's full responses. Requested-operation counts exclude SNAPSHOT observations.

| Test case | What was checked | Requested operations | A | B |
| --- | --- | ---: | --- | --- |
| [empty_errors](empty_errors/input.trace) | Empty extraction and missing-ID deletion/update throw the required exception. | 3 | [PASS](empty_errors/A.log) | [PASS](empty_errors/B.log) |
| [basic_order](basic_order/input.trace) | Build, extract in priority order, and become empty. | 5 | [PASS](basic_order/A.log) | [PASS](basic_order/B.log) |
| [all_tie_breaks](all_tie_breaks/input.trace) | Order by key, then earlier arrival, then smaller ID. | 1 | [PASS](all_tie_breaks/A.log) | [PASS](all_tie_breaks/B.log) |
| [decrease_preserves_patient_fields](decrease_preserves_patient_fields/input.trace) | A decrease changes the key without changing ID, severity or arrival; equal updates are allowed. | 3 | [PASS](decrease_preserves_patient_fields/A.log) | [PASS](decrease_preserves_patient_fields/B.log) |
| [invalid_increase_no_mutation](invalid_increase_no_mutation/input.trace) | Reject a larger key and missing IDs without changing logical contents. | 4 | [PASS](invalid_increase_no_mutation/A.log) | [PASS](invalid_increase_no_mutation/B.log) |
| [arbitrary_delete_needs_upward_repair](arbitrary_delete_needs_upward_repair/input.trace) | Delete a patient whose last-element replacement must move upward. | 2 | [PASS](arbitrary_delete_needs_upward_repair/A.log) | [PASS](arbitrary_delete_needs_upward_repair/B.log) |
| [delete_first_middle_last](delete_first_middle_last/input.trace) | Remove several IDs from a shrinking queue; detect missing ID afterward. | 5 | [PASS](delete_first_middle_last/A.log) | [PASS](delete_first_middle_last/B.log) |
| [replace_and_clear](replace_and_clear/input.trace) | Build replaces prior contents, and an empty build clears the queue. | 3 | [PASS](replace_and_clear/A.log) | [PASS](replace_and_clear/B.log) |
| [removed_ids_missing](removed_ids_missing/input.trace) | Extracted patient IDs are absent for later update/deletion. | 4 | [PASS](removed_ids_missing/A.log) | [PASS](removed_ids_missing/B.log) |
| [long_long_limits](long_long_limits/input.trace) | Handle minimum/maximum long long keys without subtraction-based comparison overflow. | 2 | [PASS](long_long_limits/A.log) | [PASS](long_long_limits/B.log) |
| [duplicate_live_insert](duplicate_live_insert/input.trace) | Reject an insertion whose ID already belongs to a live patient. | 2 | [FAIL](duplicate_live_insert/A.log) | [PASS](duplicate_live_insert/B.log) |
| [duplicate_build](duplicate_build/input.trace) | Reject a build collection with a repeated patient ID. | 2 | [FAIL](duplicate_build/A.log) | [PASS](duplicate_build/B.log) |
| [tie_heavy_1000](tie_heavy_1000/input.trace) | Build and check the full extraction order of 1,000 patients with many ties. | 1 | [PASS](tie_heavy_1000/A.log) | [PASS](tie_heavy_1000/B.log) |
| [repeated_revisions_and_stale_delete](repeated_revisions_and_stale_delete/input.trace) | Apply 400 revisions, then delete the revised patient and continue extracting. | 403 | [PASS](repeated_revisions_and_stale_delete/A.log) | [PASS](repeated_revisions_and_stale_delete/B.log) |
| [stale_entries_below_extracted_patients](stale_entries_below_extracted_patients/input.trace) | Extract small keys before a repeatedly revised large-key patient; verify logical output. | 8 | [PASS](stale_entries_below_extracted_patients/A.log) | [PASS](stale_entries_below_extracted_patients/B.log) |
| [tombstone_churn_1500](tombstone_churn_1500/input.trace) | Perform 1,500 insert/delete cycles, a missing-ID lookup, and a new insertion. | 3,002 | [PASS](tombstone_churn_1500/A.log) | [PASS](tombstone_churn_1500/B.log) |
| [resize_and_large_live_set](resize_and_large_live_set/input.trace) | Build 1,500 live patients, delete one and decrease another; compare full contents. | 3 | [PASS](resize_and_large_live_set/A.log) | [PASS](resize_and_large_live_set/B.log) |
| [step_like_supplied_updates](step_like_supplied_updates/input.trace) | Apply supplied step-like keys that change the patient order; not a simulator test. | 3 | [PASS](step_like_supplied_updates/A.log) | [PASS](step_like_supplied_updates/B.log) |
| [random_seed_1](random_seed_1/input.trace) | Seed 1: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_1/A.log) | [PASS](random_seed_1/B.log) |
| [random_seed_2](random_seed_2/input.trace) | Seed 2: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_2/A.log) | [PASS](random_seed_2/B.log) |
| [random_seed_3](random_seed_3/input.trace) | Seed 3: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_3/A.log) | [PASS](random_seed_3/B.log) |
| [random_seed_4](random_seed_4/input.trace) | Seed 4: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_4/A.log) | [PASS](random_seed_4/B.log) |
| [random_seed_5](random_seed_5/input.trace) | Seed 5: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_5/A.log) | [PASS](random_seed_5/B.log) |
| [random_seed_6](random_seed_6/input.trace) | Seed 6: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_6/A.log) | [PASS](random_seed_6/B.log) |
| [random_seed_7](random_seed_7/input.trace) | Seed 7: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_7/A.log) | [PASS](random_seed_7/B.log) |
| [random_seed_8](random_seed_8/input.trace) | Seed 8: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_8/A.log) | [PASS](random_seed_8/B.log) |
| [random_seed_9](random_seed_9/input.trace) | Seed 9: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_9/A.log) | [PASS](random_seed_9/B.log) |
| [random_seed_10](random_seed_10/input.trace) | Seed 10: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_10/A.log) | [PASS](random_seed_10/B.log) |
| [random_seed_11](random_seed_11/input.trace) | Seed 11: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_11/A.log) | [PASS](random_seed_11/B.log) |
| [random_seed_12](random_seed_12/input.trace) | Seed 12: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_12/A.log) | [PASS](random_seed_12/B.log) |
| [random_seed_13](random_seed_13/input.trace) | Seed 13: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_13/A.log) | [PASS](random_seed_13/B.log) |
| [random_seed_14](random_seed_14/input.trace) | Seed 14: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_14/A.log) | [PASS](random_seed_14/B.log) |
| [random_seed_15](random_seed_15/input.trace) | Seed 15: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_15/A.log) | [PASS](random_seed_15/B.log) |
| [random_seed_16](random_seed_16/input.trace) | Seed 16: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_16/A.log) | [PASS](random_seed_16/B.log) |
| [random_seed_17](random_seed_17/input.trace) | Seed 17: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_17/A.log) | [PASS](random_seed_17/B.log) |
| [random_seed_18](random_seed_18/input.trace) | Seed 18: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_18/A.log) | [PASS](random_seed_18/B.log) |
| [random_seed_19](random_seed_19/input.trace) | Seed 19: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_19/A.log) | [PASS](random_seed_19/B.log) |
| [random_seed_20](random_seed_20/input.trace) | Seed 20: mixed operations, errors, replacement builds and full-state checkpoints. | 2,000 | [PASS](random_seed_20/A.log) | [PASS](random_seed_20/B.log) |

## 4. Exact failures in Structure A

### Failure 1: `duplicate_live_insert`

**First differing trace instruction:** 2

Minimal saved prefix through the first mismatch (not an automatically minimized test):

```text
BUILD 2 1 4 0 -4 2 9 0 -9
INSERT 1 50 99 -100
```

| Expected oracle response | Actual A response |
| --- | --- |
| `ERROR invalid_argument` | `OK INSERT` |

The build creates live patients 1 and 2. The next insertion uses ID 1 again, with different severity, arrival and priority fields. The same ID cannot represent two live patients. A reports successful insertion instead of throwing `std::invalid_argument`.

**Source cause:** `Min_Heap::insert` appends to the heap without a duplicate-ID check. Its locator insertion updates an existing mapping for that ID, so two heap records can share one locator entry. This can invalidate both uniqueness and locator consistency.

**Recommended owner action:** detect an already-live ID using the custom locator before appending or otherwise mutating the heap; throw `std::invalid_argument`. Keep duplicate priority keys valid for distinct IDs.

Evidence: [first difference](duplicate_live_insert/A_first_difference.json), [A response log](duplicate_live_insert/A.log), [B response log](duplicate_live_insert/B.log), [oracle response log](duplicate_live_insert/oracle.log).

### Failure 2: `duplicate_build`

**First differing trace instruction:** 2

Minimal saved prefix through the first mismatch (not an automatically minimized test):

```text
BUILD 1 10 5 1 -5
BUILD 3 1 4 0 -4 2 4 0 -4 1 9 20 -9
```

| Expected oracle response | Actual A response |
| --- | --- |
| `ERROR invalid_argument` | `OK BUILD` |

The second build supplies IDs 1, 2 and 1. A reports a successful build even though ID 1 appears twice. The existing queue before that build contains patient 10; replacing a queue is allowed, but duplicate IDs inside the supplied collection are not.

**Source cause:** `Min_Heap::build_heap` copies all input records and inserts locator mappings without detecting repeated IDs. The repeated mapping is overwritten while both patient records remain in the heap.

**Recommended owner action:** detect duplicates during input/locator construction and throw `std::invalid_argument`, while preserving the intended expected-linear build complexity. Document any guarantee about queue contents after a rejected build.

**Test scope:** this invalid-build case ends at the rejected operation. No particular post-exception state was required of A or B because the shared interface does not specify that guarantee.

Evidence: [first difference](duplicate_build/A_first_difference.json), [A response log](duplicate_build/A.log), [B response log](duplicate_build/B.log), [oracle response log](duplicate_build/oracle.log).

## 5. Structure B findings

B matched the oracle in **38 out of 38 cases**, including both duplicate-ID exceptions, all tie-breaking cases, repeated revisions, deletion after revisions, hash-table growth/churn, and every random seed.

**No functional failure was observed for B in this suite.** This is not proof of all required B properties. In particular, matching logical extraction output does not prove that compaction occurs whenever stale entries exceed live entries, nor that total memory is bounded by the live count.

## 6. Interpretation and limitations

- A passed all 36 other cases, including the arbitrary-deletion upward-repair trace and the tie-break tests. Earlier audit findings about an older version must not be substituted for these current executed results.
- All 20 random traces passed for both structures. Random successful insert/build requests use unique IDs; duplicate rejection is deliberately covered by separate targeted cases. Random passes do not cancel the two observed A failures.
- The tested oracle had already passed its independent 47-test suite after its duplicate-ID fix. Its responses, rather than hand-entered expected outputs for every random operation, were used for A/B comparisons.
- These are finite functional tests, not a mathematical proof, an internal invariant check, a hidden-test guarantee or a performance study.
- The supplied step-like key trace tests queue updates only. No simulator clock, aging formula or event scheduler was executed by this harness.
- One selected queue instance is used per process per case. Cross-instance effects, such as A's shared global locator capacity, are outside this run.
- B uses `src/structB/structureB.cpp`. The repository also tracks `StructureB.cpp`, which collides on Windows. The source hash below identifies the actual compiled bytes.
- A/B code was not fixed by this test contribution. Existing failed cases remain regression tests for the owners to rerun.

## 7. Reproduce

Run the complete suite from the repository root:

```powershell
python oracle/run_differential.py
```

Run only the two failing cases:

```powershell
python oracle/run_differential.py --case duplicate_live_insert
python oracle/run_differential.py --case duplicate_build
```

Or replay a saved failure prefix:

```powershell
python oracle/run_differential.py --replay results/differential/duplicate_live_insert/A_failing_prefix.trace
```

A subset/replay invocation overwrites `latest.json` and `summary.log` with its own results. This report is a snapshot of the full run identified above; regenerate it after queue changes and a new full run.

## 8. Evidence and source identity

- [Reconstructed historical manifest and case verdicts](historical_manifest.json)
- [Compact execution summary](summary.log)
- [Harness instructions](../../oracle/DIFFERENTIAL_TESTING.md)

| Tested source | SHA-256 |
| --- | --- |
| `app/ITriageQueue.h` | `bb2b833d2bbf23155f0f66f14ca5f58e4925f51ae28144583277c29d2457008f` |
| `oracle/OracleQueue.h` | `e1037ccaac31c870aa7cf8b686dedfa09301053fa5e40bc0aad6b5764a418446` |
| `oracle/OracleQueue.cpp` | `4d88ee56923b4a84870724695e835c30eee525981e7e75caf1d2eb4b90bb465e` |
| `oracle/differential_driver.cpp` | `83cded2e044ac39d99845a242413767a1c693be35843829d0a95abdae280c17d` |
| `oracle/run_differential.py` | `13d833101cb64b03e5a2542689f729cc3a0e3dffb684f131f844cde0bd188f5f` |
| `src/structA/structure_A.cpp` | `8718d59be0148848be7a4aedb4c0aaed10139896c37cf1762d06a6a1c840a5a3` |
| `src/structA/locator_for_structure_A.cpp` | `89a3990653065b934f88decd551f14d2412bbbcba82ac47a7e4dc07f51025820` |
| `src/structB/structureB.cpp` | `4974219e1e38a73003ec9f84d24e90bb21f2fca51b86665f8b0180d4ef10c6be` |

**Next step:** A's owner fixes duplicate insert/build rejection, then the team reruns the full unchanged suite. Internal invariant checkers, independent simulator aging tests and D4 remain separate Oracle-role deliverables.
