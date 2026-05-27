#pragma once

#include "types/query.hpp"
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
//
// Eh file sirf TEXT nu SQL tokens vich tori hai. Oh data shapes (CompareOp,
// WhereCondition, SelectQuery, ...) `types/query.hpp` vich ne.
// (This file only turns text into SQL tokens. The data shapes live in
// `types/query.hpp`.)
// ============================================================================

namespace sqlite {

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
