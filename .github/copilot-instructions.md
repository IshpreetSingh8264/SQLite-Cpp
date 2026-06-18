# codecrafters-sqlite-cpp — Architecture

A read-only SQLite file reader. It opens a `.db` file, walks the B-trees and
answers `.dbinfo`, `.tables` and `SELECT` queries.

This is the **CodeCrafters "Build a SQLite" course**, which is 9 read-only stages
and is complete. `CREATE TABLE`, `INSERT`, `UPDATE`, `DELETE`, `JOIN`, `DISTINCT`,
`GROUP BY`, `ORDER BY`, `LIMIT`, `VACUUM`, integrity checks and writing to disk
are **not on the curriculum** and are deliberately absent. Do not add them.

---

## 1. Read this first: the course is finished

The temptation when a project like this looks "small" is to start adding SQL.
Don't. The course was trimmed to 9 read-only stages and it is finished. Every
defect this project has is in the code that exists, not in missing features.

`std::ifstream` in binary mode is the correct way to open the file. There is no
write path and there must not be one.

## 2. Layout

```
src/
  main.cpp                     116 lines. Wiring only: args -> Database -> registry.
  types/
    query.hpp                  Data contracts, zero logic: CompareOp, LiteralValue,
                               WhereCondition, SelectQuery, ColumnValue, StorageClass
  commands/
    command_registry.{hpp,cpp} The command table (Rules 5 + 6)
  utils/
    diagnostics.{hpp,cpp}      Cross-cutting error reporting, all to STDERR
    like.{hpp,cpp}             SQL LIKE: %, _, whole-string, ASCII case folding
    value_compare.{hpp,cpp}    Affinity rules + storage-class comparison
  database.{hpp,cpp}           File access + the 100-byte database header
  page.{hpp,cpp}               One page: header fields, cell pointers, cell payloads
  record.{hpp,cpp}             Record decoding: serial types, varints, values
  btree.{hpp,cpp}              Table B-tree: full scan, count, search by rowid
  index_btree.cpp              Index B-tree: (key, rowid) entries, key -> rowids
  schema.{hpp,cpp}             sqlite_schema: tables, indexes, CREATE TABLE parsing
  sql_parser.{hpp,cpp}         Text -> SelectQuery. No file or B-tree knowledge.
  query_executor.{hpp,cpp}     The planner + the scan/filter/project pipeline
```

The three directories that exist — `types/`, `commands/`, `utils/` — are layered by
role, which is the point of Rule 3. The nine domain modules are still **flat in
`src/`**, one header/impl pair each, as they have always been. Do not invent a
`components/` directory to match the diagram in the plan; if you move a module,
move it deliberately and keep the build green.

`utils/` is a genuine leaf: `like` and `value_compare` know nothing about pages,
B-trees or files. They are pure functions over text and values, which is what makes
them testable against the real `sqlite3` in isolation.

`btree.hpp` declares two classes, `BTree` (table) and `IndexBTree` (index). Their
implementations are split across `btree.cpp` and `index_btree.cpp` because they
are different concepts with different cell formats.

Every header has a matching `.cpp`. `CMakeLists.txt` uses
`GLOB_RECURSE ... CONFIGURE_DEPENDS`, so adding a file re-triggers the build;
`target_include_directories(sqlitecore PUBLIC src)` lets layers reach each other by
role (`src/utils/...` includes `"types/query.hpp"`).

## 3. Data flow

Two entry points, both unidirectional. No layer calls back up.

```
argv ─▶ main ─▶ command_registry ─┬─▶ .dbinfo  ─▶ Database + Schema
                                  ├─▶ .tables  ─▶ Schema
                                  └─▶ select   ─▶ QueryExecutor
                                                      │
                                       ┌──────────────┴───────────────┐
                                       ▼                              ▼
                              findUsableIndex?                  full table scan
                                       │ yes                          │
                                       ▼                              │
                              IndexBTree.findRowIds                  │
                                       │                              │
                                       ▼                              │
                              BTree.findByKey(rowid) ────────────────┤
                                                                      ▼
                                                              evaluateWhere
                                                              (value_compare)
                                                                      │
                                                                      ▼
                                                              project columns
                                                                      │
                                                                      ▼
                                                                   main ─▶ stdout
```

Storage access, bottom up:

```
QueryExecutor ─▶ BTree / IndexBTree ─▶ Database::readPage ─▶ std::ifstream
                                      └▶ Page ─▶ std::vector<uint8_t>
                                                   └▶ Record::decode ─▶ ColumnValue
```

`std::ifstream` is binary-read only. There is no `std::ofstream` anywhere in
`src/`, and there should never be one.

## 4. Conventions

- **Everything is `namespace sqlite`.** It already was; keep it.
- **Headers hold contracts, not definitions.** No inline function bodies except
  trivial one-line accessors. Each header has a real `.cpp`.
