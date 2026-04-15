from pathlib import Path
from dataclasses import dataclass
from typing import Optional
from collections.abc import Iterator
import sys

# TODO: pass these on the command line or something
TESTCASE_PATH = Path('testcases')
EXECUTABLE_PATH = Path('../x64/Debug/llisp.exe')

@dataclass
class Testcase:
    name: str
    category: Optional[str]
    file: Path


def find_tests() -> Iterator[Testcase]:
    for t in TESTCASE_PATH.glob('**/*.llisp'):
        t = t.relative_to(TESTCASE_PATH)
        parts = t.parts
        category: Optional[str] = None
        if len(parts) > 1:
            category = '/'.join(parts[:-1])
        yield Testcase(t.stem, category, t)


if __name__ == '__main__':
    if not EXECUTABLE_PATH.is_file():
        print(f'Executable {EXECUTABLE_PATH} is not a valid file.', file=sys.stderr)
        sys.exit(1)
    if not TESTCASE_PATH.is_dir():
        print(f'Could not find test cases in {TESTCASE_PATH}.', file=sys.stderr)
        sys.exit(1)

    print(list(find_tests()))
