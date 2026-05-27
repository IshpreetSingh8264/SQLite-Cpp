#include "value_compare.hpp"
#include "like.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

// ============================================================================
// UTILS/VALUE_COMPARE.CPP - SQL Comparison With Affinity Rules Implementation
// ============================================================================

namespace sqlite {

namespace {

// Value di storage class - comparison vich rank wada
// (A value's storage class - what its rank in comparisons is)
StorageClass storageClassOf(const ColumnValue& value) {
    if (std::holds_alternative<std::monostate>(value)) return StorageClass::Null;
    if (std::holds_alternative<int64_t>(value))    return StorageClass::Number;
    if (std::holds_alternative<double>(value))     return StorageClass::Number;
    if (std::holds_alternative<std::string>(value)) return StorageClass::Text;
    return StorageClass::Blob;
}

// NUMBER rank vich dono classes ik hi rank de - INTEGER te REAL nu
// separately rank dena galat hunda.
// (INTEGER and REAL share the NUMBER rank; ranking them separately is wrong.)
bool isNumberClass(StorageClass cls) {
    return cls == StorageClass::Number;
}

// Literal nu ColumnValue vich badlo - dono jiyon hi storage class de variants ne
// (Turn a literal into a ColumnValue: they are both storage-class variants)
ColumnValue literalToColumnValue(const LiteralValue& literal) {
    if (std::holds_alternative<int64_t>(literal)) {
        return std::get<int64_t>(literal);
    }
    if (std::holds_alternative<double>(literal)) {
        return std::get<double>(literal);
    }
    if (std::holds_alternative<std::string>(literal)) {
        return std::get<std::string>(literal);
    }
    if (std::holds_alternative<bool>(literal)) {
        // SQLite bool nu 1/0 maan lenda aa
        // (SQLite treats a boolean as 1/0)
        return static_cast<int64_t>(std::get<bool>(literal) ? 1 : 0);
    }
    return std::monostate{};
}

// Value nu text bana do - LIKE te TEXT affinity dono lai chahida
// (Render a value as text - needed by LIKE and by TEXT affinity)
std::string valueToText(const ColumnValue& value) {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    }
    if (std::holds_alternative<double>(value)) {
        return realToText(std::get<double>(value));
    }
    if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        return std::string(reinterpret_cast<const char*>(std::get<std::vector<uint8_t>>(value).data()),
                           std::get<std::vector<uint8_t>>(value).size());
    }
    return std::string();
}

// Kya e text ek well-formed number aa? (Is this text a well-formed number?)
// Aage pichhe di space chalti hai, sign te decimal point te exponent de mildi hai.
// (Leading and trailing spaces are allowed, as are a sign, a decimal point and
// an exponent.)
bool isWellFormedNumber(const std::string& text, double& number) {
    if (text.empty()) {
        return false;
    }

    const char* start = text.c_str();
    char* end = nullptr;
    errno = 0;
    number = std::strtod(start, &end);

    if (end == start) {
        return false;  // Koi number hi parse nahi hoya (no number parsed at all)
    }

    // "nan" / "inf" jive - SQLite ohde nu 0.0 banayda aa
    // ("nan" / "inf" and the like: SQLite turns those into 0.0)
    if (!std::isfinite(number)) {
        number = 0.0;
    }

    // Sirf spaces bachde hone chahide
    // (Only spaces may remain)
    while (*end != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end))) {
            return false;
        }
        ++end;
    }

    return true;
}

// NUMERIC affinity lai TEXT value - number hoye te number bana do
// (Apply NUMERIC affinity to a TEXT value: turn it into a number if it is one)
void applyNumericAffinity(ColumnValue& value) {
    if (!std::holds_alternative<std::string>(value)) {
        return;  // Already a number or BLOB - kuch nahi karna (nothing to do)
    }

    double number = 0.0;
    if (!isWellFormedNumber(std::get<std::string>(value), number)) {
        return;  // Number nahi hai - text hi rehne de (not a number: leave it TEXT)
    }

    // Poori value number da integer represent hoyi te INTEGER bana do
    // (If the whole value is representable as an integer, make it INTEGER)
    if (number >= -9223372036854775808.0 && number < 9223372036854775808.0 &&
        number == static_cast<double>(static_cast<int64_t>(number))) {
        value = static_cast<int64_t>(number);
    } else {
        value = number;
    }
}

