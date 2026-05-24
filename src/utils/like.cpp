#include "like.hpp"
#include <cstddef>

// ============================================================================
// UTILS/LIKE.CPP - SQL LIKE Pattern Matching Implementation
// ============================================================================

namespace sqlite {

namespace {

// ASCII da lower case - sirf 'A'-'Z' nu hi badalde aa.
// (ASCII lowercase - only 'A'-'Z' are folded.)
// std::tolower locale te depend karda aa (Turkish I wagera), isliye khud likhya.
// (std::tolower depends on the locale - Turkish I and such - so this is hand-rolled.)
char asciiFold(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool foldEqual(char a, char b) {
    return asciiFold(a) == asciiFold(b);
}

// Ek UTF-8 character kitne bytes de hunda - lead byte ton pawaanda.
// (How many bytes one UTF-8 character takes - from its lead byte.)
// SQLite da LIKE character-wich chalda, byte-wich nahi: `'_' LIKE 'é'` match hunda
// 'é' 2 bytes da hunda te vi. (SQLite's LIKE walks characters, not bytes:
// `'é' LIKE '_'` matches even though é is two bytes.)
size_t utf8CharLength(unsigned char lead) {
    if (lead < 0x80) return 1;        // ASCII
    if (lead < 0xC0) return 1;        // continuation byte - corrupt, ek maan lo
    if (lead < 0xE0) return 2;
    if (lead < 0xF0) return 3;
    if (lead < 0xF8) return 4;
    return 1;                         // invalid lead byte
}

} // namespace

// ----------------------------------------------------------------------------
// Sql Like Match - Do pointers te backtracking, O(n*m) worst case, O(1) space
// (Two pointers plus backtracking: O(n*m) worst case, O(1) space)
// ----------------------------------------------------------------------------
// `star`  - aaj hi `%` dekha si te usda pattern index (where the last `%` was)
// `resume` - oh text position jithon `%` ke badle aage dubara try karna aa
// (the text position to retry from after letting the `%` swallow one more char)
bool sqlLikeMatch(const std::string& text, const std::string& pattern) {
    constexpr size_t NO_STAR = static_cast<size_t>(-1);

    size_t t = 0;        // text vich kithon (position in text)
    size_t p = 0;        // pattern vich kithon (position in pattern)
    size_t star = NO_STAR;
    size_t resume = 0;

    while (t < text.size()) {
        // ORDER MATTERS: wildcards pehlan check karo, normal character baad vich.
        // (ORDER MATTERS: check the wildcards first, the ordinary character last.)
        // Agar normal character pehlan check kariye te pattern vich da literal '%'
        // text de '%' naal match ho ke wildcard wala '%' miss ho janda.
        // (If the ordinary character were checked first, a literal '%' in the
        // pattern would match a '%' in the text and the wildcard would be missed.)
        if (p < pattern.size() && pattern[p] == '%') {
            // `%` mil gaya - pehlan e nu khali chhad de (zero characters), te
            // jado match fail hove te ik hor character khaane lai yaad rakh de
            // (Found a `%`: first try it matching nothing, and remember to come
            // back and let it swallow one more character if we get stuck)
            star = p++;
            resume = t;
        } else if (p < pattern.size() && pattern[p] == '_') {
            // `_` - ek hi character khaa do, kuch bhi sahi.
            // (`:_`: consume exactly one character, whatever it is.)
            // Character, byte nahi - multi-byte UTF-8 da poora character khaanda aa.
            // (A character, not a byte: a whole multi-byte UTF-8 character is consumed.)
            ++p;
            t += utf8CharLength(static_cast<unsigned char>(text[t]));
        } else if (p < pattern.size() && foldEqual(pattern[p], text[t])) {
            // Normal character - case-insensitive compare karo
            // (Ordinary character - compared case-insensitively)
            ++p;
            ++t;
        } else if (star != NO_STAR) {
            // Pichla `%` ik hor character kha le
            // (Backtrack: let the last `%` consume one more character)
            p = star + 1;
            t = ++resume;
        } else {
            // Koi `%` bhi nahi si te match nahi hona - koi wild card nahi bacha
            // (No `%` to fall back on, so this cannot match)
            return false;
        }
    }

    // Text khatam. Pattern vich jo `%` bache ne oh sab khali ho sakde ne.
    // (Text exhausted. Any `%` left over in the pattern can still match empty.)
    while (p < pattern.size() && pattern[p] == '%') {
        ++p;
    }

    return p == pattern.size();
}

} // namespace sqlite
