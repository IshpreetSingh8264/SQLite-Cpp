# Tests

Local test infrastructure for the SQLite engine. **The oracle is the real
`sqlite3`**, not a recorded expectation file: the CLI binary and the `sqlite3`
module in CPython. If SQLite's behaviour changes, these suites follow it and
the comparison stays honest.

## Run everything

```sh
./tests/run.sh
```

Configures, builds, runs every suite, exits non-zero on a real failure.
**15,544 cases**: 15,487 fuzz + 57 differential, of which 48 differential
cases are expected to match and 9 are known gaps.

| Command | What it runs |
|---|---|
| `./tests/run.sh` | build + both suites |
| `./tests/run.sh fuzz` | the 15,487-case fuzz against real sqlite3 |
| `./tests/run.sh differential` | the CLI differential on the sample DBs |

`BUILD_DIR` and `PYTHON` are honoured. `ctest --test-dir build` works too.

Requires the `sqlite3` CLI (`apt install sqlite3` / `pacman -S sqlite`). The
differential suite **refuses to run without it** rather than skipping, because
without it there is no oracle and the suite would be theatre.

## What is here

```
tests/
  run.sh                          the entrypoint
  check_size.py                   fails if a suite file passes 500 lines
  unit/
    sqlite_probe.cpp              drives sqlLikeMatch / columnAffinity / valueMatches
    fuzz_utils.py                 the fuzz driver and SQLite ground truth
  differential/
    compare_cli.sh                query-by-query diff against the real CLI
```

### `unit/` — 15,487 cases against real SQLite

`utils/like.hpp` and `utils/value_compare.hpp` are pure functions, so they are
compiled into a probe and driven directly rather than through the CLI. Going
through the CLI would mean one process spawn per case, and the fact that they
are pure is exactly what makes the direct approach possible.

**`affinity` — 36 cases.** `columnAffinity()` over 18 declared type strings.
SQLite does not expose affinity through the Python API, so ground truth comes
from its *behaviour*: affinity is applied on INSERT and the resulting storage
class is visible with `typeof()`. Four probes separate `Blob` / `Text` / `Real`
/ the numeric family:

| | `typeof(123)` | `typeof('123')` | `typeof('1.5')` | `typeof(3.0)` |
|---|---|---|---|---|
| BLOB | integer | text | text | real |
| TEXT | text | text | text | text |
| REAL | real | real | real | real |
| numeric family | integer | integer | real | integer |

`INTEGER` and `NUMERIC` are **not separable by any value coercion** — SQLite's
own docs say INTEGER behaves like NUMERIC apart from a lossless
REAL→INTEGER conversion, and NUMERIC performs that same conversion. So the
typeof oracle returns the numeric *family* and the suite accepts either member.
The INTEGER/NUMERIC split is then checked against SQLite's *documented rule*
(a declared type containing `INT` is INTEGER) as a separate, explicitly
labelled assertion. This is not a papering-over: note that `POINT` contains
`INT` (P-O-I-N-T) and really is INTEGER affinity, which the typeof oracle
cannot see and the rule test can.

**`like` — 1,591 cases.** `sqlLikeMatch()` over the full cross product of 37
texts and 43 patterns, checked against SQLite's own `LIKE`. The texts and
patterns are chosen for the cases that break hand-rolled matchers: casing
(`Sup%` / `sup%` / `SUP%`), a pattern with no wildcard (`Sup` must not match
`Superman`), a bare `_` and a bare `%`, `%%` and `a%%b`, mixed-case underscore
(`a_c` vs `A_C`), and values containing a tab, a newline, a carriage return
and a backslash — which a tab-separated line-oriented protocol would otherwise
truncate silently, so they are escaped and the probe reverses it.

**`match` — 13,860 cases.** `valueMatches()` over the full product of 5
declared types × 22 column values × 7 operators × 18 literals, checked against
a real `SELECT EXISTS(...)` against a real table.

This suite has one subtlety that matters more than its size. **SQLite applies
the column's affinity on INSERT**, so the value it ends up comparing is often
not the value that was written: `0.0` into an INTEGER column is stored as
INTEGER `0`, and `'1'` likewise. Handing `valueMatches` the pre-insert value
makes it compare `0.0` against `0`, and every such case reports a difference
that is an artefact of the harness rather than a fact about the code. So the
driver **inserts, reads the value back, and feeds the read-back value to both
sides.** With that done, all 13,860 cases agree.

### `differential/` — 57 cases: 48 pass, 9 known gaps, against the real CLI

Every query is run through our binary and through
`sqlite3 -noheader -nullvalue NULL -separator '|'` and the output must be
byte-identical. `-nullvalue NULL` matters: the bare CLI renders NULL as the
empty string and we render `NULL`, so without it every NULL row is a false
difference. `-noheader` because real SQLite emits column names and we do not.

