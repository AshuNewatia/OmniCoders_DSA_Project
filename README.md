# Project 10: Emergency Triage Queue

**Team:** OmniCoders  
**Course:** CS 253 / MA 253 — Data Structures and Algorithms Laboratory

## Team and Roles

| Name | Roll Number | Role | Module | Derivation |
|---|---|---|---|---|---|
| Adityamoni | 250003008 | Structure A owner | `src/structA/` | D1 |
| Anuj Kumar | 250041004 | Structure B owner | `src/structB/` | D2 |
| Ashu Newatia | 250001011 | Architect | `app/`, `INTERFACE.md` | D3 |
| Rohit Jhajhra | 250041032 | Oracle owner | `oracle/` | D4 |
| Kambhampaty Krupalini Paul | 250001036 | Adversary | `adversary/` | D5 |
| Ritik Sarawag | 250001061 | Analyst | `bench/`, `results/` | D6 |

## Project Summary

This project simulates an emergency triage queue in a casualty ward.
Patients are treated according to priority rather than only by arrival
order. While waiting, a patient's medical severity can increase, their
priority can rise because of waiting time, and they may leave before
receiving treatment.

We compare two priority queue implementations:

- **Structure A:** An indexed binary min-heap with a locator hash table.
- **Structure B:** A binary min-heap with lazy invalidation.

An unsorted-array Oracle provides a simpler reference implementation for
correctness testing. The simulation supports linear aging and step-wise
escalation over 30 simulated days.

The project also includes adversarial input generation, benchmark code,
CSV event logs, and a local dashboard.

## Requirements

The project uses:

- A C++ compiler supporting **C++17**.
- GNU Make or `mingw32-make`.
- Python for experiment scripts.
- Node.js for the dashboard.
- A modern web browser.

Check installed versions:

```bash
g++ --version
make --version
python --version
node --version
```

On Windows, use:

```powershell
mingw32-make --version
```

Record the exact compiler, Python, Node.js, operating system, and
benchmark environment versions in `REPORT.md`.

## Build

From the repository root, run:

```bash
make all
```

On Windows with MinGW:

```powershell
mingw32-make all
```

In the version reviewed for this README, this builds the simulator
executable and the Oracle test executable.

## Quick Start: Dashboard

1. Open a terminal in the project root.
2. Start the dashboard:

   ```bash
   node dashboard/server.js
   ```

3. Open the following address in a browser on the same computer:

   ```text
   http://localhost:3000
   ```

4. Select:

   - A queue implementation.
   - An aging policy.
   - A random seed.

5. Click **Run simulation**.

The dashboard displays patient events and waiting-time statistics. A
successful run creates the latest event log at:

```text
results/dashboard_latest.csv
```

The latest dashboard run may overwrite the previous dashboard CSV.
Download or copy the file if you want to retain a particular run.

### Example Configuration

```text
Queue implementation: Structure A
Aging policy:         Step-wise escalation
Seed:                 42
Duration:             30 simulated days
```

A successful run displays the number of patients who arrived, received
treatment, left before treatment, and remained waiting at the end.
Exact values depend on the code version, configuration, and run
environment.

## Common Interface

The shared patient type is declared in `app/ITriageQueue.h`:

```cpp
struct Patient {
    int id;
    int severity;
    int arrival_time;
    long long priority_key;
};
```

All queue implementations expose the following operations:

```cpp
build_heap(patients)
insert(patient)
extract_min()
decrease_key(id, new_key)
delete_patient(id)
```

A smaller `priority_key` represents a higher queue priority.

The intended tie-breaking order is:

1. Smaller priority key.
2. Earlier arrival time.
3. Smaller patient ID.

See `INTERFACE.md` for the operation contract and structural invariants.

## Aging Policies

### Linear Aging

Let:

- `s` be current medical severity.
- `a` be arrival time.
- `t` be current simulated time.
- `C` be the common linear aging rate.

Effective urgency is:

```text
urgency(t) = s + C × (t − a)
```

The equivalent min-priority key is:

```text
dynamic_key(t) = −s − C × (t − a)
               = −s − C × t + C × a
```

The `−C × t` term is common to all waiting patients at the same time, so
it cancels when their priorities are compared. The simulator therefore
uses the static key:

