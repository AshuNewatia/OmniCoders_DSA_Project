# Oracle validation results

## Preserved successful run

**47 named tests passed; 0 failed.** The preserved [run.log](run.log) includes
25 targeted tests, 4,374 bounded exhaustive builds, a 2,000-patient tie-heavy
batch and 20 seeded cases containing 40,000 requested operations.

The log was last written at `2026-11-20T21:10:27.844847+00:00`.
[Historical summary and log hash](historical_success.json) explicitly record
that this is earlier evidence, not a fresh execution.

## Latest rerun

- Source HEAD: `3f0fed091646ba02200ffb1526386e5c12167b75`.
- C++ compilation succeeded.
- Execution status: **EXECUTION_ERROR**.
- Windows reported: `[WinError 4551] An Application Control policy has blocked this file`.
- No fresh test results were produced. No security policy was disabled or bypassed.

The older passing log remains present because the runner does not delete prior
logs on setup/execution failure. Read [latest.json](latest.json) to determine
which stages actually ran; do not treat an old log as a successful new run.

## Reproduce on a machine that permits the test executable

```powershell
python oracle/run_validation.py
```

See [test methodology](../../oracle/TESTING.md). Passing these finite tests is
evidence, not a proof of every possible input or a check of simulator aging.