Covered: `.dbinfo` and `.tables` on all three bundled databases, full table
scans, counts, every comparison operator against an integer primary key,
projection, text lookups, seven LIKE shapes, affinity coercion between a
column and a literal, and NULL against all six comparison operators.

## Known gaps: XFAIL, and why that is not a soft pass

Nine cases are known to differ from real SQLite. They are **run every time and
their output is still shown**, but a difference is recorded as `XFAIL` so the
suite is a usable gate while the gap is open.

The direction that matters is the reverse one. **A known gap that starts
MATCHING is reported as a `FAILURE`** — "KNOWN GAP BUT IT NOW MATCHES -- remove
it from the list". A known-gap list that quietly absorbs a fix is worse than no
list, because the next reader has no idea the behaviour changed. This is the
convention already in `.github/copilot-instructions.md` section 9.

The mechanism earned its keep on the way in: three NULL cases were initially
listed as gaps, and the suite immediately reported all three as "now matches".
`= NULL`, `> NULL` and `>= NULL` do agree with SQLite — but only by accident,
because the text `'NULL'` outranks every number, so those comparisons are false
for every row. `!=`, `<` and `<=` are the ones that actually diverge. The suite
now keeps the three coincidences as *passing* cases with a comment saying they
are right for the wrong reason, so nobody later mistakes them for working.

The nine gaps:

| Case | Why |
|---|---|
| `!= NULL`, `< NULL`, `<= NULL` | `parseLiteral` has no NULL case, so the bareword `NULL` becomes the 4-character **text** `'NULL'` and the comparison runs against that string. Every integer sorts below it, so `!=`, `<` and `<=` are true for every row where SQLite says NULL matches nothing. |
| `ORDER BY` asc / desc | Not implemented — `sql_parser.cpp` marks it a future enhancement. Rows come back in storage order. |
| `LIMIT 2` | `LIMIT` is not parsed at all. `sql_parser.cpp` has no branch for it and `SelectQuery::limit` is never assigned, so the tokens are left unconsumed and ignored; all rows are returned. |
| `LIMIT 2 OFFSET 1` | Same, and `OFFSET` is not implemented either. |
| unknown column (`SELECT color FROM oranges`) | An unknown column yields `NULL` rows instead of a parse error, so a typo'd column returns rows rather than failing. The most dangerous shape of bug here: it is silent. |

## What this does NOT cover

- **`codecrafters test` is still the only authority** for the 9 course stages,
  and it generates its own `test.db`. These suites never touch that fixture;
  they run against the bundled `sample.db`, `superheroes.db` and
  `companies.db`. The course's own tables (`apple`, `banana`, `blueberry`,
  `orange`, `pear`, `raspberry`) and its `companies` index on `country` are
  therefore **not** exercised here.
- **These are not the 9 gaps in section 9 — there are more.** `COUNT(*)`
  ignoring `WHERE`, the exact-lookup-only index planner, an index on a numeric
  column matching nothing, `WITHOUT ROWID`, embedded payload overflow, the
  `uint16_t page_size_` overflow, and REAL rendering at extreme magnitudes are
  all still open and all still untested by this suite. This suite found
  *different* gaps; it did not close any of the listed ones.
- **No fuzzing of the b-tree, the page layer or the record codec.** The fuzz
  covers three pure functions. Page parsing, cell pointer arrays, overflow
  chains and interior pages are entirely untested — which is why the payload
  overflow gap is invisible here.
- **No corrupted-database testing.** Nothing feeds the engine a truncated,
  malformed or hostile `.db` file. `corrupt.db` exists in the repo's history
  but no fixture of that kind is committed.
- **The fuzz corpus is small and hand-picked, not random.** 37 texts, 43
  patterns, 22 values, 18 literals. It is chosen to hit known-tricky shapes,
  not to explore the space statistically. There is no property-based generator
  and no shrinking on failure.
- **No multi-column or compound WHERE.** Every case is a single column against a
  single literal. `AND`/`OR`/`NOT`, `IN`, `BETWEEN` and subqueries are untested.
- **No writes.** Nothing exercises `CREATE TABLE` through this engine, so
  nothing checks that a database it writes is readable by real SQLite. All
  reads; all fixtures pre-existing.
- **`DELETE`/`UPDATE` are not covered** for the same reason, and neither is
  transaction or rollback behaviour.
- **The `.dbinfo` and `.tables` expectations are literals, not differentials.**
  Real SQLite has no equivalent command, so those six are hand-written
  expectations and are only as good as the page-size and table-count
  assumptions in them.
- **`IS NULL` / `ISNULL` / `IS NOT NULL` are not tested at all.** They are
  outside the course's scope and are not implemented; the suite does not assert
  that they are rejected.
- **Timing and scale are not covered.** No large database, no deep b-tree, no
  many-column record, no performance assertion. `companies.db` is the biggest
  fixture and is a single full scan.
