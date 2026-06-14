#!/usr/bin/env python3
"""Fails if any test suite file has grown past the ~500-line budget.

A 2000-line test file is a test file nobody reads and nobody splits, so the
budget is enforced by the test suite rather than by good intentions. Fixture
data (JSON, .lox, .pack, manifests) is exempt -- only code is counted.
"""
import pathlib
import sys

BUDGET = 500
EXEMPT_SUFFIXES = {".json", ".lox", ".pack", ".txt", ".db", ".md", ".gz"}
SKIP_DIRS = {"build", ".git"}


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    over = []
    counted = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if SKIP_DIRS & set(path.relative_to(root).parts):
            continue
        if path.suffix in EXEMPT_SUFFIXES:
            continue
        if path.name == "check_size.py":  # this file
            continue
        try:
            n = len(path.read_text(encoding="utf-8").splitlines())
        except UnicodeDecodeError:
            continue
        counted += 1
        if n > BUDGET:
            over.append((path.relative_to(root), n))

    if over:
        print(f"FAIL {len(over)} test file(s) over the {BUDGET}-line budget:")
        for rel, n in over:
            print(f"     {n:5d}  {rel}")
        print("split these before adding more cases")
        return 1
    print(f"ok   {counted} test source file(s), all within {BUDGET} lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
