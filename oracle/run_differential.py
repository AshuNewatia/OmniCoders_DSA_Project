"""Replay identical saved traces against the oracle, A and B in isolated processes."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def record(patient):
    return ' '.join(str(field) for field in patient)


def build(patients):
    return 'BUILD ' + str(len(patients)) + ''.join(' ' + record(p) for p in patients)


def snapshot(count):
    return 'SNAPSHOT ' + str(count)


def fixed_cases():
    cases = {}
    cases['empty_errors'] = ['EXTRACT', 'DELETE 999', 'DECREASE 999 -10', snapshot(0)]
    cases['basic_order'] = [build([(101, 7, 0, -7), (102, 3, 1, -3), (103, 9, 2, -9)]),
                            snapshot(3), 'EXTRACT', snapshot(2), 'EXTRACT', 'EXTRACT', 'EXTRACT']
    cases['all_tie_breaks'] = [build([(4, 3, 20, -5), (3, 8, 10, -5),
                                    (1, 9, 10, -5), (2, 1, 0, -4)]), snapshot(4)]
    cases['decrease_preserves_patient_fields'] = [build([(1, 7, 0, -7), (2, 3, 10, -3)]),
        'DECREASE 2 -10', snapshot(2), 'DECREASE 2 -10', snapshot(2)]
    cases['invalid_increase_no_mutation'] = [build([(1, 7, 0, -7), (2, 3, 10, -3)]),
        'DECREASE 1 100', snapshot(2), 'DELETE 999', snapshot(2), 'DECREASE 999 -100', snapshot(2)]
    cases['arbitrary_delete_needs_upward_repair'] = [
        build([(i + 1, 1, 0, key) for i, key in enumerate([1, 10, 2, 11, 12, 3, 4])]),
        'DELETE 4', snapshot(6)]
    cases['delete_first_middle_last'] = [build([(1, 7, 0, -7), (2, 3, 1, -3), (3, 9, 2, -9)]),
        'DELETE 2', snapshot(2), 'DELETE 3', snapshot(1), 'DELETE 1', snapshot(0), 'DELETE 1']
    cases['replace_and_clear'] = [build([(1, 7, 0, -7)]),
        build([(2, 4, 0, -4), (3, 9, 0, -9)]), snapshot(2), build([]), snapshot(0)]
    cases['removed_ids_missing'] = ['INSERT 1 4 0 -4', 'EXTRACT', 'DECREASE 1 -10', 'DELETE 1']
    cases['long_long_limits'] = [build([(1, 1, 0, 2**63 - 1), (2, 2, 0, -2**63), (3, 3, 0, 0)]),
        snapshot(3), 'DECREASE 1 ' + str(-2**63), snapshot(3)]
    cases['duplicate_live_insert'] = [build([(1, 4, 0, -4), (2, 9, 0, -9)]),
        'INSERT 1 50 99 -100', snapshot(2)]
    # Invalid build is terminal: the shared interface does not specify its
    # exception-safety guarantee, so do not compare post-error queue contents.
    cases['duplicate_build'] = [build([(10, 5, 1, -5)]),
        build([(1, 4, 0, -4), (2, 4, 0, -4), (1, 9, 20, -9)])]
    cases['tie_heavy_1000'] = [build([(i, i % 50, i % 7, i % 11 - 5)
                                          for i in range(1000, 0, -1)]), snapshot(1000)]
    lines = [build([(1, 1, 0, 1), (2, 2, 0, 2), (3, 3, 0, 3), (4, 4, 0, 100)])]
    for key in range(99, -301, -1):
        lines += ['DECREASE 4 ' + str(key), snapshot(4)]
    lines += ['DELETE 4', snapshot(3), 'EXTRACT', snapshot(2)]
    cases['repeated_revisions_and_stale_delete'] = lines
    cases['stale_entries_below_extracted_patients'] = [
        build([(1, 1, 0, 1), (2, 2, 0, 2), (3, 3, 0, 3), (4, 4, 0, 100)]),
        'DECREASE 4 99', 'DECREASE 4 98', 'DECREASE 4 97',
        'EXTRACT', 'EXTRACT', 'EXTRACT', snapshot(1), 'EXTRACT', snapshot(0)]
    lines = []
    for i in range(1, 1501):
        lines += ['INSERT ' + record((i, 1, i, -i)), 'DELETE ' + str(i)]
    lines += [snapshot(0), 'DELETE 999999', 'INSERT 2000 5 0 -5', snapshot(1)]
    cases['tombstone_churn_1500'] = lines
    patients = [(i, i % 50, i % 9, 2000 - i) for i in range(1, 1501)]
    cases['resize_and_large_live_set'] = [build(patients), snapshot(1500),
        'DELETE 750', snapshot(1499), 'DECREASE 1 -5000', snapshot(1499)]
    # Tests queue behavior with aging-like supplied keys, not simulator formulas.
    cases['step_like_supplied_updates'] = [build([(1, 2, 0, -2), (2, 4, 10, -4)]),
        snapshot(2), 'DECREASE 1 -7', snapshot(2), 'DECREASE 2 -9', snapshot(2)]
    return cases


def random_case(seed, steps):
    rng = random.Random(seed)
    live = {}
    next_id = 1
    lines = []

    def patient():
        nonlocal next_id
        p = (next_id, rng.randrange(1, 51), rng.randrange(31), rng.randrange(-10, 11))
        next_id += 1
        return p

    for _ in range(steps):
        op = rng.randrange(8)
        if len(live) >= 64 and op == 0:
            op = 4
        if op == 0:
            p = patient(); live[p[0]] = p
            lines.append('INSERT ' + record(p))
        elif op == 1:
            patients = [patient() for _ in range(rng.randrange(16))]
            live = {p[0]: p for p in patients}
            lines.append(build(patients))
        elif op in (2, 3, 4) and live:
            pid = rng.choice(sorted(live))
            p = live[pid]
            if op == 2:
                key = p[3] - rng.randrange(6)
                live[pid] = p[:3] + (key,)
                lines.append(f'DECREASE {pid} {key}')
            elif op == 3:
                del live[pid]; lines.append(f'DELETE {pid}')
            else:
                # The generator needs a logical live set to keep later requests
                # valid. Test verdicts still come from actual oracle execution.
                best = min(live.values(), key=lambda item: (item[3], item[2], item[0]))
                del live[best[0]]; lines.append('EXTRACT')
        elif op == 5 and live:
            p = live[rng.choice(sorted(live))]
            lines.append(f'DECREASE {p[0]} {p[3] + 1}')
        elif op == 6:
            lines.append('DELETE 0')
        elif op == 7:
            lines.append('DECREASE 0 -100')
        elif not live:
            lines.append('EXTRACT')
        else:
            p = live[rng.choice(sorted(live))]
            lines.append(f'DECREASE {p[0]} {p[3]}')
        lines.append(snapshot(len(live)))
    return lines


def run_process(command, cwd, env, timeout):
    try:
        result = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                                text=True, timeout=timeout)
        return {'exit_code': result.returncode, 'stdout': result.stdout,
                'stderr': result.stderr, 'timeout': False}
    except subprocess.TimeoutExpired as error:
        def text(value):
            return value.decode(errors='replace') if isinstance(value, bytes) else value or ''
        return {'exit_code': None, 'stdout': text(error.stdout),
                'stderr': text(error.stderr), 'timeout': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', help='g++/clang++ executable or full path')
    parser.add_argument('--seed', type=int, help='one random seed instead of default seeds 1..20')
    parser.add_argument('--steps', type=int, default=2000)
    parser.add_argument('--case', help='run only this named case')
    parser.add_argument('--replay', type=Path, help='replay a saved .trace file')
    parser.add_argument('--timeout', type=int, default=20, help='seconds per executable per case')
    args = parser.parse_args()
    if not 1 <= args.steps <= 100000 or not 1 <= args.timeout <= 300:
        parser.error('steps must be 1..100000; timeout must be 1..300')
    if args.replay and (args.case or args.seed is not None):
        parser.error('--replay cannot be combined with --case or --seed')
    output = ROOT / 'results' / 'differential'
    output.mkdir(parents=True, exist_ok=True)
    report = {'status': 'NOT_RUN', 'timestamp_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'scope': 'Functional queue differential testing; no internal invariants, timing or simulator verification.',
              'arguments': {**vars(args), 'replay': str(args.replay) if args.replay else None},
              'builds': {}, 'cases': []}
    sources = ['app/ITriageQueue.h', 'oracle/OracleQueue.h', 'oracle/OracleQueue.cpp',
               'oracle/differential_driver.cpp', 'oracle/run_differential.py',
               'src/structA/structure_A.cpp', 'src/structA/locator_for_structure_A.cpp',
               'src/structB/structureB.cpp']
    report['source_sha256'] = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in sources}
    try:
        report['commit'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        report['git_status'] = subprocess.check_output(['git', 'status', '--short'], cwd=ROOT, text=True).splitlines()
    except (OSError, subprocess.CalledProcessError):
        report['commit'] = 'unknown'

    def save():
        (output / 'latest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

    candidates = [args.compiler] if args.compiler else ['g++', 'clang++',
        str(ROOT / 'tmp/toolchains/mingw64/bin/g++.exe'),
        r'C:\msys64\ucrt64\bin\g++.exe', r'C:\msys64\mingw64\bin\g++.exe',
        r'C:\MinGW\bin\g++.exe', r'C:\Program Files\LLVM\bin\clang++.exe']
    compiler = next((shutil.which(c) for c in candidates if shutil.which(c)), None)
    if not compiler:
        report['reason'] = 'No g++/clang++ found; nothing compiled or executed.'
        save(); print(report['reason']); return 2

    env = os.environ.copy()
    env['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + env.get('PATH', '')
    build_dir = ROOT / 'tmp' / 'differential'
    build_dir.mkdir(parents=True, exist_ok=True)
    flags = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-pedantic']
    executables = {}
    summary_lines = []
    try:
        version = run_process([compiler, '--version'], ROOT, env, 15)
        report['compiler_version'] = version['stdout'].splitlines()[0] if version['stdout'] else 'unknown'
        for implementation, macro in [('oracle', None), ('A', 'DIFF_QUEUE_A'), ('B', 'DIFF_QUEUE_B')]:
            executable = build_dir / (implementation + ('.exe' if os.name == 'nt' else ''))
            command = [compiler] + flags
            if macro:
                command.append('-D' + macro)
            command += [str(ROOT / 'oracle/differential_driver.cpp')]
            if implementation == 'oracle':
                command.append(str(ROOT / 'oracle/OracleQueue.cpp'))
            command += ['-o', str(executable)]
            result = run_process(command, ROOT, env, 120)
            (output / (implementation + '_build.log')).write_text(result['stdout'] + result['stderr'], encoding='utf-8')
            report['builds'][implementation] = {'command': command, 'exit_code': result['exit_code'],
                                                'timeout': result['timeout']}
            if result['timeout'] or result['exit_code'] != 0:
                report['status'] = 'COMPILE_FAILED'; save()
                print('Compile failed:', implementation, '(see build log)'); return 2
            executables[implementation] = executable

        if args.replay:
            cases = {'replay': args.replay.resolve().read_text(encoding='utf-8').splitlines()}
        else:
            cases = fixed_cases()
            seeds = [args.seed] if args.seed is not None else range(1, 21)
            for seed in seeds:
                cases['random_seed_' + str(seed)] = random_case(seed, args.steps)
            if args.case:
                if args.case not in cases:
                    raise ValueError('Unknown case: ' + args.case + '; available: ' + ', '.join(cases))
                cases = {args.case: cases[args.case]}

        for name, lines in cases.items():
            case_dir = output / name
            case_dir.mkdir(parents=True, exist_ok=True)
            trace_path = case_dir / 'input.trace'
            data = '\n'.join(lines) + '\n'
            trace_path.write_bytes(data.encode('utf-8'))
            case = {'name': name, 'trace': str(trace_path.relative_to(ROOT)),
                    'trace_sha256': hashlib.sha256(data.encode()).hexdigest(),
                    'trace_lines': len(lines),
                    'requested_operations': sum(not line.startswith('SNAPSHOT ') for line in lines),
                    'results': {}}
            expected = None
            for implementation in ['oracle', 'A', 'B']:
                result = run_process([str(executables[implementation]), str(trace_path)], ROOT, env, args.timeout)
                (case_dir / (implementation + '.log')).write_text(result['stdout'], encoding='utf-8')
                (case_dir / (implementation + '.stderr.log')).write_text(result['stderr'], encoding='utf-8')
                answers = result['stdout'].splitlines()
                outcome = {'exit_code': result['exit_code'], 'timeout': result['timeout'],
                           'response_lines': len(answers)}
                if implementation == 'oracle':
                    if result['timeout'] or result['exit_code'] != 0 or len(answers) != len(lines):
                        outcome['status'] = 'REFERENCE_ERROR'
                        case['results'][implementation] = outcome
                        report['cases'].append(case); report['status'] = 'REFERENCE_ERROR'
                        save(); print('Oracle reference execution failed:', name); return 2
                    expected = answers
                    outcome['status'] = 'REFERENCE'
                else:
                    mismatch = next((i for i in range(max(len(expected), len(answers)))
                                     if (expected[i] if i < len(expected) else None) !=
                                        (answers[i] if i < len(answers) else None)), None)
                    outcome['status'] = 'TIMEOUT' if result['timeout'] else (
                        'CRASH_OR_DRIVER_ERROR' if result['exit_code'] != 0 else (
                            'FAIL' if mismatch is not None else 'PASS'))
                    if mismatch is not None:
                        first = {'trace_line': mismatch + 1,
                                 'operation': lines[mismatch] if mismatch < len(lines) else '<extra output>',
                                 'expected': expected[mismatch] if mismatch < len(expected) else '<no response>',
                                 'actual': answers[mismatch] if mismatch < len(answers) else '<missing response>'}
                        outcome['first_difference'] = first
                        (case_dir / (implementation + '_first_difference.json')).write_text(json.dumps(first, indent=2), encoding='utf-8')
                        (case_dir / (implementation + '_failing_prefix.trace')).write_bytes(
                            ('\n'.join(lines[:mismatch + 1]) + '\n').encode('utf-8'))
                    message = f"{outcome['status']} {implementation} {name}"
                    if mismatch is not None:
                        message += f" | trace line {mismatch + 1}: {lines[mismatch][:100] if mismatch < len(lines) else '<extra output>'}"
                    print(message); summary_lines.append(message)
                case['results'][implementation] = outcome
            report['cases'].append(case)
            save()

        verdicts = [r for c in report['cases'] for k, r in c['results'].items() if k != 'oracle']
        report['passed'] = sum(r['status'] == 'PASS' for r in verdicts)
        report['failed'] = len(verdicts) - report['passed']
        report['status'] = 'PASS' if report['failed'] == 0 else 'FAIL'
        report['summary'] = f"SUMMARY cases={len(cases)} comparisons={len(verdicts)} passed={report['passed']} failed={report['failed']}"
        print(report['summary']); summary_lines.append(report['summary'])
        (output / 'summary.log').write_text('\n'.join(summary_lines) + '\n', encoding='utf-8')
        save(); print('Report:', output / 'latest.json')
        return 0 if report['status'] == 'PASS' else 1
    except (OSError, ValueError) as error:
        report['status'] = 'SETUP_ERROR'; report['reason'] = str(error)
        save(); print('Setup error:', error); return 2


if __name__ == '__main__':
    sys.exit(main())
