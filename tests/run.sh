#!/usr/bin/env bash
#
# The one command that runs every local test in this repo.
#
#   ./tests/run.sh              # build, then run everything
#   ./tests/run.sh fuzz         # just the 15,487-case fuzz against real sqlite3
#   ./tests/run.sh differential # just the CLI differential on the sample DBs
#
# Both suites use the REAL sqlite3 as the oracle -- the `sqlite3` CLI and the
# `sqlite3` module in CPython. Neither compares against a recorded expectation
# file, so if SQLite's behaviour changes these follow it.
#
# Exits non-zero on a real failure. Known gaps are reported as XFAIL and do
# not fail the run, but a known gap that starts matching IS a failure.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO/build}"
PY="${PYTHON:-python3}"

cd "$REPO"

ALL="fuzz differential"
SUITES="$ALL"
case "${1:-}" in
  fuzz|differential) SUITES="$1"; shift ;;
esac

CMAKE_ARGS=(-B "$BUILD_DIR" -S .)
if [ -n "${VCPKG_ROOT:-}" ] && [ -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]; then
  CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
fi

want() {
  case " $SUITES " in *" $1 "*) return 0 ;; *) return 1 ;; esac
}

echo "== configure =="
cmake "${CMAKE_ARGS[@]}" >/dev/null
echo "== build =="
cmake --build "$BUILD_DIR" >/dev/null

rc=0

if want fuzz; then
  echo
  echo "== fuzz: utils/ against real sqlite3 (tests/unit/) =="
  "$PY" tests/unit/fuzz_utils.py "$BUILD_DIR/sqlite_probe" || rc=1
fi

if want differential; then
  echo
  echo "== differential: CLI against real sqlite3 (tests/differential/) =="
  bash tests/differential/compare_cli.sh "$BUILD_DIR/sqlite" || rc=1
fi

echo
echo "== suite size budget =="
"$PY" tests/check_size.py tests || rc=1

echo
if [ $rc -eq 0 ]; then
  echo "ALL SUITES PASSED"
else
  echo "SOME SUITES FAILED"
fi
exit $rc