// TEXT affinity lai number - text bana do
// (Apply TEXT affinity to a number: render it as text)
void applyTextAffinity(ColumnValue& value) {
    if (isNumberClass(storageClassOf(value))) {
        value = valueToText(value);
    }
}

// Do numbers nu compare karo - REAL nu INTEGER naal barabar nachaunda aa
// (Compare two numbers: REAL compares equal to the matching INTEGER)
int compareNumbers(const ColumnValue& left, const ColumnValue& right) {
    const bool left_is_int = std::holds_alternative<int64_t>(left);
    const bool right_is_int = std::holds_alternative<int64_t>(right);

    if (left_is_int && right_is_int) {
        const int64_t l = std::get<int64_t>(left);
        const int64_t r = std::get<int64_t>(right);
        return (l < r) ? -1 : (l > r ? 1 : 0);
    }

    const double l = left_is_int ? static_cast<double>(std::get<int64_t>(left))
                                 : std::get<double>(left);
    const double r = right_is_int ? static_cast<double>(std::get<int64_t>(right))
                                 : std::get<double>(right);
    return (l < r) ? -1 : (l > r ? 1 : 0);
}

} // namespace

// ----------------------------------------------------------------------------
// Real To Text - SQLite jive REAL nu text bana do
// ----------------------------------------------------------------------------
std::string realToText(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.15g", value);
    std::string text(buffer);

    // Koi decimal point ya exponent na ho te ".0" jod de - "5" nahi, "5.0"
    // (If there is no decimal point or exponent, append ".0" - "5.0", not "5")
    if (text.find_first_of(".eEnN") == std::string::npos) {
        text += ".0";
    }

    return text;
}

// ----------------------------------------------------------------------------
// Column Affinity - Declared type vichon affinity niklo
// (Derive the affinity from a declared type)
// ----------------------------------------------------------------------------
// Rules theek order vich follow hoye aa - "INT" pehlan check hona chahida, karan
// "POINT" jive types "INT" vachde aa, te "CHAR" "VARCHAR" dono lai pawaanda.
// (The rules must be checked in this order: "INT" first, because types such as
// "POINT" contain "INT", and "CHAR" also covers "VARCHAR".)
Affinity columnAffinity(const std::string& declared_type) {
    std::string upper = declared_type;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upper.find("INT") != std::string::npos) {
        return Affinity::Integer;
    }
    if (upper.find("CHAR") != std::string::npos || upper.find("CLOB") != std::string::npos ||
        upper.find("TEXT") != std::string::npos) {
        return Affinity::Text;
    }
    if (upper.empty() || upper.find("BLOB") != std::string::npos) {
        return Affinity::Blob;
    }
    if (upper.find("REAL") != std::string::npos || upper.find("FLOA") != std::string::npos ||
        upper.find("DOUB") != std::string::npos) {
        return Affinity::Real;
    }

    return Affinity::Numeric;
}

// ----------------------------------------------------------------------------
// Is Numeric Affinity - Numeric family vich aa?
// ----------------------------------------------------------------------------
bool isNumericAffinity(Affinity affinity) {
    return affinity == Affinity::Integer || affinity == Affinity::Real ||
           affinity == Affinity::Numeric;
}

