#include "query_executor.hpp"
#include "utils/like.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

// ============================================================================
// QUERY_EXECUTOR.CPP - SQL Query Executor Implementation
// ============================================================================
// Oye query_executor.cpp vich assi queries execute karde aa!
// (Hey in query_executor.cpp we execute queries!)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Constructor - Executor setup karo
// (Setup executor)
// ----------------------------------------------------------------------------
QueryExecutor::QueryExecutor(Database& database, Schema& schema)
    : database_(database)
    , schema_(schema)
{
}

// ----------------------------------------------------------------------------
// Execute - SELECT query execute karo
// (Execute SELECT query)
// ----------------------------------------------------------------------------
QueryResult QueryExecutor::execute(const SelectQuery& query) {
    // Table definition kadho schema vichon
    // (Get table definition from schema)
    const TableDefinition* table_def = schema_.getTableDefinition(query.table_name);
    
    if (!table_def) {
        // Table nahi mili - error!
        // (Table not found - error!)
        throw std::runtime_error("Table not found yaar: " + query.table_name);
        // (Table not found dude)
    }
    
    // Index use kar sakde ya nahi check karo
    // (Check if we can use index)
    const IndexDefinition* index_def = nullptr;
    
    if (query.where) {
        // WHERE clause aa - index dhundho
        // (WHERE clause present - find index)
        index_def = findUsableIndex(query, table_def);
    }
    
    if (index_def) {
        // Index use kar - jaldi hoyi
        // (Use index - will be fast)
        return indexScan(query, table_def, index_def);
    } else {
        // Full table scan - sab kuch padh
        // (Full table scan - read everything)
        return fullTableScan(query, table_def);
    }
}

// ----------------------------------------------------------------------------
// Count - Table vich kitne rows ne
// (How many rows in table)
// ----------------------------------------------------------------------------
size_t QueryExecutor::count(const std::string& table_name) {
    const TableDefinition* table_def = schema_.getTableDefinition(table_name);
    
    if (!table_def) {
        throw std::runtime_error("Table not found: " + table_name);
    }
    
    // B-tree scan karke count kar
    // (Scan B-tree and count)
    BTree tree(database_, table_def->root_page);
    return tree.countRecords();
}

// ----------------------------------------------------------------------------
// Full Table Scan - Puri table scan kar te filter kar
// (Scan entire table and filter)
// ----------------------------------------------------------------------------
QueryResult QueryExecutor::fullTableScan(const SelectQuery& query, const TableDefinition* table_def) {
    QueryResult result;
    
    // Column names set kar
    // (Set column names)
    if (query.select_all || query.columns.empty()) {
        // Saare columns select (select all columns)
        for (const auto& col : table_def->columns) {
            result.column_names.push_back(col.name);
        }
    } else {
        result.column_names = query.columns;
    }
    
    // Check karo ki pehla column INTEGER PRIMARY KEY aa
    // (Check if first column is INTEGER PRIMARY KEY)
    bool has_rowid_column = false;
    if (!table_def->columns.empty() && table_def->columns[0].is_primary_key) {
        // Pehla column primary key aa - rowid use hunda
        // (First column is primary key - uses rowid)
        has_rowid_column = true;
    }
    
    // B-tree scan karo
    // (Scan B-tree)
    BTree tree(database_, table_def->root_page);
    
    tree.scanAll([&](const Record& record) {
        // WHERE condition check karo agar aa
        // (Check WHERE condition if present)
        if (query.where) {
            if (!evaluateWhere(record, *query.where, table_def)) {
                // Condition match nahi hoyi - skip kar
                // (Condition didn't match - skip)
                return;
            }
        }
        
        // Columns extract kar record vichon
        // (Extract columns from record)
        std::vector<std::string> row;
        
        if (query.select_all || query.columns.empty()) {
            // Saare columns (all columns)
            if (has_rowid_column) {
                // Pehla column table definition vich rowid aa
                // (First column in table definition is rowid)
                row.push_back(std::to_string(record.getRowId()));
                // Record pehle column vich NULL placeholder aa, skip kar
                // (Record first column has NULL placeholder, skip it)
                // Baaki columns (1 onwards) output kar
                // (Output remaining columns (1 onwards))
                for (size_t i = 1; i < record.getColumnCount(); ++i) {
                    row.push_back(columnValueToString(record.getColumnValue(i)));
                }
            } else {
                // Koi rowid column nahi - saare record columns output kar
                // (No rowid column - output all record columns)
                for (size_t i = 0; i < record.getColumnCount(); ++i) {
                    row.push_back(columnValueToString(record.getColumnValue(i)));
                }
            }
        } else {
            // Specific columns
            row = extractColumns(record, query.columns, table_def);
        }
        
        result.rows.push_back(row);
        result.row_count++;
    });
    
    return result;
}

