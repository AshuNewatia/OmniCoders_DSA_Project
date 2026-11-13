# Repository Guide

This document describes the files and folders present on the `main` branch of [`AshuNewatia/OmniCoders_DSA_Project`](https://github.com/AshuNewatia/OmniCoders_DSA_Project). The inventory reflects the branch at the time this guide was added; update it as files are added or moved.

## Project at a glance

The project is an Emergency Triage Queue simulator intended to compare priority-queue implementations for a casualty ward. The shared C++ queue contract and an application simulation scaffold are present. Several folders reserved for implementations, testing, benchmarking, and outputs currently contain only `.gitkeep` placeholders.

## Repository tree

```text
.
├── INTERFACE.md
├── README.md
├── REPORT.md
├── TOOLS.md
├── adversary/
│   └── .gitkeep
├── app/
│   ├── .gitkeep
│   ├── ITriageQueue.h
│   └── main.cpp
├── bench/
│   └── .gitkeep
├── docs/
│   └── derivations/
│       ├── .gitkeep
│       └── D3.tex
├── oracle/
│   └── .gitkeep
├── results/
│   └── .gitkeep
└── src/
    └── .gitkeep
```

## Root-level files

- **`README.md`** — Introduces the Emergency Triage Queue Simulator, lists team roles, and documents the intended `make all` and `make run_all` commands. At the time of this inventory, the repository tree does not contain a `Makefile`, so those commands are not currently backed by a checked-in build file.
- **`INTERFACE.md`** — Defines the shared patient model and abstract priority-queue API, along with the intended invariants for interchangeable queue implementations.
- **`REPORT.md`** — Empty placeholder file; no report content is currently present.
- **`TOOLS.md`** — Empty placeholder file; no tool documentation is currently present.
- **`detailed.md`** — This directory and file guide.

## Directories and files

### `adversary/`

Reserved for adversarial tests or test inputs intended to exercise edge cases in the queue implementations.

- **`adversary/.gitkeep`** — Empty marker file that keeps this otherwise empty directory in Git. No adversarial test files are present yet.

### `app/`

Contains the simulator-facing queue contract and the application entry point.

- **`app/ITriageQueue.h`** — Declares `Patient` (`id`, `severity`, and `arrival_time`) and the pure-virtual `ITriageQueue` API: `build_heap`, `insert`, `extract_min`, `decrease_key`, and `delete_patient`. Queue implementations are expected to implement this interface.
- **`app/main.cpp`** — Defines `AgingPolicy` (`NONE`, `LINEAR`, `STEP_WISE`), wait-time statistics, and a `simulation` routine. The routine generates simulated arrivals and treatment durations, applies optional aging and patient abandonment, and writes event records to `results/simulation_log.csv`. The current `main()` only prints a readiness message; it does not instantiate a queue or call `simulation`.
- **`app/.gitkeep`** — Empty directory marker; it is retained alongside the actual app files.

### `bench/`

Reserved for benchmark programs and performance-measurement support.

- **`bench/.gitkeep`** — Empty marker file. No benchmark source files or results are present yet.

### `docs/`

Holds project documentation and technical derivations.

#### `docs/derivations/`

- **`docs/derivations/D3.tex`** — LaTeX derivation titled “Aging Policies and Priority Key Dynamics.” It discusses static keys for linear aging and bounds the number of priority updates under step-wise escalation.
- **`docs/derivations/.gitkeep`** — Empty directory marker retained alongside `D3.tex`.

### `oracle/`

Reserved for a reference/oracle queue implementation or related correctness-checking code.

- **`oracle/.gitkeep`** — Empty marker file; no oracle implementation is present yet.

### `results/`

Intended destination for generated simulation output. In particular, `app/main.cpp` attempts to write `results/simulation_log.csv` when its simulation routine runs.

- **`results/.gitkeep`** — Empty marker file. No generated results or CSV files are currently checked in.

### `src/`

Reserved for the core data-structure implementations (the README describes comparing two queue structures).

- **`src/.gitkeep`** — Empty marker file; no queue implementation source files are present yet.

## Current implementation and build notes

- The only queue contract currently checked in is `app/ITriageQueue.h`; the `src/` and `oracle/` directories do not yet contain implementations.
- The simulator logic is defined in `app/main.cpp`, but its current entry point does not run it.
- The README documents Make-based build/run commands, but no `Makefile` or other build manifest appears in the current repository tree.
- `results/` must exist for the simulation routine to open its CSV log; the tracked `.gitkeep` keeps the directory in a clone.