```text
priority_key = −s + C × a
```

Linear aging alone does not require periodic key updates when all
patients use the same aging rate. A clinical severity increase still
requires a key update.

### Step-Wise Escalation

For a 30-minute escalation interval:

```text
steps = floor((current_time − arrival_time) / 30)

priority_key = −severity − step_amount × steps
```

The simulator calls `decrease_key()` at each applicable escalation
boundary.

The derivation and assumptions are documented in `docs/derivations/`.

## Simulation Event Order

The simulator processes possible events in this order for each simulated
minute:

1. Patient arrival.
2. Step-wise escalation, if applicable.
3. Clinical severity revision.
4. Patient departure.
5. Treatment.

This ordering means that an escalation at the same simulated minute as
a departure or treatment is processed first.

## Tests

### Oracle Tests

Run:

```bash
make test
```

On Windows:

```powershell
mingw32-make test
```

The Oracle tests in the reviewed project snapshot include:

- Insertion, key updates, deletion, and extraction.
- Equal keys and tie-breaking.
- Replacing queue contents using `build_heap`.
- Extracting from an empty queue.
- Updating or deleting a missing patient.
- Rejecting a key increase through `decrease_key`.

A successful run prints:

```text
All oracle tests passed.
```

### Differential Tests and Invariant Checks

The project compares the behavior of Structure A and Structure B with
the Oracle using matching operations. Structural invariants are
documented in `INTERFACE.md`.

**Final differential-test command:** To be confirmed for the final commit.

**Final invariant-check command:** To be confirmed for the final commit.

**Operation count, seed, and passing output:** To be added after running
the final tests.

## Evaluation Interface

The exact input format, output format, and evaluator commands for the
instructor's hidden tests must be confirmed against the Project 10
evaluation specification.

The dashboard's CSV event log is an application output and should not
be assumed to be the hidden-test input or output format.

## Adversarial Experiments

The adversary components generate input patterns and run heap
experiments. From the project root:

```bash
python adversary/run_experiments.py
```

Adversarial patterns include ordered and random inputs. Depending on the
benchmark and instrumentation, the experiments can measure priority
comparisons for different input sizes and patterns.

See `adversary/` and `REPORT.md` for the generator, benchmark design,
results, and limitations.

## Benchmarks and Figures

The `bench/` directory contains benchmarking code. Experimental CSV
files are stored under `results/` in the reviewed project snapshot.

The final command for reproducing **all** benchmarks and figures, the
number of repetitions, seeds, and expected runtime must be confirmed
for the final submission.

See `REPORT.md` for the cost model, experimental setup, figures, and
discussion of measured results.

## Repository Map

```text
.
├── README.md                  Project overview and instructions
├── REPORT.md                  Project report
├── INTERFACE.md               Common interface and invariants
├── TOOLS.md                   Tool and AI-use declaration
├── Makefile                   Build and test targets
├── app/
│   ├── ITriageQueue.h         Shared patient type and interface
│   └── main.cpp               Simulation logic
├── src/
│   ├── structA/               Indexed heap and locator
│   └── structB/               Lazy-invalidation heap
├── oracle/                    Unsorted-array Oracle and tests
├── adversary/                 Adversarial inputs and D5 experiments
├── bench/                     Benchmark code
├── dashboard/                 Dashboard server and UI
├── docs/
│   └── derivations/           Mathematical derivations
└── results/                   Simulation and experiment outputs
```


See `REPORT.md` for raw-data references, figures, predicted costs,
experimental setup, and analysis.

## Known Limitations

- The application is a simplified simulation and is not a clinical
  decision system.
- The reviewed step-wise simulator scans waiting patients each
  simulated minute to identify escalation boundaries.
- A fixed seed supports repeatability under the same conditions, but
  replaying an identical saved operation trace is a stronger way to
  compare implementations.
- A finite simulation does not by itself prove that starvation is
  impossible under every workload.
- Any unresolved test failures or incomplete features must be recorded
  here before the final submission.

## Tools and References

See `TOOLS.md` for the declaration of AI assistants and other tools.

See `REPORT.md` for the assigned reading, project references,
experimental methods, and detailed analysis.