// ----------------------------------------------------------------------------
// Index Scan - Index use karke jaldi search
// (Fast search using index)
// ----------------------------------------------------------------------------
QueryResult QueryExecutor::indexScan(const SelectQuery& query, const TableDefinition* table_def,
                                    const IndexDefinition* index_def) {
    QueryResult result;
    
    // Column names set kar
    // (Set column names)
    if (query.select_all || query.columns.empty()) {
        for (const auto& col : table_def->columns) {
            result.column_names.push_back(col.name);
        }
    } else {
        result.column_names = query.columns;
    }
    
    // Check karo ki pehla column INTEGER PRIMARY KEY aa
    // (Check if first column is INTEGER PRIMARY KEY)
    bool has_rowid_column = false;
    if (!table_def->columns.empty() && table_def->columns[0].is_primary_key) {
        has_rowid_column = true;
    }
    
    // WHERE value kadho - string vich convert kar
    // (Get WHERE value - convert to string)
    std::string search_key;
    if (std::holds_alternative<std::string>(query.where->value)) {
        search_key = std::get<std::string>(query.where->value);
    } else if (std::holds_alternative<int64_t>(query.where->value)) {
        search_key = std::to_string(std::get<int64_t>(query.where->value));
    } else if (std::holds_alternative<double>(query.where->value)) {
        search_key = std::to_string(std::get<double>(query.where->value));
    }
    
    // Index B-tree use karke matching rowids dhundho
    // (Find matching rowids using index B-tree)
    IndexBTree index_tree(database_, index_def->root_page);
    std::vector<int64_t> rowids = index_tree.findRowIds(search_key);
    
    // Har rowid lai table vichon record kadho
    // (For each rowid, get record from table)
    BTree table_tree(database_, table_def->root_page);
    
    for (int64_t rowid : rowids) {
        auto record_opt = table_tree.findByKey(rowid);
        
        if (record_opt) {
            const Record& record = *record_opt;
            
            // Index ton milya hua row phir se check karo.
            // (Re-check the row we got back from the index.)
            // Index di keys te jadon column de affinity naal jadon litral
            // alag ho jande aa, tab hi ye zaroori hunda; warna ye har nahi.
            // (Only needed when index keys and the literal can differ because of
            // the column's affinity; for a plain text = text it is a no-op.)
            if (query.where && !evaluateWhere(record, *query.where, table_def)) {
                continue;
            }
            
            // Columns extract kar record vichon
            // (Extract columns from record)
            std::vector<std::string> row;
            
            if (query.select_all || query.columns.empty()) {
                // Saare columns (all columns)
                if (has_rowid_column) {
                    row.push_back(std::to_string(record.getRowId()));
                    for (size_t i = 1; i < record.getColumnCount(); ++i) {
                        row.push_back(columnValueToString(record.getColumnValue(i)));
                    }
                } else {
                    for (size_t i = 0; i < record.getColumnCount(); ++i) {
                        row.push_back(columnValueToString(record.getColumnValue(i)));
                    }
                }
            } else {
                row = extractColumns(record, query.columns, table_def);
            }
            
            result.rows.push_back(row);
            result.row_count++;
        }
    }
    
    return result;
}

// ----------------------------------------------------------------------------
// Evaluate WHERE - Condition check karo record te
// (Check condition on record)
// ----------------------------------------------------------------------------
bool QueryExecutor::evaluateWhere(const Record& record, const WhereCondition& where,
                                  const TableDefinition* table_def) {
    // Column index dhundho
    // (Find column index)
    int col_index = findColumnIndex(where.column_name, table_def);
    
    if (col_index < 0) {
        // Column nahi mileya - false return kar
        // (Column not found - return false)
        return false;
    }
    
    // Check karo ki rowid column aa
    // (Check if it's rowid column)
    bool has_rowid_column = false;
    if (!table_def->columns.empty() && table_def->columns[0].is_primary_key) {
        has_rowid_column = true;
    }
    
    ColumnValue record_value;
    
    if (has_rowid_column && col_index == 0) {
        // Pehla column rowid aa
        // (First column is rowid)
        record_value = record.getRowId();
    } else {
        // Record vichon value kadho
        // (Get value from record)
        // Note: record vich pehla column NULL placeholder aa agar has_rowid_column
        // (Note: first column in record is NULL placeholder if has_rowid_column)
        // So same index use kar
        // (So use same index)
        int record_index = col_index;
        
        if (record_index < 0 || record_index >= static_cast<int>(record.getColumnCount())) {
            return false;
        }
        
        record_value = record.getColumnValue(record_index);
    }
    
    // Compare karo
    // (Compare)
    return compareValues(record_value, where.op, where.value);
}

