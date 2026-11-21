"""Create a readable report from saved differential results without rerunning tests."""
import datetime
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'results' / 'differential'

DESCRIPTIONS = {
    'empty_errors': 'Empty extraction and missing-ID errors',
    'basic_order': 'Build, extract in priority order, then empty extraction',
    'all_tie_breaks': 'Priority key, arrival time and ID ordering',
    'decrease_preserves_patient_fields': 'Lower/equal keys; other patient fields unchanged',
    'invalid_increase_no_mutation': 'Reject larger keys and missing IDs; preserve contents',
    'arbitrary_delete_needs_upward_repair': 'Last-element replacement requires upward repair',
    'delete_first_middle_last': 'Delete from a shrinking queue and detect a removed ID',
    'replace_and_clear': 'Replacement build and empty build',
    'removed_ids_missing': 'Extracted IDs cannot be updated or deleted',
    'long_long_limits': 'Minimum and maximum long long keys',
    'duplicate_live_insert': 'Reject a patient ID already live',
    'duplicate_build': 'Reject repeated IDs in build input',
    'tie_heavy_1000': 'Full extraction order of 1,000 tie-heavy patients',
    'repeated_revisions_and_stale_delete': '400 revisions, deletion, then extraction',
    'stale_entries_below_extracted_patients': 'Logical extraction when stale entries remain below minima',
    'tombstone_churn_1500': '1,500 insert/delete cycles, missing lookup and new insertion',
    'resize_and_large_live_set': '1,500 live patients, arbitrary deletion and revision',
    'step_like_supplied_updates': 'Supplied keys change order; not a simulator test',
}


