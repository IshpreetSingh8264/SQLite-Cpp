#!/usr/bin/env bash
#
# Differential test: run the same query through our binary and through the real
# sqlite3 CLI, and require byte-identical output.
#
# The bundled sample.db / superheroes.db / companies.db are the better ground
# truth for behaviour the course harness does not cover, because real SQLite is
# the oracle -- not a hand-written expectation.
#
# `-nullvalue NULL` matters: the bare CLI renders NULL as the empty string, we
# render NULL. Without it every row containing a NULL is a false difference.
# `-separator '|'` matches our own column separator. `-noheader` drops the
# column names, which real SQLite also emits and we do not.
#
# Usage: compare_cli.sh <path-to-sqlite-binary> [repo-dir]

set -uo pipefail

BIN="${1:?usage: compare_cli.sh <path-to-sqlite-binary> [repo-dir]}"
REPO="${2:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
cd "$REPO" || exit 1

pass=0
fail=0
xfail=0
SQLITE_CLI="${SQLITE_CLI:-sqlite3}"

if ! command -v "$SQLITE_CLI" >/dev/null 2>&1; then
  echo "FATAL: the real sqlite3 CLI is not installed. This suite is a" >&2
  echo "       differential against it -- without it there is no oracle." >&2
  echo "       Debian/Ubuntu: apt install sqlite3   Arch: pacman -S sqlite" >&2
  exit 2
fi

# --------------------------------------------------------------------------
# Known gaps.
#
# `run_case db query label "reason"` marks a case as a known gap. It is still
# RUN every time and its output is still shown, but a difference is recorded
# as XFAIL rather than FAIL, so the suite remains a usable gate while the gap
# is open. The reason lives in the case itself rather than in a variable set
# earlier, because a sticky flag silently leaves the following cases unmarked
# and understates the gap.
#
# The direction that matters is the reverse one: if a known gap starts MATCHING,
# that is a FAILURE ("now matches, remove it from the list"). A known-gap list
# that quietly absorbs a fix is worse than no list, because the next reader has
# no idea the behaviour changed. This is the convention in
# .github/copilot-instructions.md section 9: "If you fix one, remove it from
# this list."
# --------------------------------------------------------------------------

# $1=db $2=query $3=label [$4=known-gap reason]
run_case() {
  local db="$1" q="$2" label="$3" reason="${4:-}" ours theirs differs=0
  ours=$("$BIN" "$db" "$q" 2>/dev/null)
  theirs=$("$SQLITE_CLI" -noheader -nullvalue NULL -separator '|' "$db" "$q" 2>/dev/null)
  [ "$ours" != "$theirs" ] && differs=1

  if [ -n "$reason" ]; then
    if [ "$differs" -eq 1 ]; then
      xfail=$((xfail+1))
      printf 'XFAIL %-50s %s\n      known gap: %s\n' "$label" "$q" "$reason"
    else
      fail=$((fail+1))
      printf 'FAIL  %-50s %s\n      KNOWN GAP BUT IT NOW MATCHES -- remove it from the list\n' \
        "$label" "$q"
    fi
    return
  fi

  if [ "$differs" -eq 0 ]; then
    pass=$((pass+1)); printf 'PASS  %-50s %s\n' "$label" "$q"
  else
    fail=$((fail+1)); printf 'FAIL  %-50s %s\n' "$label" "$q"
    printf '  ours   (%d lines):\n' "$(printf '%s' "$ours" | grep -c .)"
    printf '%s\n' "$ours" | head -6 | sed 's/^/    | /'
    printf '  sqlite3 (%d lines):\n' "$(printf '%s' "$theirs" | grep -c .)"
    printf '%s\n' "$theirs" | head -6 | sed 's/^/    | /'
  fi
}

