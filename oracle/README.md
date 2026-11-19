# Simple array oracle

The oracle uses one `std::vector<Patient>`: a resizable array. It has no heap,
map, hash table, sorting, or stale entries. Ordinary loops make it easy to
inspect independently of Structures A and B.

## How it works

- `build_heap`: copy the supplied patients, replacing existing contents.
- `insert`: append a patient to the array.
- `extract_min`: scan every patient and select the best one.
- `decrease_key`: scan for an ID and change its key.
- `delete_patient`: scan for an ID and remove that patient.
- `empty` and `size`: inspect the current queue without changing it.

Removal copies the last patient into the removed slot, then calls `pop_back`.
This avoids shifting all subsequent patients. Array order is unimportant.

Smaller severity keys win. Ties use earlier arrival, then smaller ID.
These tie rules must be agreed with the other owners before differential tests.
Live IDs must be unique; duplicate detection is not implemented.
Missing IDs and empty extraction throw `std::out_of_range`; an attempted key
increase throws `std::invalid_argument`. Equal-key updates are allowed.

The simulator currently supplies aging-adjusted keys. This queue does not apply
aging again. Independent aging-policy tests and heap differential tests remain
future work, along with invariant checks and derivation D4.

## Efficiency

| Operation | Time |
| --- | --- |
| Build | O(n) |
| Insert | O(1) amortized |
| Extract minimum | O(n) |
| Decrease key | O(n) |
| Delete by ID | O(n) |
| Size / empty | O(1) |

Space is O(n). Minimum selection needs a full scan in this unsorted-array
design. A map or FIFO queue is unnecessary for this reference implementation.

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
