# Simple array oracle

The oracle stores waiting patients in one `std::vector<Patient>`: a resizable
unsorted array. Selection, revision and deletion use ordinary scans. A temporary
standard-library `unordered_set` is used only to reject duplicate IDs during
build; it is not retained and does not select patients.

## How it works

- `build_heap`: validate unique IDs, then copy the patients, replacing contents.
- `insert`: scan for a duplicate ID, then append the patient.
- `extract_min`: scan every patient and select the best one.
- `decrease_key`: scan for an ID and change its key.
- `delete_patient`: scan for an ID and remove that patient.
- `empty` and `size`: inspect the current queue without changing it.

Removal copies the last patient into the removed slot, then calls `pop_back`.
This avoids shifting all subsequent patients. Array order is unimportant.

Smaller `priority_key` values win. Ties use earlier arrival, then smaller ID,
as specified in `INTERFACE.md`. Medical severity is a separate field.
Duplicate live IDs in insert or build throw `std::invalid_argument`, even if
their other fields differ. Rejection leaves the existing queue unchanged.
Duplicate priority keys are allowed for different patient IDs.
Missing IDs and empty extraction throw `std::out_of_range`; an attempted key
increase throws `std::invalid_argument`. Equal-key updates are allowed.

The simulator supplies aging-adjusted keys. This queue does not apply aging
again. Functional A/B differential tests are available; see
`DIFFERENTIAL_TESTING.md`. Independent simulator aging tests, internal invariant
checks and derivation D4 remain separate work.

## Efficiency

| Operation | Time |
| --- | --- |
| Build | O(n) expected; O(n²) worst case for hash-based duplicate validation |
| Insert | O(n) |
| Extract minimum | O(n) |
| Decrease key | O(n) |
| Delete by ID | O(n) |
| Size / empty | O(1) |

Stored space is O(n). Build additionally uses O(n) temporary space to validate
IDs. Insertion scans existing records, so its total cost is O(n), even though
appending alone is O(1) amortized. Minimum selection still scans the full array.

## Run the basic tests

From the repository root, with a C++ compiler available:

```powershell
g++ -std=c++11 -Wall -Wextra -pedantic oracle/OracleQueue.cpp oracle/oracle_tests.cpp -o oracle/oracle_tests.exe
.\oracle\oracle_tests.exe
```

Expected output: `All oracle tests passed.`

Do not commit the generated executable. The tests cover selection, revisions,
deletion, ties, replacement builds, negative keys, and error cases. They are
standalone tests, not yet a differential harness for the other structures.

For the extended suite (targeted cases, exhaustive small inputs and seeded
random operations), run `python oracle/run_validation.py`; see `TESTING.md`.

To compare A and B with the oracle on identical saved traces, run
`python oracle/run_differential.py`; see `DIFFERENTIAL_TESTING.md`.
