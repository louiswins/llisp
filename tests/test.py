from pathlib import Path
from dataclasses import dataclass
from typing import Optional
from collections.abc import Iterator
import re
import subprocess
import sys

# TODO: pass these on the command line or something
TESTCASE_PATH = Path(__file__).parent / 'testcases'
EXECUTABLE_PATH = Path(__file__).parent.parent / 'x64/Debug/llisp.exe'

NAME_WIDTH = 60

REQUIREMENTS_PATTERN = re.compile('; expect: (.*)')
PROHIBITIONS_PATTERN = re.compile('; forbid: (.*)')

@dataclass
class Testcase:
    name: str
    category: Optional[str]
    file: Path
    expects: list[str]
    forbids: list[str]


def find_expectations(srcfile: Path) -> tuple[list[str], list[str]]:
    source = srcfile.read_text()
    return (REQUIREMENTS_PATTERN.findall(source), PROHIBITIONS_PATTERN.findall(source))

def find_tests() -> Iterator[Testcase]:
    for t in TESTCASE_PATH.glob('**/*.llisp'):
        rel = t.relative_to(TESTCASE_PATH)
        parts = rel.parts
        category: Optional[str] = None
        if len(parts) > 1:
            category = '/'.join(parts[:-1])
        exp, fbd = find_expectations(t)
        yield Testcase(t.stem, category, t, exp, fbd)


def run_test(test: Testcase) -> list[str]:
    res = subprocess.run([EXECUTABLE_PATH, test.file], capture_output=True, text=True)
    failures: list[str] = []
    if res.returncode != 0:
        failures.append(f'Exited with {res.returncode}')
    if res.stderr:
        failures.append(f'Wrote to stderr:\n{res.stderr}')
    for exp in test.expects:
        if exp not in res.stdout:
            failures.append(f'Expected "{exp}"')
    for fbd in test.forbids:
        if fbd in res.stdout:
            failures.append(f'Did not expect "{fbd}"')

    return failures

if __name__ == '__main__':
    if not EXECUTABLE_PATH.is_file():
        print(f'Executable {EXECUTABLE_PATH} is not a valid file.', file=sys.stderr)
        sys.exit(1)
    if not TESTCASE_PATH.is_dir():
        print(f'Could not find test cases in {TESTCASE_PATH}.', file=sys.stderr)
        sys.exit(1)

    tests: dict[Optional[str], dict[str, Testcase]] = {}
    for test in find_tests():
        if test.category not in tests:
            tests[test.category] = {}
        tests[test.category][test.name] = test

    num_failures = 0
    num_tests = 0
    for category, cases in tests.items():
        indent = ''
        if category is None:
            category = '<root tests>'
        print(f'{category}:')
        for name, case in cases.items():
            num_tests += 1
            if len(name) > NAME_WIDTH:
                name = name[:NAME_WIDTH - 3] + '...'
            name += ':'
            # +1 for the colon
            print(f'    {name.ljust(NAME_WIDTH + 1)} ', end='')
            sys.stdout.flush()

            failures = run_test(case)

            if not failures:
                print('PASSED')
            else:
                print('FAILED')
                num_failures += 1
                for fail in failures:
                    fl = fail.splitlines()
                    print('      - ' + fl[0])
                    for l in fl[1:]:
                        print('        ' + l)

        print()

    if num_failures:
        print(f'{num_tests-num_failures}/{num_tests} tests passed.')
        print(f'{num_failures} failures.')
    else:
        print(f'All tests passed ({num_tests}/{num_tests}).')
