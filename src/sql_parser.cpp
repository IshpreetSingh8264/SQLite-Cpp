#include "sql_parser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

// ============================================================================
// SQL_PARSER.CPP - SQL Query Parser Implementation
// ============================================================================
// Oye sql_parser.cpp vich assi SQL queries nu parse karde aa!
// (Hey in sql_parser.cpp we parse SQL queries!)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Parse SELECT - SELECT query parse karo
// (Parse SELECT query)
// ----------------------------------------------------------------------------
// Example: SELECT name, age FROM users WHERE age > 18
std::optional<SelectQuery> SqlParser::parseSelect(const std::string& sql) {
    error_message_.clear();
    
    // Tokens vich split kar - space, comma te
    // (Split into tokens - on space, comma)
    auto tokens = tokenize(sql);
    
    if (tokens.empty() || normalize(tokens[0]) != "select") {
        // SELECT nahi aa - galat query
        // (Not SELECT - wrong query)
        error_message_ = "Query should start with SELECT yaar!";
        // (Query should start with SELECT dude!)
        return std::nullopt;
    }
    
    SelectQuery query;
    size_t index = 1;  // SELECT ke baad ton shuru (start after SELECT)
    
    // Column list parse karo - SELECT te FROM ke beech
    // (Parse column list - between SELECT and FROM)
    query.columns = parseColumnList(tokens, index);
    
    if (query.columns.size() == 1 && query.columns[0] == "*") {
        // * aa - saare columns select karo
        // (* present - select all columns)
        query.select_all = true;
        query.columns.clear();
    }
    
    // FROM keyword check karo
    // (Check FROM keyword)
    if (index >= tokens.size() || normalize(tokens[index]) != "from") {
        error_message_ = "FROM keyword missing hai bai!";
        // (FROM keyword is missing buddy!)
        return std::nullopt;
    }
    index++;  // FROM skip kar (skip FROM)
    
    // Table naam kadho
    // (Get table name)
    if (index >= tokens.size()) {
        error_message_ = "Table naam nahi mileya!";
        // (Table name not found!)
        return std::nullopt;
    }
    query.table_name = tokens[index++];
    
    // WHERE clause check karo - optional
    // (Check WHERE clause - optional)
    if (index < tokens.size() && normalize(tokens[index]) == "where") {
        index++;  // WHERE skip
        query.where = parseWhere(tokens, index);
        
        if (!query.where) {
            // WHERE parsing fail hoyi
            // (WHERE parsing failed)
            return std::nullopt;
        }
    }
    
    // ORDER BY clause - future enhancement
    // (ORDER BY clause - future enhancement)
    if (index < tokens.size() && normalize(tokens[index]) == "order") {
        index++;
        if (index < tokens.size() && normalize(tokens[index]) == "by") {
            index++;
            if (index < tokens.size()) {
                query.order_by_column = tokens[index++];
                
                // DESC check karo
                // (Check for DESC)
                if (index < tokens.size() && normalize(tokens[index]) == "desc") {
                    query.order_desc = true;
                    index++;
                }
            }
        }
    }
    
    return query;
}

// ----------------------------------------------------------------------------
// Tokenize - SQL nu tokens vich split karo
// (Split SQL into tokens)
// ----------------------------------------------------------------------------
std::vector<std::string> SqlParser::tokenize(const std::string& sql) {
    std::vector<std::string> tokens;
    std::string current_token;
    bool in_string = false;
    char string_delimiter = '\0';
    
    for (size_t i = 0; i < sql.length(); ++i) {
        char c = sql[i];
        
        // String handling - quotes de andar
        // (String handling - inside quotes)
        if (c == '\'' || c == '"') {
            if (!in_string) {
                // String shuru hoyi
                // (String started)
                in_string = true;
                string_delimiter = c;
                current_token += c;
            } else if (c == string_delimiter) {
                // String khatam hoyi
                // (String ended)
                in_string = false;
                current_token += c;
                string_delimiter = '\0';
            } else {
                current_token += c;
            }
            continue;
        }
        
        if (in_string) {
            // String vich aa - saara kuch add kar
            // (Inside string - add everything)
            current_token += c;
            continue;
        }
        
        // Separators - space, comma, parentheses, operators
        if (std::isspace(c) || c == ',' || c == '(' || c == ')' || c == '=' || c == '<' || c == '>' || c == '!') {
            // Current token save kar
            // (Save current token)
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
            
            // Operator tokens - >, <, =, !=, <=, >=
            if (c == '=' || c == '<' || c == '>' || c == '!') {
                std::string op(1, c);
                
                // Next character check kar - composite operator ho sakda
                // (Check next character - might be composite operator)
                if (i + 1 < sql.length()) {
                    char next = sql[i + 1];
                    if ((c == '<' && next == '=') || 
                        (c == '>' && next == '=') ||
                        (c == '<' && next == '>') ||
                        (c == '!' && next == '=')) {
                        op += next;
                        i++;  // Extra character skip kar (skip extra character)
                    }
                }
                
                tokens.push_back(op);
            } else if (c == ',') {
                tokens.push_back(",");
            }
            
            // Space ignore kar (ignore space)
        } else {
            current_token += c;
        }
    }
    
    // Last token add kar
    // (Add last token)
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }
    
    return tokens;
}

