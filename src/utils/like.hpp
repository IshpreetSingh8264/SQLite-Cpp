#pragma once

#include <string>

// ============================================================================
// UTILS/LIKE.HPP - SQL LIKE Pattern Matching
// ============================================================================
// SQL da LIKE operator - `%` te `_` wildcards naal.
// (SQL's LIKE operator - with the `%` and `_` wildcards.)
//
// Pehlan assi sirf `find(lit_val) != npos` karde si, yaani aam substring match.
// (We used to do a plain `find(lit_val) != npos`, i.e. an ordinary substring match.)
// Isliye `LIKE 'Sup%'` te `LIKE 'Sup'` dono same si, te `LIKE '%man'` galat si.
// (So `LIKE 'Sup%'` and `LIKE 'Sup'` behaved identically, and `LIKE '%man'` was wrong.)
//
// Rule 3: iss utility vich koi domain knowledge nahi - e pure text di dafa da function aa.
// (Rule 3: no domain knowledge here - this is a pure text function.)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Sql Like Match - Text pattern naal match hunda aa?
// (Does the text match the pattern?)
// ----------------------------------------------------------------------------
// `%`    -> kitne vi characters, inclu zero (any number of characters, including zero)
// `_`    -> ek hi character (exactly one character)
//
// Puri string match honi chahidi - `LIKE 'Super'` te "Superman" match nahi hunda.
// (The WHOLE string must match - `LIKE 'Super'` does not match "Superman".)
//
// ASCII characters lai case nahi pawaanda - `LIKE 'sup%'` te "Superman" match hunda.
// (Case-insensitive for ASCII - `LIKE 'sup%'` matches "Superman".)
// Non-ASCII bytes (UTF-8 continuation bytes) waise hi compare hoye ne.
// (Non-ASCII bytes are compared as-is.)
//
// SQLite jive hi, koi escape character support nahi - `%` te `_` hamesha wildcard ne.
// (As in SQLite there is no escape character: `%` and `_` are always wildcards.)
bool sqlLikeMatch(const std::string& text, const std::string& pattern);

} // namespace sqlite