def main():
    data = json.loads((OUT / 'latest.json').read_text(encoding='utf-8'))
    cases = data.get('cases', [])
    if not cases or data['status'] not in ('PASS', 'FAIL'):
        raise RuntimeError('A completed differential run is required')
    for case in cases:
        actual = hashlib.sha256((ROOT / case['trace']).read_bytes()).hexdigest()
        if actual != case['trace_sha256']:
            raise RuntimeError('Saved trace no longer matches manifest: ' + case['name'])
    local_time = datetime.datetime.fromisoformat(data['timestamp_utc']).astimezone(
        datetime.timezone(datetime.timedelta(hours=5, minutes=30)))
    counts = {}
    for impl in ('A', 'B'):
        results = [case['results'][impl]['status'] for case in cases]
        counts[impl] = {status: results.count(status) for status in
                        ('PASS', 'FAIL', 'TIMEOUT', 'CRASH_OR_DRIVER_ERROR')}
    lines = [
        '# Structure A and B: differential test report', '',
        '**Run time:** ' + local_time.strftime('%d %B %Y, %H:%M:%S IST'), '',
        '**Tested source HEAD:** `' + data['commit'] + '`', '',
        '**Compiler:** ' + data['compiler_version'], '',
        '**Flags:** `-std=c++17 -O2 -Wall -Wextra -pedantic`', '',
        '## Results', '',
        '| Implementation | Passed | Failed | Timeouts | Crashes / driver errors |',
        '| --- | ---: | ---: | ---: | ---: |',
    ]
    for impl in ('A', 'B'):
        c = counts[impl]
        lines.append(f"| {impl} versus oracle | {c['PASS']} / {len(cases)} | {c['FAIL']} | {c['TIMEOUT']} | {c['CRASH_OR_DRIVER_ERROR']} |")
    lines += ['', f"**Suite status: {data['status']}.** {data['summary']}", '',
              'These are executed results from the saved run, not source-only predictions. '
              'The manifest records the source HEAD, working-tree status, exact source '
              'hashes, commands and per-case results. A dirty working tree means HEAD '
              'alone is not sufficient to identify all tested bytes.', '',
              '## Method', '',
              'Each saved trace was replayed unchanged against separate oracle, A and B '
              'executables. Responses were compared with the independently validated C++ '
              'oracle: successful results, exception category, and every patient field. '
              'Snapshots drain copies to check complete logical contents without changing '
              'the real queues. Each case starts in a fresh process.', '',
              'Random traces contain requested queue operations, not simulated minutes; '
              'every random operation is followed by a snapshot. The generator maintains '
              'a logical live set to choose valid IDs/keys, while test verdicts come from '
              'actual oracle execution.', '',
              '## All test cases', '',
              'Links open the common input and full A/B response logs. Operation counts exclude snapshots.', '',
              '| Case | Coverage | Operations | A | B |',
              '| --- | --- | ---: | --- | --- |']
    for case in cases:
        name = case['name']
        description = DESCRIPTIONS.get(name, 'Seeded mixed operations and full-state checks')
        lines.append(f"| [{name}]({name}/input.trace) | {description} | {case['requested_operations']:,} | "
                     f"[{case['results']['A']['status']}]({name}/A.log) | "
                     f"[{case['results']['B']['status']}]({name}/B.log) |")
    lines += ['', '## Failed cases', '']
    failures = 0
    for case in cases:
        for impl in ('A', 'B'):
            result = case['results'][impl]
            if result['status'] == 'PASS':
                continue
            failures += 1
            name = case['name']
            lines += [f"### {impl}: {name}", '', '**Status:** ' + result['status'], '']
            difference = result.get('first_difference')
            if difference:
                prefix = OUT / name / (impl + '_failing_prefix.trace')
                lines += ['First differing instruction: ' + str(difference['trace_line']), '',
                          '```text', prefix.read_text(encoding='utf-8').rstrip(), '```', '',
                          '| Expected oracle response | Actual response |', '| --- | --- |',
                          f"| `{difference['expected']}` | `{difference['actual']}` |", '',
                          f"[Difference details]({name}/{impl}_first_difference.json) · "
                          f"[Full log]({name}/{impl}.log) · [Oracle log]({name}/oracle.log)", '']
                if name in ('duplicate_live_insert', 'duplicate_build'):
                    lines += ['The contract requires `std::invalid_argument` for duplicate '
                              'live IDs. Different patients may share a priority key. '
                              'Repeated IDs can leave multiple heap records represented '
                              'by a single overwritten locator mapping. The owner should '
                              'check duplicate IDs before accepting the operation.', '']
            else:
                lines += ['Inspect the process exit status and stderr log in the manifest.', '']
    if not failures:
        lines += ['No functional mismatches were observed in this run.', '']
    lines += [
        '## Limits', '',
        '- Passing finite traces is evidence, not a proof or hidden-test guarantee.',
        '- No heap/locator/version invariant checker was run; B can return correct '
        'patients while violating an internal compaction-size bound.',
        '- Step-like supplied keys test queue updates, not the simulator clock, '
        'aging formulas or event scheduler.',
        '- Each process uses one selected queue instance. Cross-instance effects '
        'from A\'s global locator capacity are not covered.',
        '- The driver uses lower-case `src/structB/structureB.cpp`; the case-only '
        'filename collision in the repository remains a separate portability issue.',
        '- A/B implementations were not changed by this harness. No performance '
        'or peak-memory conclusions are drawn.', '',
        '## Reproduce', '', '```powershell',
        'python oracle/run_validation.py',
        'python oracle/run_differential.py',
        'python oracle/write_differential_report.py', '```', '',
        'Subset/replay commands are documented in '
        '[the harness guide](../../oracle/DIFFERENTIAL_TESTING.md). '
        '`latest.json` and `summary.log` refer to the most recent invocation, '
        'including subset runs. Regenerate this report after a new run.', '',
        '## Evidence and exact source identity', '',
        '[Run manifest](latest.json) · [Execution summary](summary.log) · '
        '[Oracle build](oracle_build.log) · [A build](A_build.log) · [B build](B_build.log)', '',
        '| Source | SHA-256 |', '| --- | --- |',
    ]
    for name, digest in data['source_sha256'].items():
        lines.append(f'| `{name}` | `{digest}` |')
    target = OUT / 'REPORT.md'
    target.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(target)


if __name__ == '__main__':
    main()