- **Every module is a header/impl pair**, named the same, in the same directory.
- **Comments are bilingual**: a Punjabi/Hindi line then an English line in
  parentheses. This is the house style of the whole repo — match it when you add
  code. Do not strip the existing ones.
- **Errors go to `std::cerr`, never `std::cout`.** STDOUT carries results and
  nothing else. The CodeCrafters harness compares STDOUT exactly. The leading
  `Logs from your program will appear here` line in `main` is expected output of
  the harness and must stay.
- **No file over 600 lines.** If you push one past that, split it.
- **NULL renders as the literal `NULL`** in results, not as an empty string.
  (The bare `sqlite3` CLI renders NULL as empty because of its own `.nullvalue`
  default; that is a CLI setting, not a difference in our output.)
- **The `.tables` line ends with a trailing space**, because the harness expects
  `banana blueberry orange pear raspberry ` exactly. Don't "fix" it.

## 5. How to add a command

Rule 5: dispatch is data, not control flow. Rule 6: every handler has the same
signature. So adding a command never touches `main.cpp`.

1. Write the handler in `src/commands/command_registry.cpp`, inside the anonymous
   namespace, with exactly this signature:

   ```cpp
   void handleWhatever(Database& db, Schema& schema, const std::string& command) {
       // ...
   }
   ```

2. Add one entry to the table in `buildRegistry()`:

   ```cpp
   {".whatever", handleWhatever},
   ```

3. If the command needs a new verb, teach `SqlParser` to produce it. The parser
   is text-only and must not learn about pages or files.

4. Verify: `./your_program.sh sample.db ".whatever"` and then `codecrafters test`
   must still report 9/9.

## 6. How to add a WHERE operator

1. Add the enum to `CompareOp` in `src/types/query.hpp`.
2. Map the token to it in `SqlParser::parseCompareOp` (`src/sql_parser.cpp`).
3. Implement the semantics in `src/utils/value_compare.cpp`. The affinity rules
   (rules 1 and 2) and the storage-class rank
   (`NULL < numbers < TEXT < BLOB`) are already handled there and must apply to
   your operator too.
4. **Check `findUsableIndex` first.** The index is only used for `=`, because we
   only implement exact lookups. Any new operator must fall back to a full table
   scan, or the query will silently return the wrong rows.

## 7. Integration points

