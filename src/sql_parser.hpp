#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>

// ============================================================================
// SQL_PARSER.HPP - SQL Query Parser
// ============================================================================
// Oye sql_parser.hpp vich assi SQL queries parse karde aa!
// (Hey in sql_parser.hpp we parse SQL queries!)
//
// SELECT name, age FROM users WHERE age > 18
// Eh query nu break karke samajh lade - kina naam, ki table, ki condition
// (Break this query and understand - column names, which table, which condition)
// ============================================================================

namespace sqlite {

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

// ----------------------------------------------------------------------------
// SQL Parser Class - SQL queries parse karda
// (Parses SQL queries)
// ----------------------------------------------------------------------------
class SqlParser {
public:
    // Constructor
    SqlParser() = default;
    
    // SELECT query parse karo
    // (Parse SELECT query)
    // Returns parsed query agar success, nullopt agar fail
    // (Returns parsed query if success, nullopt if fail)
    std::optional<SelectQuery> parseSelect(const std::string& sql);
    
    // Error message kadho agar parsing fail hoyi
    // (Get error message if parsing failed)
    const std::string& getError() const { return error_message_; }
    
private:
    // Helper methods - Andar de kaam (Internal work)
    
    // SQL nu tokens vich break karo - space, comma te split
    // (Break SQL into tokens - split on space, comma)
    std::vector<std::string> tokenize(const std::string& sql);
    
    // Column list parse karo - SELECT te FROM de beech
    // (Parse column list - between SELECT and FROM)
    std::vector<std::string> parseColumnList(const std::vector<std::string>& tokens, size_t& index);
    
    // WHERE clause parse karo
    // (Parse WHERE clause)
    std::optional<WhereCondition> parseWhere(const std::vector<std::string>& tokens, size_t& index);
    
    // Comparison operator string vichon enum vich convert karo
    // (Convert comparison operator from string to enum)
    static CompareOp parseCompareOp(const std::string& op);
    
    // Literal value parse karo - number, string ya boolean
    // (Parse literal value - number, string or boolean)
    static LiteralValue parseLiteral(const std::string& token);
    
    // String normalize karo - lowercase te trim
    // (Normalize string - lowercase and trim)
    static std::string normalize(const std::string& str);
    
    // Quotes remove karo string vichon
    // (Remove quotes from string)
    static std::string removeQuotes(const std::string& str);
    
    // Member variables
    std::string error_message_;     // Error message agar parsing fail (error message if parsing fails)
};

// ----------------------------------------------------------------------------
// Helper Functions - Chhote chhote kaam lai (For small tasks)
// ----------------------------------------------------------------------------

// String nu lowercase vich convert karo
// (Convert string to lowercase)
std::string toLower(const std::string& str);

// String trim karo - aage pichhe di spaces hatao
// (Trim string - remove leading/trailing spaces)
std::string trim(const std::string& str);

// String split karo delimiter te
// (Split string on delimiter)
std::vector<std::string> split(const std::string& str, char delimiter);

} // namespace sqlite