# A dot-command has no real-SQLite equivalent, so it is checked against a
# literal expectation. Kept separate from run_case so the difference in kind is
# visible rather than smuggled in.
run_literal() {
  local db="$1" cmd="$2" label="$3" expected="$4" ours
  ours=$("$BIN" "$db" "$cmd" 2>/dev/null)
  if [ "$ours" == "$expected" ]; then
    pass=$((pass+1)); printf 'PASS  %-50s %s\n' "$label" "$cmd"
  else
    fail=$((fail+1)); printf 'FAIL  %-50s %s\n' "$label" "$cmd"
    printf '    ours: [%s]\n    want: [%s]\n' "$ours" "$expected"
  fi
}

NULL_GAP="parseLiteral has no NULL case, so the bareword NULL becomes the 4-character text 'NULL' and the comparison runs against that string instead of doing nothing"
ORDER_GAP="ORDER BY is not implemented (sql_parser.cpp marks it a future enhancement); rows come back in storage order"
LIMIT_GAP="LIMIT is parsed but never applied, so every row is returned"

echo "===== dot-commands (no real-SQLite equivalent) ====="
run_literal sample.db      ".dbinfo" ".dbinfo sample" \
  "$(printf 'database page size: 4096\nnumber of tables: 2')"
run_literal sample.db      ".tables"  ".tables sample"  "apples oranges "
run_literal superheroes.db ".dbinfo" ".dbinfo superheroes" \
  "$(printf 'database page size: 4096\nnumber of tables: 1')"
run_literal superheroes.db ".tables"  ".tables superheroes"  "superheroes "
run_literal companies.db   ".dbinfo"  ".dbinfo companies" \
  "$(printf 'database page size: 4096\nnumber of tables: 1')"
run_literal companies.db   ".tables"  ".tables companies"  "companies "

echo
echo "===== full scans ====="
run_case sample.db     "SELECT * FROM apples"          "full scan apples"
run_case sample.db     "SELECT * FROM oranges"         "full scan oranges"
run_case sample.db     "SELECT * FROM sqlite_sequence" "full scan sqlite_sequence"
run_case superheroes.db "SELECT * FROM superheroes"    "full scan superheroes"
run_case companies.db  "SELECT * FROM companies"       "full scan companies"

echo
echo "===== counts ====="
run_case sample.db     "SELECT COUNT(*) FROM apples"      "count apples"
run_case sample.db     "SELECT COUNT(*) FROM oranges"     "count oranges"
run_case superheroes.db "SELECT COUNT(*) FROM superheroes" "count superheroes"
run_case companies.db  "SELECT COUNT(*) FROM companies"    "count companies"

echo
echo "===== WHERE on an integer primary key ====="
run_case sample.db "SELECT * FROM apples WHERE id = 1"   "pk = 1"
run_case sample.db "SELECT * FROM apples WHERE id = 2"   "pk = 2"
run_case sample.db "SELECT * FROM apples WHERE id > 2"   "pk > 2"
run_case sample.db "SELECT * FROM apples WHERE id >= 3"  "pk >= 3"
run_case sample.db "SELECT * FROM apples WHERE id < 2"   "pk < 2"
run_case sample.db "SELECT * FROM apples WHERE id <= 2"  "pk <= 2"
run_case sample.db "SELECT * FROM apples WHERE id != 1"  "pk != 1"
run_case sample.db "SELECT * FROM apples WHERE id = 99"  "pk = 99 (no rows)"

echo
echo "===== projection ====="
run_case sample.db "SELECT name, color FROM apples WHERE id = 3" "projection + where"
run_case sample.db "SELECT name FROM apples"                   "projection only"
run_case sample.db "SELECT description FROM oranges"          "projection oranges"
# oranges has columns (id, name, description) -- there is no `color` column.
run_case sample.db "SELECT color FROM oranges" "unknown column" \
  "an unknown column yields NULL rows instead of a parse error, so a typo'd column returns rows rather than failing"

echo
echo "===== WHERE on text (index lookup) ====="
run_case superheroes.db "SELECT * FROM superheroes WHERE name = 'Superman'"             "text = full scan"
run_case superheroes.db "SELECT * FROM superheroes WHERE name = 'Batman'"               "text = 2"
run_case superheroes.db "SELECT * FROM superheroes WHERE name = 'Spider-Man (Peter Parker)'" "text = 3"
run_case superheroes.db "SELECT name FROM superheroes WHERE name = 'Iron Man'"          "text = projection"
run_case sample.db "SELECT * FROM apples WHERE color = 'Red'"                            "text color filter"
run_case sample.db "SELECT * FROM apples WHERE color = 'Green'"                          "text color 2"
run_case sample.db "SELECT * FROM apples WHERE name = 'nope'"                             "text = no match"

