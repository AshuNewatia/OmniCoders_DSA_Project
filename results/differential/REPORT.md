# A/B test results: current check and historical evidence

## Latest verification

- Source HEAD: `3f0fed091646ba02200ffb1526386e5c12167b75`.
- Current status: **COMPILE_FAILED**. No fresh A/B differential cases ran.
- Oracle driver compilation succeeded; Structure A compilation failed.
- B compilation/execution was not reached in this latest attempt.
- The upstream `src/structA/structure_A.cpp` contains stray `adversary` and
  `main` tokens, duplicated branches/declarations, and unmatched braces.
- No teammate implementation was changed to conceal or bypass these errors.

Evidence: [current manifest](latest.json), [A compiler diagnostics](A_build.log).

## Historical executed results

The preserved full run from **21 November 2026, 02:47 IST** used base commit
`5f846f7976f39441af6665df81b118bdc3084dec` plus the local oracle/harness changes.

| Implementation | Passed | Failed |
| --- | ---: | ---: |
| A versus oracle | 36 / 38 | 2 |
| B versus oracle | 38 / 38 | 0 |

A accepted duplicate IDs during insertion and build; both should throw
`std::invalid_argument`. All other cases and all twenty random seeds matched.
These historical numbers are **not results for the newly updated A source**.

- [Historical per-case report](HISTORICAL_REPORT_2026-11-21.md)
- [Preserved execution summary](summary.log)
- [Reconstructed historical manifest](historical_manifest.json)

The historical manifest was reconstructed from preserved traces/output logs
and the prior report after the new attempt overwrote `latest.json`. Its provenance
is labeled explicitly. Every saved A/B response was compared again as text with
the preserved oracle response, confirming 74 matches and 2 mismatches. This
artifact verification does not execute the queues again.

## Reproduce after upstream fixes

```powershell
python oracle/run_differential.py
python oracle/write_differential_report.py
```

The report generator intentionally refuses incomplete/failed-compilation runs.
Internal invariants, simulator aging correctness and performance are outside
this harness's functional result claims.
