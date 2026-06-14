#!/usr/bin/env python3
"""Differential fuzz of the three pure functions in utils/ against real SQLite.

Ground truth is taken from the `sqlite3` module that ships with CPython, which
is the same SQLite the course is graded against. Nothing here compares against
a recorded expectation file: if SQLite's behaviour changes, this suite follows
it and the comparison stays honest.

Three suites:

  affinity  columnAffinity() over a set of declared type strings, checked
            against SQLite's own value-coercion behaviour (see affinity_truth)
  like      sqlLikeMatch() over the full cross product of a set of texts and
            patterns, checked against SQLite's LIKE operator
  match     valueMatches() over declared-type x column-value x operator x
            literal, checked against a real query against a real table

Usage: fuzz_utils.py <probe-binary> [affinity|like|match]...
"""
import itertools
import pathlib
import subprocess
import sys

import sqlite3

# Affinity ints, mirroring sqlite::Affinity so the driver and the probe agree.
BLOB, TEXT, NUMERIC, INTEGER, REAL = 0, 1, 2, 3, 4

# The five affinities, each named by a declared type that produces it.
AFFINITY_TYPES = [
    ("INTEGER", INTEGER), ("INT", INTEGER), ("BIGINT", INTEGER),
    ("TEXT", TEXT), ("VARCHAR(30)", TEXT), ("CHARACTER(20)", TEXT), ("CLOB", TEXT),
    ("REAL", REAL), ("DOUBLE", REAL), ("FLOAT", REAL),
    ("NUMERIC", NUMERIC), ("DECIMAL(10,5)", NUMERIC), ("POINT", NUMERIC),
    ("BOOLEAN", NUMERIC), ("VARCHARACTER", TEXT),
    ("BLOB", BLOB), ("", BLOB), ("NOTYPE", NUMERIC),
]

# Column values in the storage form they would actually be stored in.
VALUES = [
    "n:", "i:0", "i:1", "i:-1", "i:42", "i:255", "i:-9223372036854775808",
    "r:0.0", "r:1.5", "r:-2.5", "r:3.0", "r:1e10",
    "t:", "t:1", "t:1.5", "t:abc", "t:42", "t:0", "t:ABC",
    "b:00", "b:0102", "b:ff",
]

LITERALS = [
    "i:0", "i:1", "i:-1", "i:42", "i:255",
    "r:0.0", "r:1.5", "r:-2.5", "r:3.0", "r:1e10",
    "t:", "t:1", "t:1.5", "t:abc", "t:42", "t:ABC",
    "b:0", "b:1",
]

OPS = ["=", "!=", "<", "<=", ">", ">=", "LIKE"]

LIKE_TEXTS = [
    "Superman (Clark Kent)", "Batman (Bruce Wayne)", "batman", "BATMAN", "",
    "a", "ab", "abc", "a_c", "A_C", "a%b", "a_b", "x", "xx", "xxx",
    "Super_man", "superman", "%", "_", "__", "aXbXc",
    "Spider-Man (Peter Parker)", "The Flash (Barry Allen)",
    "Wonder Woman (Diana Prince)", "Ant-Man (Hank Pym)", "Abc Def Ghi",
]

LIKE_PATTERNS = [
    "Sup%", "sup%", "SUP%", "Sup", "%Sup", "%Sup%", "Sup_man", "super_man",
    "%man", "%Man", "%MAN", "%a%", "%A%", "_", "__", "___", "", "%", "_",
    "%%", "a%", "%a", "a_", "_a", "a_c", "A_C", "a_c%", "%c%", "%x", "x%",
    "%_%", "Spider-Man%", "%Peter Parker%", "The Flash%", "%Diana Prince%",
    "%Hank Pym%", "Abc Def%", "%def%", "%Ghi", "%ghi", "a%%b", "%_%_%", "%%%%",
]

# A TEXT value with a tab, a newline and a backslash in it, because LIKE is
# perfectly happy to be handed a pattern containing any of those, and a
# tab-separated line-oriented protocol would silently truncate them.
TRICKY_TEXT = ["a\tb", "a\nb", "a\rb", " lead", "trail ", "a\\b", "%", "_", "", "\t", "\n"]


def escape(s):
    """Escape a value for the probe's tab-separated, line-oriented framing.

    The probe reverses this. Without it, a text containing a tab or a newline
    would be truncated mid-value and the case would quietly test the wrong
    thing rather than fail.
    """
    return (s.replace("\\", "\\\\").replace("\t", "\\t")
             .replace("\n", "\\n").replace("\r", "\\r"))


