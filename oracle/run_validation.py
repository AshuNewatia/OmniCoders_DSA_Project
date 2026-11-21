"""Compile and run the oracle validation suite; preserve real results as JSON/text."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', help='g++/clang++ executable or full path')
    parser.add_argument('--seed', type=int, help='run one random seed instead of the default 20')
    parser.add_argument('--steps', type=int, default=2000, help='operations for --seed (default: 2000)')
    args = parser.parse_args()
    if args.seed is not None and not 0 <= args.seed <= 2**64 - 1:
        parser.error('--seed must fit an unsigned 64-bit integer')
    if not 1 <= args.steps <= 1000000:
        parser.error('--steps must be 1..1000000')
    if args.seed is None and args.steps != 2000:
        parser.error('--steps requires --seed')

    output = ROOT / 'results' / 'oracle_validation'
    output.mkdir(parents=True, exist_ok=True)
    report = {'timestamp_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'status': 'NOT_RUN', 'seed': args.seed, 'steps_per_seed': args.steps,
              'scope': 'Oracle only; not A/B differential or simulator aging tests.'}
    report['source_sha256'] = {
        name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
        for name in ['app/ITriageQueue.h', 'oracle/OracleQueue.h',
                     'oracle/OracleQueue.cpp', 'oracle/validation_tests.cpp']
    }
    try:
        report['commit'] = subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        report['git_status'] = subprocess.check_output(
            ['git', 'status', '--short'], cwd=ROOT, text=True).splitlines()
    except (OSError, subprocess.CalledProcessError):
        report['commit'] = 'unknown'

    def save():
        (output / 'latest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print('Report:', output / 'latest.json')

    candidates = [args.compiler] if args.compiler else ['g++', 'clang++']
    if not args.compiler:
        candidates += [str(ROOT / 'tmp' / 'toolchains' / 'mingw64' / 'bin' / 'g++.exe'),
                       r'C:\msys64\ucrt64\bin\g++.exe', r'C:\msys64\mingw64\bin\g++.exe',
                       r'C:\MinGW\bin\g++.exe', r'C:\Program Files\LLVM\bin\clang++.exe']
    compiler = next((shutil.which(c) for c in candidates if shutil.which(c)), None)
    if not compiler:
        report['reason'] = 'No g++ or clang++ compiler found. No C++ tests were executed.'
        print(report['reason']); save(); return 2

    build = ROOT / 'tmp' / 'oracle_validation'
    build.mkdir(parents=True, exist_ok=True)
    executable = build / ('validation_tests.exe' if os.name == 'nt' else 'validation_tests')
    command = [compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-pedantic',
               str(ROOT / 'oracle' / 'OracleQueue.cpp'),
               str(ROOT / 'oracle' / 'validation_tests.cpp'), '-o', str(executable)]
    report['compile_command'] = command
    env = os.environ.copy()
    env['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + env.get('PATH', '')
    try:
        version = subprocess.run([compiler, '--version'], env=env,
                                 capture_output=True, text=True, timeout=15)
        report['compiler_version'] = version.stdout.splitlines()[0] if version.stdout else version.stderr
        compile_result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
        report['compile_exit_code'] = compile_result.returncode
        (output / 'build.log').write_text(compile_result.stdout + compile_result.stderr, encoding='utf-8')
        if compile_result.returncode:
            report['status'] = 'COMPILE_FAILED'; save(); return 2
        run = [str(executable)]
        if args.seed is not None:
            run += [str(args.seed), str(args.steps)]
        report['run_command'] = run
        result = subprocess.run(run, cwd=output, env=env, capture_output=True, text=True, timeout=180)
        text = result.stdout + result.stderr
        (output / 'run.log').write_text(text, encoding='utf-8')
        print(text, end='')
        report['run_exit_code'] = result.returncode
        report['passed'] = [line[5:] for line in result.stdout.splitlines() if line.startswith('PASS ')]
        report['failed'] = [line[5:] for line in result.stdout.splitlines() if line.startswith('FAIL ')]
        report['status'] = 'PASS' if result.returncode == 0 else 'FAIL'
        report['summary'] = next((line for line in result.stdout.splitlines() if line.startswith('SUMMARY ')), '')
        if result.returncode == 0 and not report['summary']:
            report['status'] = 'INVALID_OUTPUT'
        save(); return 0 if report['status'] == 'PASS' else 1
    except subprocess.TimeoutExpired as error:
        report['status'] = 'TIMEOUT'; report['reason'] = str(error); save(); return 2
    except OSError as error:
        report['status'] = 'EXECUTION_ERROR'; report['reason'] = str(error); save(); return 2


if __name__ == '__main__':
    sys.exit(main())