| Module | Depends on | Knows nothing about |
|---|---|---|
| `types/query.hpp` | the standard library | everything |
| `utils/*` | `types/query.hpp` | files, pages, B-trees |
| `page` | nothing (plus `record`'s varint helper) | files, B-trees, schema |
| `database` | the standard library | pages, B-trees, schema |
| `record` | `types/query.hpp` | files, pages, schema |
| `btree` / `index_btree` | `database`, `page`, `record`, `utils/diagnostics` | SQL, schema |
| `schema` | `database`, `btree`, `utils/diagnostics` | the query pipeline |
| `sql_parser` | `types/query.hpp` | files, pages, B-trees, schema |
| `query_executor` | everything below it | nothing above it |
| `commands/*` | `query_executor`, `schema` | the argument list |
| `main` | `commands/*` | every domain detail |

## 8. Testing and ground truth

`codecrafters test` is the real gate — 9/9 must pass before you commit. It
generates its own `test.db` with tables `apple`, `banana`, `blueberry`, `orange`,
`pear`, `raspberry` plus `companies` (with an index on `country`) and
`superheroes`.

The bundled `sample.db`, `superheroes.db` and `companies.db` are the better
ground truth for behaviour the harness does not cover. Diff against the real
CLI:

```sh
diff <(./your_program.sh sample.db "SELECT * FROM apples" 2>/dev/null) \
     <(sqlite3 -noheader -nullvalue NULL -separator '|' sample.db "SELECT * FROM apples")
```

`-nullvalue NULL` matters: the bare CLI renders NULL as empty, we render `NULL`.

`utils/like` and `utils/value_compare` are pure functions, so they can be
compiled into a throwaway probe and fuzzed against `sqlite3` directly. That is how
the LIKE and affinity rules were verified, and it is much faster than going
through the CLI.

Both of those are now permanent, in one command, from a clean checkout:

```sh
./tests/run.sh              # 15,535 assertions: 15,487 fuzz + 48 differential
./tests/run.sh fuzz         # just the fuzz against real sqlite3
./tests/run.sh differential # just the CLI differential on the sample DBs
ctest --test-dir build      # same suites, via CTest
```

The oracle is the real `sqlite3` — the CLI binary and the `sqlite3` module in
CPython — never a recorded expectation file. The differential suite **refuses
to run without the `sqlite3` CLI** rather than skipping, because without it
there is no oracle and the suite would be theatre.

Known gaps are reported as `XFAIL` so the suite stays a usable gate, and a
known gap that starts matching is reported as a **failure** — so fixing one
turns the suite red until you remove it from the list. `tests/README.md`
documents what these suites deliberately do not cover; read it before trusting
a green run. The most important omission: nothing here fuzzes the b-tree, the
page layer or the record codec, which is why the payload-overflow gap stays
invisible.

## 9. Known gaps — do not mistake these for finished work

These are real and still open. If you fix one, remove it from this list. The
two marked **[new]** were found by `tests/differential/compare_cli.sh` and are
pinned there as XFAIL, so that suite fails if either starts working — remove it
from both places at once.

- **`COUNT(*)` ignores the WHERE clause.** The `COUNT(*)` shortcut in
  `src/commands/command_registry.cpp` extracts the table name and counts the whole
  table, so on `sample.db` `SELECT COUNT(*) FROM apples WHERE id = 1` prints `4`
  where real SQLite prints `1`. `QueryExecutor::count()` in `src/query_executor.cpp`
  is an unused entry point for the same job, but it has the same limitation — it also
  counts the whole table with no predicate — so switching to it is not the fix.
  **This is the highest-value fix in this list.**
- **The index planner is exact-lookup only.** An index on a column is used only
  for `=`. `>`, `<` and `LIKE` on an indexed column fall back to a full scan, which
  is correct but slow. Real range and prefix scans are not implemented.
- **An index on a numeric column matches nothing.** `IndexBTree::findRowIds` reads
  index keys with `Record::getString(0)`, which only succeeds for TEXT keys.
  Reproduce: `CREATE TABLE n (k INTEGER, v TEXT); CREATE INDEX ix ON n(k);` then
  `SELECT v FROM n WHERE k = 2` returns **zero rows** where real SQLite returns
  `two` — the plan really does use the index (`SEARCH n USING INDEX ix (k=?)`), so
  this is a silent wrong answer, not a slow one. Fixing it means comparing index
  keys through `utils/value_compare` with the column's affinity, which changes the
  `findRowIds` interface. The course's index stage indexes a TEXT column, so this
  is not stage-blocking.
- **`WITHOUT ROWID` tables are unsupported.** Their root is an index-format page
  with no rowid, which `BTree` mis-parses. There is no detection and no error, so
  `CREATE TABLE w (id INTEGER PRIMARY KEY, t TEXT) WITHOUT ROWID` with rows
  `(5,'five'),(6,'six')` prints `3|NULL` twice instead of raising anything.
- **Embedded payload overflow is not implemented.** Payloads larger than the
  in-page maximum spill to overflow pages, and `Page::getCellPayload` clamps to
  the page instead of following the overflow chain. Records near the maximum row
  size will decode wrongly.
- **`page_size_` is `uint16_t` but 65536 is assigned to it** in
  `Database::parseHeader`, overflowing to 0. A 64 KiB page-size database prints
  `database page size: 0` and then dies with `Page data too small`. The compiler
  warns about this on every build; the warning is pre-existing and unfixed. No
  sample database uses a page size other than 4096.
- **REAL rendering** follows SQLite's `%.15g` plus a trailing `.0`, so `5.0` prints
  as `5.0`, but the conversion goes through `strtod`/`snprintf` rather than
  SQLite's own routine and was not exhaustively compared for extreme magnitudes.
- **[new] `NULL` is parsed as the four-character text `'NULL'`.**
  `SqlParser::parseLiteral` has no NULL case, so a bareword `NULL` falls
  through to the "everything else is a string" branch. `LiteralValue` cannot
  even represent NULL — it is `variant<int64_t, double, string, bool>`.
  Consequence: `WHERE id != NULL`, `< NULL` and `<= NULL` compare the column
  against the *text* `'NULL'`, which outranks every number, so all three are
  true for every row where real SQLite returns none. `= NULL`, `> NULL` and
  `>= NULL` return the right answer by coincidence, not by design — do not read
  those three as evidence that NULL handling works. Fixing this means adding
  NULL to `LiteralValue` and checking for it in `parseLiteral` and
  `valueMatches`.
- **[new] An unknown column returns `NULL` rows instead of an error.**
  `SELECT color FROM oranges` prints one `NULL` per row where real SQLite
  raises `no such column: color`. A typo'd column therefore looks like a
  successful query that happened to match everything, which is the most
  dangerous shape a bug can take here. `sql_parser.cpp` does not resolve
  column names against the schema.
- **`ORDER BY` is parsed and then ignored.** `SqlParser::parseSelect` fills
  `order_by_column` and `order_desc`; nothing in the executor ever reads them.
  `LIMIT` is not even parsed — the `SelectQuery::limit` field is never assigned.
  Neither is on the course, so this is harmless, but both fields are a trap.
- **Dead code:** `printResult` and `printResultCSV` are declared and defined but
  never called, and so are the `Database` move constructor and move assignment.