// ----------------------------------------------------------------------------
// Parse Column List - Column names parse karo
// (Parse column names)
// ----------------------------------------------------------------------------
std::vector<std::string> SqlParser::parseColumnList(const std::vector<std::string>& tokens, size_t& index) {
    std::vector<std::string> columns;
    
    // FROM tak columns padhde raho
    // (Keep reading columns till FROM)
    while (index < tokens.size()) {
        std::string token = normalize(tokens[index]);
        
        if (token == "from") {
            // FROM aa gaya - column list khatam
            // (FROM arrived - column list ended)
            break;
        }
        
        if (token == ",") {
            // Comma skip kar - agle column te jao
            // (Skip comma - go to next column)
            index++;
            continue;
        }
        
        // Column naam add kar
        // (Add column name)
        columns.push_back(tokens[index]);
        index++;
    }
    
    return columns;
}

// ----------------------------------------------------------------------------
// Parse WHERE - WHERE clause parse karo
// (Parse WHERE clause)
// ----------------------------------------------------------------------------
// Example: WHERE age > 18
//          WHERE name = 'John'
std::optional<WhereCondition> SqlParser::parseWhere(const std::vector<std::string>& tokens, size_t& index) {
    WhereCondition condition;
    
    // Column naam kadho
    // (Get column name)
    if (index >= tokens.size()) {
        error_message_ = "WHERE clause incomplete hai!";
        // (WHERE clause is incomplete!)
        return std::nullopt;
    }
    condition.column_name = tokens[index++];
    
    // Operator kadho - =, !=, <, >, <=, >=
    // (Get operator)
    if (index >= tokens.size()) {
        error_message_ = "WHERE vich operator missing!";
        // (Operator missing in WHERE!)
        return std::nullopt;
    }
    condition.op = parseCompareOp(tokens[index++]);
    
    if (condition.op == CompareOp::UNKNOWN) {
        error_message_ = "Invalid comparison operator!";
        return std::nullopt;
    }
    
    // Value kadho - number, string, etc
    // (Get value - number, string, etc)
    if (index >= tokens.size()) {
        error_message_ = "WHERE vich value missing!";
        // (Value missing in WHERE!)
        return std::nullopt;
    }
    condition.value = parseLiteral(tokens[index++]);
    
    return condition;
}

// ----------------------------------------------------------------------------
// Parse Compare Op - Operator string ton enum
// (Enum from operator string)
// ----------------------------------------------------------------------------
CompareOp SqlParser::parseCompareOp(const std::string& op) {
    if (op == "=") return CompareOp::EQUAL;
    if (op == "!=" || op == "<>") return CompareOp::NOT_EQUAL;
    if (op == "<") return CompareOp::LESS_THAN;
    if (op == "<=") return CompareOp::LESS_EQUAL;
    if (op == ">") return CompareOp::GREATER_THAN;
    if (op == ">=") return CompareOp::GREATER_EQUAL;
    
    std::string lower = op;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "like") return CompareOp::LIKE;
    
    return CompareOp::UNKNOWN;
}

// ----------------------------------------------------------------------------
// Parse Literal - Value parse karo - number, string, boolean
// (Parse value - number, string, boolean)
// ----------------------------------------------------------------------------
LiteralValue SqlParser::parseLiteral(const std::string& token) {
    // Quotes check karo - string aa
    // (Check for quotes - it's string)
    if (!token.empty() && (token.front() == '\'' || token.front() == '"')) {
        return removeQuotes(token);
    }
    
    // Number check karo - integer ya float
    // (Check for number - integer or float)
    bool has_dot = false;
    bool is_number = true;
    
    for (size_t i = 0; i < token.length(); ++i) {
        char c = token[i];
        
        if (c == '.') {
            if (has_dot) {
                // Do dots - number nahi
                // (Two dots - not number)
                is_number = false;
                break;
            }
            has_dot = true;
        } else if (c == '-' || c == '+') {
            // Sign pehle hi ho sakda
            // (Sign can only be first)
            if (i != 0) {
                is_number = false;
                break;
            }
        } else if (!std::isdigit(c)) {
            is_number = false;
            break;
        }
    }
    
    if (is_number) {
        if (has_dot) {
            // Float aa (it's float)
            return std::stod(token);
        } else {
            // Integer aa (it's integer)
            return std::stoll(token);
        }
    }
    
    // Boolean check - true/false
    std::string lower = normalize(token);
    if (lower == "true") return true;
    if (lower == "false") return false;
    
    // Baaki sab string maan lo
    // (Consider everything else as string)
    return token;
}

// ----------------------------------------------------------------------------
// Helper Functions - String manipulation
// (String manipulation)
// ----------------------------------------------------------------------------

std::string SqlParser::normalize(const std::string& str) {
    return toLower(trim(str));
}

std::string SqlParser::removeQuotes(const std::string& str) {
    if (str.length() < 2) return str;
    
    if ((str.front() == '\'' && str.back() == '\'') ||
        (str.front() == '"' && str.back() == '"')) {
        return str.substr(1, str.length() - 2);
    }
    
    return str;
}

// ----------------------------------------------------------------------------
// Global Helper Functions
// ----------------------------------------------------------------------------

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

} // namespace sqlite
