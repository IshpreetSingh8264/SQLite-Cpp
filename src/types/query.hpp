#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// ============================================================================
// TYPES/QUERY.HPP - Query Data Contracts
// ============================================================================
// Eh file koi logic nahi rakundi - sirf data shapes, ke de de ki query kidi hai.
// (This file holds no logic - only data shapes describing what a query is.)
//
// Plan jive: TS project vich `types/types.ts` si, C++ vich headers banti hain.
// (Following the plan: in the TS project this was `types/types.ts`; in C++ it is
// headers.)
// QueryExecutor te ValueCompare dono ehi types use karde aa, isliye dono nu
// alag-alag rakhna te badhana parta.
// (Both QueryExecutor and ValueCompare need these, which is exactly why they live
// in a shared `types/` layer rather than inside the parser.)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Column Value - Ek column di value, kisi vi storage class di ho sakdi
// (A column value, which can be of any storage class)
// ----------------------------------------------------------------------------
// Assi koi bhi variant nahi - yahi 5 combinations SQLite de storage classes di ne.
// (Not an arbitrary variant: these five are exactly SQLite's storage classes.)
//   monostate           -> NULL
//   int64_t             -> INTEGER
//   double              -> REAL
//   std::string         -> TEXT
//   std::vector<uint8_t> -> BLOB
using ColumnValue = std::variant<
    std::monostate,      // NULL
    int64_t,             // INTEGER
    double,              // REAL
    std::string,         // TEXT
    std::vector<uint8_t> // BLOB
>;

// ----------------------------------------------------------------------------
// Storage Class - Value di class, comparison de hisaab naal rank di jandi hai
// (The class of a value; the rank is what comparisons order by)
// ----------------------------------------------------------------------------
enum class StorageClass {
    Null = 0,    // NULL - sab toh chhoti (smallest)
    Number = 1,  // INTEGER te REAL dono (INTEGER and REAL share one rank)
    Text = 2,    // TEXT
    Blob = 3     // BLOB - sab toh waddi (largest)
};

// ----------------------------------------------------------------------------
// Comparison Operators - WHERE clause vich comparisons
// (Comparisons in WHERE clause)
// ----------------------------------------------------------------------------
enum class CompareOp {
    EQUAL,              // = - equal aa (is equal)
    NOT_EQUAL,          // != or <> - equal nahi (not equal)
    LESS_THAN,          // < - chhota aa (is less than)
    LESS_EQUAL,         // <= - chhota ya equal (less than or equal)
    GREATER_THAN,       // > - wadda aa (is greater than)
    GREATER_EQUAL,      // >= - wadda ya equal (greater than or equal)
    LIKE,               // LIKE - pattern match (pattern match)
    UNKNOWN             // Unknown operator - samajh nahi aaya (didn't understand)
};

// ----------------------------------------------------------------------------
// Literal Value - WHERE clause vich values (numbers, strings)
// (Values in WHERE clause (numbers, strings))
// ----------------------------------------------------------------------------
using LiteralValue = std::variant<
    int64_t,        // Integer value - number (number)
    double,         // Float value - decimal (decimal)
    std::string,    // String value - text (text)
    bool            // Boolean - true/false (true/false)
>;

// ----------------------------------------------------------------------------
// WHERE Condition - Ek condition represent kardi
// (Represents one condition)
// ----------------------------------------------------------------------------
// Example: age > 18
//   column = "age"
//   op = GREATER_THAN
//   value = 18
struct WhereCondition {
    std::string column_name;    // Column naam - kis column te check (column name - check on which column)
    CompareOp op;               // Comparison operator
    LiteralValue value;         // Compare karne wali value (value to compare with)
    
    // Constructor
    WhereCondition() : op(CompareOp::UNKNOWN) {}
};

// ----------------------------------------------------------------------------
// SELECT Query - Puri SELECT query di information
// (Complete SELECT query information)
// ----------------------------------------------------------------------------
struct SelectQuery {
    std::vector<std::string> columns;       // Kine columns select karne (which columns to select)
    bool select_all;                        // * hai ya nahi - saare columns (is * or not - all columns)
    std::string table_name;                 // Kis table vichon (from which table)
    std::optional<WhereCondition> where;    // WHERE clause - filter condition (filter condition)
    std::optional<std::string> order_by_column; // ORDER BY column (ORDER BY column)
    bool order_desc;                        // Descending order - ulta order (reverse order)
    std::optional<int64_t> limit;           // LIMIT - kitne rows (how many rows)
    
    // Constructor - Default values set karo
    // (Set default values)
    SelectQuery() 
        : select_all(false), order_desc(false) {}
};

} // namespace sqlite