echo
echo "===== LIKE ====="
run_case superheroes.db "SELECT name FROM superheroes WHERE name LIKE 'Sup%'"   "LIKE Sup%"
run_case superheroes.db "SELECT name FROM superheroes WHERE name LIKE '%man'"   "LIKE %man"
run_case superheroes.db "SELECT name FROM superheroes WHERE name LIKE 'B_tman'" "LIKE B_tman"
run_case superheroes.db "SELECT name FROM superheroes WHERE name LIKE '%a%'"    "LIKE %a%"
run_case superheroes.db "SELECT name FROM superheroes WHERE name LIKE 'Bat%'"   "LIKE Bat%"
run_case superheroes.db "SELECT name FROM superheroes WHERE name LIKE 'Sup'"    "LIKE Sup (no wildcard)"
run_case superheroes.db "SELECT name FROM superheroes WHERE name LIKE '%zzz%'"  "LIKE %zzz% (none)"

echo
echo "===== affinity between a column and a literal ====="
run_case sample.db "SELECT * FROM apples WHERE id = '1'"      "int col = text literal"
run_case sample.db "SELECT * FROM apples WHERE name = 1"      "text col = int literal"
run_case sample.db "SELECT * FROM apples WHERE id = '1abc'"   "int col = junk text literal"
run_case sample.db "SELECT * FROM apples WHERE id = '1.0'"    "int col = '1.0'"
run_case sample.db "SELECT * FROM apples WHERE color = 'Red'"  "text col = text literal"

echo
echo "===== NULL matches nothing, under ANY operator ====="
# =, > and >= happen to agree with SQLite, and not by design: the bareword NULL
# becomes the text 'NULL', and TEXT outranks every number, so those three
# comparisons are false for every row -- which is the right answer reached for
# the wrong reason. !=, < and <= are the ones that actually diverge. If a NULL
# case is ever added to that "right answer" group, it is coincidence, not
# correctness, and the same root cause will still be there underneath.
run_case sample.db "SELECT * FROM apples WHERE id = NULL"  "= NULL"
run_case sample.db "SELECT * FROM apples WHERE id > NULL"  "> NULL"
run_case sample.db "SELECT * FROM apples WHERE id >= NULL" ">= NULL"
run_case sample.db "SELECT * FROM apples WHERE id != NULL" "!= NULL" "$NULL_GAP"
run_case sample.db "SELECT * FROM apples WHERE id < NULL"  "< NULL"  "$NULL_GAP"
run_case sample.db "SELECT * FROM apples WHERE id <= NULL" "<= NULL" "$NULL_GAP"

echo
echo "===== ORDER BY ====="
run_case sample.db "SELECT name FROM apples ORDER BY name"      "ORDER BY asc"      "$ORDER_GAP"
run_case sample.db "SELECT name FROM apples ORDER BY name DESC" "ORDER BY desc"     "$ORDER_GAP"
run_case sample.db "SELECT name FROM oranges ORDER BY name"     "ORDER BY oranges"  "$ORDER_GAP"

echo
echo "===== LIMIT / OFFSET ====="
run_case sample.db "SELECT name FROM apples LIMIT 2"          "LIMIT 2"       "$LIMIT_GAP"
run_case sample.db "SELECT name FROM apples LIMIT 2 OFFSET 1" "LIMIT 2 OFFSET 1" \
  "OFFSET is not implemented either"

echo
echo "=================================================="
printf 'TOTAL: pass=%d xfail=%d fail=%d\n' "$pass" "$xfail" "$fail"
printf 'xfail = known gaps that still differ from real SQLite.\n'
printf '       They are listed in .github/copilot-instructions.md section 9.\n'
[ "$fail" -eq 0 ]
