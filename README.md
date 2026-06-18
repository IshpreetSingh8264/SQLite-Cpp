# SQLite — C++

A read-only SQLite file reader in C++23, built for the
[CodeCrafters "Build your own SQLite" challenge](https://codecrafters.io/challenges/sqlite).

It opens a real `.db` file, walks the B-trees, decodes the record format, parses `SELECT` statements, and answers with
the same results `sqlite3` would — including SQLite's type affinity rules and `LIKE` semantics, both fuzzed against
CPython's `sqlite3` module.

> **Scope note.** This is a **reader**. It never writes, and it only understands `SELECT`. There is no `INSERT`,
> `UPDATE`, `DELETE`, `CREATE`, `JOIN`, `GROUP BY`, or `LIMIT`. The gaps are listed in
> [Known limitations](#known-limitations) rather than left to be discovered.

## Contents

- [Quick start](#quick-start)
- [Usage](#usage)
- [SQL support](#sql-support)
- [Storage engine](#storage-engine)
- [Query execution](#query-execution)
- [Comparison rules](#comparison-rules)
- [Tests](#tests)
- [Known limitations](#known-limitations)
- [Project layout](#project-layout)

## Quick start

Requires CMake 3.13+, a C++23 compiler, and Python 3 — the CMake configure step requires a Python interpreter even
just to build the CLI, because the test targets need one.

```bash
./download_sample_databases.sh     # fetches superheroes.db and companies.db
cmake -B build -S .
cmake --build ./build

./build/sqlite sample.db ".dbinfo"
./build/sqlite sample.db ".tables"
./build/sqlite sample.db "SELECT name, age FROM heroes WHERE age > 30"
```

Or in one step:

```bash
./your_program.sh sample.db "SELECT COUNT(*) FROM heroes"
```

```
$ ./build/sqlite sample.db ".dbinfo"
database page size: 4096
number of tables: 1
$ ./build/sqlite sample.db "SELECT name, age FROM heroes WHERE age > 30"
Peter|35
```

## Usage

One process, one command:

```
sqlite <database_file> <command>
```

| Command | Output |
|---|---|
| `.dbinfo` | `database page size: <n>` and `number of tables: <n>`, excluding `sqlite_*` internal tables. |
| `.tables` | All non-`sqlite_*` table names, single-space separated, alphabetical. |

Anything else is rejected. `.schema`, `.read`, `.dump`, `.indexes`, `.mode`, and the rest are **not** implemented.

Results go to stdout, one row per line, fields separated by `|`, no header. Everything else — errors, warnings, and a
one-line diagnostic summary — goes to stderr, because stdout is what gets parsed.

| Situation | Exit code |
|---|---|
| Success | 0 |
| Wrong argument count, unknown command, unreadable database | 1 |
| `SELECT` parse or execution error | 0, with the error on stderr |

## SQL support

`SELECT` is the only statement. The grammar actually implemented:

```sql
SELECT  <column> | *   [, <column> ...]
FROM    <table>                          -- required, one table, no alias
[ WHERE <column> <op> <literal> ]        -- exactly one predicate
[ ORDER BY <column> [DESC] ]             -- parsed, then ignored
```

**Operators:** `=` `!=` `<>` `<` `<=` `>` `>=` `LIKE` (case-insensitive).

**Literals:** quoted strings, integers, reals, `true` / `false`, and anything else as text.

**Functions and aggregates:** none. The one exception is `SELECT COUNT(*) FROM t`, which is special-cased before the
parser runs and counts the whole table.

Supported across the nine course stages: page size, table count, table names, row count, single-column projection,
multi-column projection, `WHERE`, full-table scan, and index scan.

## Storage engine

**Header.** The 100-byte SQLite header is checked for the `SQLite format 3` magic. Four fields are read: page size,
page count, text encoding, and schema cookie. Everything else in the header is ignored, including the freelist, the
write and read versions, and the largest root B-tree.

**Pages.** 4096 bytes in the bundled databases, though the value is always taken from the file. Page 1 is offset by 100
bytes to make room for the database header. Cell pointers, cell payload bounds, and interior-page child pointers are all
handled.

**Two B-trees.** `BTree` walks table B-trees; cells are `payload_size` varint, `rowid` varint, record. `IndexBTree`
walks index B-trees; cells are `payload_size` varint, record — with no rowid varint, because the rowid is the last column
of an index record.

**Records.** The full serial-type set is decoded: null, the six integer widths, IEEE-754 doubles, the 0 and 1
constants, blobs, and text. Values become a `variant` of exactly SQLite's five storage classes.

**Rowids** are read, never assigned. There is no write path, so no `max+1` scan and no rowid reuse.

**A rowid alias** is detected properly: a column must be declared `PRIMARY KEY`, its type must be exactly `INTEGER`
after stripping and upper-casing, and it must not be `PRIMARY KEY DESC`. When one exists, its slot in the record still
holds a null placeholder, and projection, `WHERE` evaluation, and extraction all substitute the real rowid.

## Query execution

```
main
 └─ handleSelect
     ├─ Schema::load()                    tables and indexes from sqlite_schema
     ├─ "COUNT(*)" in the text?  ── yes ──► BTree::countRecords()
     └─ SqlParser::parseSelect
         └─ QueryExecutor::execute
             ├─ look up the table definition
             ├─ findUsableIndex(where)?
             │     yes ──► IndexBTree::findRowIds ──► BTree::findByKey per rowid
             │     no  ──► BTree::scanAll
             └─ per row: evaluateWhere, then project
```

**There is no query planner.** One `if`: the index is used only when there is a `WHERE` and its operator is `=`. Ranges,
`!=`, and `LIKE` always fall back to a full scan, because only exact lookups are implemented and using the index for a
range would return the wrong rows. Among usable indexes the first match wins, walking `std::map` order — so
alphabetically by index name, not by any cost estimate.

An index is usable only when the `WHERE` is on its **leading** column. Composite indexes are otherwise ignored, as is
`is_unique`.

A row found through an index is re-checked against the `WHERE` clause before being emitted. That is a safety net for
affinity differences rather than a redundancy.

## Comparison rules

`columnAffinity` implements SQLite's documented order exactly:

| Order | Declared type contains | Affinity |
|---|---|---|
| 1 | `INT` | Integer |
| 2 | `CHAR`, `CLOB`, or `TEXT` | Text |
| 3 | empty, or `BLOB` | Blob |
| 4 | `REAL`, `FLOA`, or `DOUB` | Real |
| 5 | anything else | Numeric |

Comparisons then follow: a null on either side is never equal to anything; numeric families coerce the other operand
through `strtod` with a well-formedness check; a text-affinity column coerces the other side to text; and otherwise the
storage-class rank decides — `NULL < Number < Text < Blob`.

`LIKE` is a real implementation, not a `find`: two pointers with backtracking, `%` and `_` only, whole-string match,
ASCII-only case folding, and `_` consuming one UTF-8 character rather than one byte. A column's affinity is deliberately
ignored for `LIKE` — both sides go to text, which is what SQLite does.

## Tests

```bash
./tests/run.sh                 # configure, build, both suites, size budget
./tests/run.sh fuzz
./tests/run.sh differential
ctest --test-dir build --output-on-failure
```

The differential suite needs the real `sqlite3` CLI on `PATH` and a Python 3 with the `sqlite3` module. It exits
nonzero immediately if `sqlite3` is missing, rather than silently skipping.

**Unit suite — 15,487 cases against CPython's `sqlite3`:**

| Mode | Cases | Composition |
|---|---|---|
| `affinity` | 36 | 18 declared type strings × 2 assertions |
| `like` | 1,591 | 37 texts × 43 patterns |
| `match` | 13,860 | 5 declared types × 22 values × 7 operators × 18 literals |

Ground truth comes from CPython's in-memory `sqlite3`, never from a recorded file, so the suite cannot drift. Because
SQLite applies affinity on `INSERT`, the `match` oracle inserts a row, reads the value back, and feeds the *read-back*
value to both sides.

**Differential suite — 57 cases, 48 expected to match.** Each query is run through this binary and through real
`sqlite3 -noheader -nullvalue NULL -separator '|'`, and stdout must be byte-identical.

The 9 known gaps are declared as XFAIL. A gap that starts *matching* is reported as a failure with a message telling you
to remove it from the list, so a fixed bug cannot hide.

`tests/check_size.py` fails if any test file passes 500 lines.

## Known limitations

The first group is the dangerous kind: cases where this returns a plausible wrong answer rather than an error.

1. **`COUNT(*)` ignores the `WHERE` clause** and counts the whole table.
2. **An index on a numeric column matches nothing.** Index keys are read as text, so `WHERE int_col = 1` returns zero
   rows when an index exists on that column.
3. **An unknown column in the projection yields `NULL` rows** instead of `no such column`.
4. **`AND`, `OR`, `NOT`, `IN`, and `BETWEEN` are silently dropped**, not rejected. `WHERE a = 1 AND b = 2` filters on
   `a = 1` only.
5. **Parentheses are consumed as separators and never emitted**, so `WHERE (a = 1 OR b = 2)` degenerates to `a = 1`.
6. **A bare `NULL` is the four-character text `NULL`.** It is not a literal, and `LiteralValue` cannot represent null.
7. **A trailing `;` is not stripped**, so `SELECT * FROM t;` looks for a table named `t;`.
8. **Identifiers are case-sensitive.** `SELECT * FROM APPLES` fails where real SQLite succeeds.
9. **Overflow pages are not followed.** A record near the maximum row size decodes to garbage.
10. **64 KiB page-size databases are broken.** The page size is stored in a `uint16_t` and 65536 wraps to 0, so every
    read fails.
11. **Truncated or hostile input can read out of bounds.** The bounds check in the record decoder does not account for
    the bytes already consumed.

By design, and not bugs:

- No writes at all. There is no `std::ofstream` anywhere in `src/`, and there should never be one.
- No `CREATE`, `INSERT`, `UPDATE`, `DELETE`, `DROP`, `ALTER`, `PRAGMA`, or `EXPLAIN`.
- No `JOIN`, `GROUP BY`, `HAVING`, `DISTINCT`, `UNION`, subqueries, or CTEs.
- `ORDER BY` is parsed and then ignored; `LIMIT` and `OFFSET` are not parsed at all.
- No transactions, no journal, no WAL. A database in WAL mode is not detected, and a hot WAL is read as if the pages
  were not there.
- No views, no triggers, no `sqlite_stat1`, no `IS NULL`, no scalar functions.
- `WITHOUT ROWID` tables are mis-parsed rather than rejected.
- UTF-16 databases are read as raw bytes; the text encoding field is parsed and then never consulted.

## Project layout

```
src/
  main.cpp                  argument handling, no REPL loop
  database.cpp              file header, page reads
  page.cpp                  page types, cell pointers, payload bounds, varints
  record.cpp                serial types, record decode
  btree.cpp                 table B-tree: scan and findByKey
  index_btree.cpp           index B-tree: findRowIds
  schema.cpp                sqlite_schema parsing, table and index definitions
  sql_parser.cpp            SELECT grammar
  query_executor.cpp        planning, scanning, projection, WHERE
  commands/command_registry.cpp   the three-entry command map
  utils/                    LIKE, value comparison, affinity, diagnostics
tests/
  unit/                     fuzz probe and driver
  differential/             CLI-vs-sqlite3 comparison
  run.sh, check_size.py
download_sample_databases.sh
```

Sources are picked up by `GLOB_RECURSE ... CONFIGURE_DEPENDS`, so adding a file re-triggers CMake. `src/` is the include
root, which is why includes are role-based: `#include "types/query.hpp"`.

## Licence

No licence file is present in this repository. Add one before redistributing.