// ----------------------------------------------------------------------------
// Extract Columns - Specific columns kadho record vichon
// (Get specific columns from record)
// ----------------------------------------------------------------------------
std::vector<std::string> QueryExecutor::extractColumns(const Record& record,
                                                       const std::vector<std::string>& column_names,
                                                       const TableDefinition* table_def) {
    std::vector<std::string> values;
    
    // Check karo ki pehla column rowid column aa
    // (Check if first column is rowid column)
    bool has_rowid_column = false;
    if (!table_def->columns.empty() && table_def->columns[0].is_primary_key) {
        has_rowid_column = true;
    }
    
    for (const std::string& col_name : column_names) {
        int col_index = findColumnIndex(col_name, table_def);
        
        if (col_index >= 0) {
            // Column mileya - check karo rowid aa ya nahi
            // (Column found - check if it's rowid or not)
            if (has_rowid_column && col_index == 0) {
                // Pehla column rowid aa
                // (First column is rowid)
                values.push_back(std::to_string(record.getRowId()));
            } else {
                // Record vichon kadho
                // (Get from record)
                // Note: agar has_rowid_column aa, te record de pehle column vich NULL placeholder aa
                // (Note: if has_rowid_column is true, first record column has NULL placeholder)
                // So actual columns record vich col_index te shuru nahi, balki same index te ne
                // (So actual columns in record start at same index, not col_index - 1)
                int record_index = col_index;  // Same as schema index!
                
                if (record_index >= 0 && record_index < static_cast<int>(record.getColumnCount())) {
                    values.push_back(columnValueToString(record.getColumnValue(record_index)));
                } else {
                    values.push_back("NULL");
                }
            }
        } else {
            values.push_back("NULL");
        }
    }
    
    return values;
}

// ----------------------------------------------------------------------------
// Column Value to String - Value nu string vich convert
// (Convert value to string)
// ----------------------------------------------------------------------------
std::string QueryExecutor::columnValueToString(const ColumnValue& value) {
    if (std::holds_alternative<std::monostate>(value)) {
        return "NULL";
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        std::ostringstream oss;
        oss << std::get<double>(value);
        return oss.str();
    } else if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        return "<BLOB>";
    }
    
    return "";
}

// ----------------------------------------------------------------------------
// Literal Value to Text - WHERE value nu LIKE lai text bana do
// (Convert a WHERE literal to text, for LIKE)
// ----------------------------------------------------------------------------
std::string QueryExecutor::literalValueToText(const LiteralValue& value) {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        std::ostringstream oss;
        oss << std::get<double>(value);
        return oss.str();
    } else if (std::holds_alternative<bool>(value)) {
        // SQLite bool nu 1/0 maan lenda aa
        // (SQLite treats a boolean as 1/0)
        return std::to_string(static_cast<int64_t>(std::get<bool>(value)));
    }
    
    return "";
}

// ----------------------------------------------------------------------------
// Find Column Index - Column naam ton index dhundho
// (Find index from column name)
// ----------------------------------------------------------------------------
int QueryExecutor::findColumnIndex(const std::string& column_name, const TableDefinition* table_def) {
    for (size_t i = 0; i < table_def->columns.size(); ++i) {
        if (table_def->columns[i].name == column_name) {
            return static_cast<int>(i);
        }
    }
    
    return -1;  // Not found
}