// ----------------------------------------------------------------------------
// Compare Column Values - Storage class di rank te rank da comparison
// ----------------------------------------------------------------------------
bool compareColumnValues(const ColumnValue& left, const ColumnValue& right, CompareOp op) {
    if (op == CompareOp::UNKNOWN || op == CompareOp::LIKE) {
        return false;  // LIKE alag treatment lainda - dekhyo valueMatches
    }

    const StorageClass left_class = storageClassOf(left);
    const StorageClass right_class = storageClassOf(right);

    // NULL kisi operator naal match nahi hunda
    // (NULL never matches under any operator)
    if (left_class == StorageClass::Null || right_class == StorageClass::Null) {
        return false;
    }

    int order = 0;  // -1 left chhota, 0 barabar, 1 left wadda
    if (isNumberClass(left_class) && isNumberClass(right_class)) {
        order = compareNumbers(left, right);
    } else if (left_class != right_class) {
        // Class alag - rank hi order aa
        // (Different classes: the rank alone is the order)
        const int left_rank = static_cast<int>(left_class);
        const int right_rank = static_cast<int>(right_class);
        order = (left_rank < right_rank) ? -1 : 1;
    } else if (left_class == StorageClass::Text) {
        const std::string& l = std::get<std::string>(left);
        const std::string& r = std::get<std::string>(right);
        const int cmp = l.compare(r);
        order = (cmp < 0) ? -1 : (cmp > 0 ? 1 : 0);
    } else {
        // BLOB - byte te byte compare karo
        const std::vector<uint8_t>& l = std::get<std::vector<uint8_t>>(left);
        const std::vector<uint8_t>& r = std::get<std::vector<uint8_t>>(right);
        const int cmp = std::lexicographical_compare(l.begin(), l.end(), r.begin(), r.end());
        if (cmp) {
            order = -1;
        } else if (std::lexicographical_compare(r.begin(), r.end(), l.begin(), l.end())) {
            order = 1;
        } else {
            order = 0;
        }
    }

    switch (op) {
        case CompareOp::EQUAL:        return order == 0;
        case CompareOp::NOT_EQUAL:    return order != 0;
        case CompareOp::LESS_THAN:    return order < 0;
        case CompareOp::LESS_EQUAL:   return order <= 0;
        case CompareOp::GREATER_THAN: return order > 0;
        case CompareOp::GREATER_EQUAL:return order >= 0;
        default:                      return false;
    }
}

// ----------------------------------------------------------------------------
// Value Matches - Pehlan affinity apply karo, phir compare karo
// (Apply affinity first, then compare)
// ----------------------------------------------------------------------------
bool valueMatches(const ColumnValue& record_value, Affinity column_affinity,
                  CompareOp op, const LiteralValue& literal) {
    // NULL kisi naal match nahi hunda - LIKE de pawaandal
    // (NULL matches nothing - that holds for LIKE too)
    if (std::holds_alternative<std::monostate>(record_value)) {
        return false;
    }

    // LIKE alag de - dono operands TEXT bana ke match hoya hunda, column de
    // affinity nu koi matlab nahi padda.
    // (LIKE is separate: both operands become TEXT and are matched. The column's
    // affinity plays no part.)
    if (op == CompareOp::LIKE) {
        return sqlLikeMatch(valueToText(record_value), valueToText(literalToColumnValue(literal)));
    }

    // Rules 1 te 2, bilkul SQLite vicham - literal de apni koi affinity nahi hundi,
    // isliye sirf column di affinity hi drive kardi hai.
    // (Rules 1 and 2, exactly as in SQLite. A literal carries no affinity of its
    // own, so only the column's affinity drives the conversion.)
    ColumnValue other = literalToColumnValue(literal);

    if (isNumericAffinity(column_affinity) &&
        !isNumberClass(storageClassOf(other))) {
        // Rule 1 - column numeric, literal TEXT ya BLOB: literal nu NUMERIC bana do
        // (Rule 1 - numeric column, TEXT or BLOB literal: apply NUMERIC)
        applyNumericAffinity(other);
    } else if (column_affinity == Affinity::Text &&
               isNumberClass(storageClassOf(other))) {
        // Rule 2 - column TEXT, literal number: literal nu TEXT bana do
        // (Rule 2 - TEXT column, numeric literal: apply TEXT)
        applyTextAffinity(other);
    }

    return compareColumnValues(record_value, other, op);
}

} // namespace sqlite
