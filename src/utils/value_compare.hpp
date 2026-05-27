#pragma once

#include "types/query.hpp"
#include <string>

// ============================================================================
// UTILS/VALUE_COMPARE.HPP - SQL Comparison With Affinity Rules
// ============================================================================
// SQLite comparing values nalo pehlan "affinity" apply karda hai. Column de
// declared type vichon ek affinity nikalda hai, te usde hisaab naal dusre
// operand nu convert karda hai. Usde baad hi storage class di rank vich
// comparison hoyi hundi hai.
// (Before comparing, SQLite applies "affinity". An affinity is derived from the
// column's declared type, and the other operand is converted according to it.
// Only then is the comparison made, by storage-class rank.)
//
// Pehlan assi sirf same-type comparisons karde si; koi bhi mismatched type
// hunda te seedha `false`. Isliye `WHERE str_col = 5` te `WHERE int_col = '5'`
// kuch nahi dende si.
// (We used to compare only when both sides had the same type, and returned
// false immediately on any mismatch - so `WHERE str_col = 5` matched nothing.)
//
// Rule 3: iss utility vich koi domain knowledge nahi - e pure text di dafa da
// function aa, te sirf `types/query.hpp` de contracts use kardi hai.
// (Rule 3: no domain knowledge here - these are pure functions over text, using
// only the `types/query.hpp` contracts.)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Affinity - Column de declared type vichon nikli gayi preference
// (The preference derived from a column's declared type)
// ----------------------------------------------------------------------------
enum class Affinity {
    Blob,     // Koi preference nahi - BLOB ya empty type (no preference - BLOB or empty type)
    Text,     // CHAR / CLOB / TEXT vadde hon
    Numeric,  // Baaki sab kuch (everything else)
    Integer,  // INT vachda aa
    Real      // REAL / FLOAT / DOUBLE vachde hon
};

// ----------------------------------------------------------------------------
// Column Affinity - Column da declared type nu Affinity vich badlo
// (Turn a column's declared type into its Affinity)
// ----------------------------------------------------------------------------
// `declared_type` wohi aa jo CREATE TABLE vich likheya si, e.g. "INTEGER",
// "VARCHAR(20)", "text".
Affinity columnAffinity(const std::string& declared_type);

// ----------------------------------------------------------------------------
// Real To Text - REAL value nu SQLite jive text bana do
// (Render a REAL value to text the way SQLite does)
// ----------------------------------------------------------------------------
// C++ da `ostringstream << 5.0` nu "5" deta hai, par SQLite "5.0" deta hai.
// Isi liye `1 LIKE 1.0` galat ho jandi si.
// (C++'s `ostringstream << 5.0` yields "5" but SQLite yields "5.0", which is why
// `1 LIKE 1.0` was wrong.)
//
// SQLite "%!.15g" use karda hai: 15 significant digits, te trailing ".0" jadon
// add karda aa jado koi decimal point ya exponent na ho.
// (SQLite uses "%!.15g": 15 significant digits, and a trailing ".0" is added
// whenever there is no decimal point or exponent.)
std::string realToText(double value);

// ----------------------------------------------------------------------------
// Is Numeric Affinity - Kya e affinity numeric family vich aa?
// (Is this affinity in the numeric family?)
// ----------------------------------------------------------------------------
// Rule 1 te "NUMERIC affinity dusre operand lai apply kardi" eh integer,
// real te numeric teeno lai pawaandi aa.
// (Rule 1 says NUMERIC affinity is applied to the other operand; integer, real
// and numeric all count.)
bool isNumericAffinity(Affinity affinity);

// ----------------------------------------------------------------------------
// Value Matches - Kya record di value WHERE condition manak kardi hai?
// (Does the record's value satisfy the WHERE condition?)
// ----------------------------------------------------------------------------
// `column_affinity` e column di hai jithon `record_value` aaya si.
// (`column_affinity` is that of the column `record_value` came from.)
//
// NULL kisi naal match nahi hunda, kisi operator naal - jive SQL vich.
// (NULL never matches, under any operator, as in SQL.)
bool valueMatches(const ColumnValue& record_value, Affinity column_affinity,
                  CompareOp op, const LiteralValue& literal);

// ----------------------------------------------------------------------------
// Compare Column Values - Do values nu operator naal compare karo
// (Compare two values with the given operator)
// ----------------------------------------------------------------------------
// Kya e `record_value` WHERE clause manak karda hai, ohi decide karda hai - ohdi
// badle kuch nahi. Isliye e `valueMatches` da core aa.
// (It decides whether `record_value` satisfies the WHERE clause and nothing
// more, so it is the core of `valueMatches`.)
//
// Column vich di value te koi affinity nahi rakhi - ke usde da TEXT hai ya
// INTEGER oh aap dasso, usse pehlan call karo. (Callers must first say whether
// the value in the column is TEXT or INTEGER; this function does not guess.)
bool compareColumnValues(const ColumnValue& left, const ColumnValue& right, CompareOp op);

} // namespace sqlite