// ----------------------------------------------------------------------------
// Compare Values - Do values compare karo operator naal
// (Compare two values with operator)
// ----------------------------------------------------------------------------
bool QueryExecutor::compareValues(const ColumnValue& record_value, CompareOp op,
                                 const LiteralValue& literal_value) {
    // NULL handling - NULL kisi naal vi match nahi
    // (NULL handling - NULL doesn't match with anything)
    if (std::holds_alternative<std::monostate>(record_value)) {
        return false;
    }
    
    // LIKE alag de - SQLite da LIKE dono operands nu TEXT bana ke match karda,
    // column de affinity nu koi matlab nahi padda ('é' jive multi-byte character
    // te `1` jive number dono match ho sakde aa).
    // (LIKE is special: SQLite's LIKE casts BOTH operands to TEXT before matching.
    // The column's affinity plays no part, so a number and a multi-byte character
    // can both match.)
    if (op == CompareOp::LIKE) {
        return sqlLikeMatch(columnValueToString(record_value),
                            literalValueToText(literal_value));
    }
    
    // Integer comparison - numbers da comparison
    // (Integer comparison - comparison of numbers)
    if (std::holds_alternative<int64_t>(record_value) && std::holds_alternative<int64_t>(literal_value)) {
        int64_t rec_val = std::get<int64_t>(record_value);
        int64_t lit_val = std::get<int64_t>(literal_value);
        
        switch (op) {
            case CompareOp::EQUAL: return rec_val == lit_val;
            case CompareOp::NOT_EQUAL: return rec_val != lit_val;
            case CompareOp::LESS_THAN: return rec_val < lit_val;
            case CompareOp::LESS_EQUAL: return rec_val <= lit_val;
            case CompareOp::GREATER_THAN: return rec_val > lit_val;
            case CompareOp::GREATER_EQUAL: return rec_val >= lit_val;
            default: return false;
        }
    }
    
    // Float comparison - decimal numbers
    if (std::holds_alternative<double>(record_value) && std::holds_alternative<double>(literal_value)) {
        double rec_val = std::get<double>(record_value);
        double lit_val = std::get<double>(literal_value);
        
        switch (op) {
            case CompareOp::EQUAL: return rec_val == lit_val;
            case CompareOp::NOT_EQUAL: return rec_val != lit_val;
            case CompareOp::LESS_THAN: return rec_val < lit_val;
            case CompareOp::LESS_EQUAL: return rec_val <= lit_val;
            case CompareOp::GREATER_THAN: return rec_val > lit_val;
            case CompareOp::GREATER_EQUAL: return rec_val >= lit_val;
            default: return false;
        }
    }
    
    // String comparison - text da comparison
    // (String comparison - comparison of text)
    if (std::holds_alternative<std::string>(record_value) && std::holds_alternative<std::string>(literal_value)) {
        const std::string& rec_val = std::get<std::string>(record_value);
        const std::string& lit_val = std::get<std::string>(literal_value);
        
        switch (op) {
            case CompareOp::EQUAL: return rec_val == lit_val;
            case CompareOp::NOT_EQUAL: return rec_val != lit_val;
            case CompareOp::LESS_THAN: return rec_val < lit_val;
            case CompareOp::LESS_EQUAL: return rec_val <= lit_val;
            case CompareOp::GREATER_THAN: return rec_val > lit_val;
            case CompareOp::GREATER_EQUAL: return rec_val >= lit_val;
            default: return false;
        }
    }
    
    // Type mismatch - false return kar
    // (Type mismatch - return false)
    return false;
}

// ----------------------------------------------------------------------------
// Find Usable Index - Query lai index dhundho
// (Find index for query)
// ----------------------------------------------------------------------------
const IndexDefinition* QueryExecutor::findUsableIndex(const SelectQuery& query,
                                                       const TableDefinition* table_def) {
    if (!query.where) {
        // WHERE nahi te index faida nahi
        // (No WHERE then index is useless)
        return nullptr;
    }
    
    // Table de saare indexes check karo
    // (Check all indexes of table)
    auto index_names = schema_.getIndexesForTable(table_def->name);
    
    // Sirf '=' naal index use kar sakte aa. Baaki har operator lai index chahida
    // hi nahi - `country > 'x'` lai range scan chahidi hundi aa, te `LIKE 'x%'`
    // lai prefix scan. Assi sirf exact lookup karwan dende aa, isliye baaki
    // operators te full table scan hi sahi rahega.
    // (Only '=' can use the index. Every other operator would need a range scan
    // ('country > x') or a prefix scan ("LIKE 'x%'"). We only implement exact
    // lookups, so for every other operator the full table scan is the right plan
    // - using the index there silently returns the wrong rows.)
    if (query.where->op != CompareOp::EQUAL) {
        return nullptr;
    }
    
    for (const auto& index_name : index_names) {
        const IndexDefinition* index_def = schema_.getIndexDefinition(index_name);
        
        if (!index_def || index_def->columns.empty()) {
            continue;
        }
        
        // Check karo WHERE column index vich aa ya nahi
        // (Check if WHERE column is in index)
        if (index_def->columns[0] == query.where->column_name) {
            // First column match - index use kar sakte
            // (First column matches - can use index)
            return index_def;
        }
    }
    
    return nullptr;
}

// ----------------------------------------------------------------------------
// Print Result - Result nu pretty format vich print
// (Print result in pretty format)
// ----------------------------------------------------------------------------
void printResult(const QueryResult& result) {
    // Column names print karo
    // (Print column names)
    for (size_t i = 0; i < result.column_names.size(); ++i) {
        if (i > 0) std::cout << "|";
        std::cout << result.column_names[i];
    }
    std::cout << "\n";
    
    // Rows print karo
    // (Print rows)
    for (const auto& row : result.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) std::cout << "|";
            std::cout << row[i];
        }
        std::cout << "\n";
    }
}

// ----------------------------------------------------------------------------
// Print Result CSV - CSV format vich print
// (Print in CSV format)
// ----------------------------------------------------------------------------
void printResultCSV(const QueryResult& result) {
    // Column names
    for (size_t i = 0; i < result.column_names.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << result.column_names[i];
    }
    std::cout << "\n";
    
    // Rows
    for (const auto& row : result.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << row[i];
        }
        std::cout << "\n";
    }
}

} // namespace sqlite