def run_probe(probe, mode, records):
    """Feed `records` to the probe and return its stdout lines."""
    payload = "".join(r + "\n" for r in records)
    proc = subprocess.run([str(probe), mode], input=payload,
                          capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit(f"probe mode {mode} failed rc={proc.returncode}\n{proc.stderr}")
    return proc.stdout.split("\n")


def sql_literal(spec):
    """Render a tagged value as a SQL literal for the truth query."""
    kind, _, payload = spec.partition(":")
    if kind == "n":
        return "NULL"
    if kind == "i":
        return payload
    if kind == "r":
        return payload if ("." in payload or "e" in payload) else payload + ".0"
    if kind == "b":
        return "1" if payload == "1" else "0"
    return "'" + payload.replace("'", "''") + "'"


# ---------------------------------------------------------------------------
# affinity
# ---------------------------------------------------------------------------

def affinity_truth(declared_type):
    """Ask SQLite what affinity a declared type has, by observing coercion.

    SQLite does not expose affinity directly through the Python API, but it
    applies it on INSERT, and the storage class it lands in is observable with
    typeof(). The four probes below separate Blob / Text / Real / "the numeric
    family":

        typeof(123)   numeric family -> integer, Text -> text, Blob -> integer
        typeof('123') numeric family -> integer, Text -> text, Blob -> text
        typeof('1.5') numeric family -> real,    Text -> text, Blob -> text
        typeof(3.0)   real           -> real,    others as above

    Returns a SET of affinities SQLite cannot be distinguished within. The
    numeric family comes back as {NUMERIC, INTEGER}, because no value coercion
    can tell those two apart -- see check_affinity_rule for the part of the
    rule that only a rule test can check.
    """
    con = sqlite3.connect(":memory:")
    try:
        con.execute("CREATE TABLE t (c " + declared_type + ")")
        for lit in ("123", "'123'", "'1.5'", "3.0"):
            con.execute("INSERT INTO t(c) VALUES (" + lit + ")")
        sig = tuple(r[0] for r in con.execute("SELECT typeof(c) FROM t"))
    finally:
        con.close()
    if sig == ("text", "text", "text", "text"):
        return {TEXT}
    if sig == ("real", "real", "real", "real"):
        return {REAL}
    if sig == ("integer", "text", "text", "real"):
        return {BLOB}
    return {NUMERIC, INTEGER}


def documented_affinity(declared_type):
    """SQLite's affinity rule, transcribed from the docs.

    A declared type containing "INT" -> INTEGER. Else containing CHAR, CLOB or
    TEXT -> TEXT. Else BLOB or empty -> BLOB. Else containing REAL, FLOA or DOUB
    -> REAL. Else NUMERIC.

    This is a transcription, so on its own it proves nothing. It is used only
    for the INTEGER/NUMERIC split, which affinity_truth provably cannot observe.
    """
    upper = declared_type.upper()
    if "INT" in upper:
        return INTEGER
    if "CHAR" in upper or "CLOB" in upper or "TEXT" in upper:
        return TEXT
    if not upper or "BLOB" in upper:
        return BLOB
    if "REAL" in upper or "FLOA" in upper or "DOUB" in upper:
        return REAL
    return NUMERIC


def suite_affinity(probe):
    records = [t for t, _ in AFFINITY_TYPES]
    got = [int(x) for x in run_probe(probe, "affinity", records) if x != ""]
    if len(got) != len(records):
        print(f"FAIL the probe returned {len(got)} results for {len(records)} inputs")
        return len(records), 1
    fails = 0
    for (declared, _want), actual in zip(AFFINITY_TYPES, got):
        family = affinity_truth(declared)
        if actual in family:
            note = "  (numeric family: integer/numeric not separable by coercion)" \
                if len(family) > 1 else ""
            print(f"ok   affinity({declared!r}) = {actual}{note}")
        else:
            print(f"FAIL affinity({declared!r}) = {actual}, sqlite can only be "
                  f"{sorted(family)}")
            fails += 1
    # The one part of the rule that no coercion probe can observe: the "INT"
    # substring test that separates INTEGER from NUMERIC. Note that "POINT"
    # contains "INT" (P-O-I-N-T), so POINT really is INTEGER affinity -- and
    # this is the check that knows it, where the typeof oracle cannot.
    for declared, actual in zip(records, got):
        if actual in (INTEGER, NUMERIC):
            rule = documented_affinity(declared)
            if rule == actual:
                print(f"ok   the documented INT rule: affinity({declared!r}) = {actual}")
            else:
                print(f"FAIL the documented INT rule: affinity({declared!r}) = {actual}, "
                      f"the rule says {rule}")
                fails += 1
    return len(records) * 2, fails


# ---------------------------------------------------------------------------
# like
# ---------------------------------------------------------------------------

def suite_like(probe):
    pairs = [(t, p) for t in LIKE_TEXTS for p in LIKE_PATTERNS]
    pairs += [(t, p) for t in TRICKY_TEXT for p in LIKE_PATTERNS]
    records = [f"{escape(t)}\t{escape(p)}" for t, p in pairs]
    got = [int(x) for x in run_probe(probe, "like", records) if x != ""]
    if len(got) != len(pairs):
        print(f"FAIL the probe returned {len(got)} results for {len(pairs)} inputs")
        return len(pairs), 1

    con = sqlite3.connect(":memory:")
    fails = 0
    for (text, pattern), actual in zip(pairs, got):
        truth = con.execute("SELECT (? LIKE ?)", (text, pattern)).fetchone()[0]
        if bool(actual) != bool(truth):
            if fails < 25:
                print(f"FAIL like({text!r}, {pattern!r}) = {actual}, "
                      f"sqlite says {int(bool(truth))}")
            fails += 1
    con.close()
    if not fails:
        print(f"ok   {len(pairs)} LIKE cases all match sqlite3")
    return len(pairs), fails


# ---------------------------------------------------------------------------
# match
# ---------------------------------------------------------------------------

def stored_value_tag(value):
    """Render a value python read back out of SQLite in our probe's tagging.

    This is the crux of the suite. SQLite applies the column's affinity on
    INSERT, so the value it ends up comparing is frequently NOT the value that
    was written: `0.0` into an INTEGER column is stored as INTEGER 0, and
    `'1'` into an INTEGER column likewise. Handing valueMatches the pre-insert
    value would have it comparing 0.0 against 0, and every one of those cases
    would report a difference that is an artefact of the harness rather than a
    fact about the code. So: insert, read back, and feed the read-back value to
    both sides.
    """
    if value is None:
        return "n:"
    if isinstance(value, bool):
        return "i:" + str(int(value))
    if isinstance(value, int):
        return f"i:{value}"
    if isinstance(value, float):
        return "r:" + repr(value)
    if isinstance(value, bytes):
        return "b:" + value.hex()
    return "t:" + escape(value)


def suite_match(probe):
    types = [t for t, _ in AFFINITY_TYPES if t in
             ("INTEGER", "TEXT", "BLOB", "REAL", "NUMERIC")]
    combos = list(itertools.product(types, VALUES, OPS, LITERALS))

    # For every (declared type, value to insert) pair, ask SQLite what the
    # column actually holds afterwards. Both sides then see the same input.
    con = sqlite3.connect(":memory:")
    stored = {}
    for declared in types:
        for value in VALUES:
            con.execute("DROP TABLE IF EXISTS t")
            con.execute("CREATE TABLE t (c " + declared + ")")
            con.execute("INSERT INTO t(c) VALUES (" + sql_literal(value) + ")")
            row = con.execute("SELECT c FROM t").fetchone()
            stored[(declared, value)] = row[0]
    truth = []
    for declared, value, op, lit in combos:
        con.execute("DROP TABLE IF EXISTS t")
        con.execute("CREATE TABLE t (c " + declared + ")")
        con.execute("INSERT INTO t(c) VALUES (" + sql_literal(value) + ")")
        sql_op = "LIKE" if op == "LIKE" else op
        truth.append(int(con.execute(
            f"SELECT EXISTS(SELECT 1 FROM t WHERE c {sql_op} {sql_literal(lit)})"
        ).fetchone()[0]))
    con.close()

    records = [f"{d}\t{stored_value_tag(stored[(d, v)])}\t{op}\t{lit}"
               for d, v, op, lit in combos]
    got = [int(x) for x in run_probe(probe, "match", records) if x != ""]
    if len(got) != len(combos):
        print(f"FAIL the probe returned {len(got)} results for {len(combos)} inputs")
        return len(combos), 1

    fails = 0
    by_op = {}
    for (declared, value, op, lit), actual, expected in zip(combos, got, truth):
        if actual != expected:
            by_op[op] = by_op.get(op, 0) + 1
            if fails < 30:
                got_val = stored[(declared, value)]
                print(f"FAIL match({declared}, stored {got_val!r}, {op}, {lit}) "
                      f"= {actual}, sqlite says {expected}")
            fails += 1
    if not fails:
        print(f"ok   {len(combos)} affinity/comparison cases all match sqlite3")
    else:
        print(f"     differences by operator: "
              + ", ".join(f"{k}={v}" for k, v in sorted(by_op.items())))
    return len(combos), fails


SUITES = {"affinity": suite_affinity, "like": suite_like, "match": suite_match}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    probe = pathlib.Path(sys.argv[1]).resolve()
    wanted = sys.argv[2:] or list(SUITES)
    total = fails = 0
    for name in wanted:
        if name not in SUITES:
            print(f"unknown suite: {name}", file=sys.stderr)
            return 2
        print(f"\n--- {name} ---")
        t, f = SUITES[name](probe)
        total += t
        fails += f
    print(f"\nTOTAL: {total} cases, {total - fails} matched sqlite3, {fails} differed")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
