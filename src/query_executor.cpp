#include "query_executor.hpp"
#include "utils/like.hpp"
#include "utils/value_compare.hpp"
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
    
    // Check karo ki kaunsa column rowid da alias aa
    // (Work out which column, if any, is the rowid alias)
    const int rowid_alias = rowIdAliasIndex(*table_def);
    
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
            // Saare columns
            // (All columns)
            // Rowid alias wale column da record vich NULL placeholder hunda, isliye
            // usde jagah record de rowid print karna padta hai.
            // (The rowid alias column holds a NULL placeholder in the record, so
            // the record's rowid has to be printed in its place.)
            row = projectWholeRecord(record, table_def, rowid_alias);
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
// Project Whole Record - Poora record table de column order vich project karo
// (Project a whole record in the table's column order)
// ----------------------------------------------------------------------------
// Ke columns hain te oh kithon aane ne, ohdi decision ohdaar aa; ohna da kaam
// ikhde. Isliye indexScan te fullTableScan dono ihdi function use karde aa.
// (Deciding which columns exist and where they come from belongs to the caller;
// this is only the rendering. So indexScan and fullTableScan share it.)
std::vector<std::string> QueryExecutor::projectWholeRecord(const Record& record,
                                                          const TableDefinition* table_def,
                                                          int rowid_alias) {
    std::vector<std::string> row;
    
    // Alias hundi te us column tak da schema hisaab poora hona chahida
    // (With an alias, go as far as the schema says the table goes)
    size_t width = record.getColumnCount();
    if (rowid_alias >= 0) {
        width = std::max(width, table_def->columns.size());
    }
    
    row.reserve(width);
    
    for (size_t i = 0; i < width; ++i) {
        if (static_cast<int>(i) == rowid_alias) {
            // Ye column rowid da alias aa - asli value rowid hi aa
            // (This column aliases the rowid: its real value IS the rowid)
            row.push_back(std::to_string(record.getRowId()));
        } else if (i < record.getColumnCount()) {
            row.push_back(columnValueToString(record.getColumnValue(i)));
        } else {
            row.push_back("NULL");
        }
    }
    
    return row;
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
    
    // Check karo ki kaunsa column rowid da alias aa
    // (Work out which column, if any, is the rowid alias)
    const int rowid_alias = rowIdAliasIndex(*table_def);
    
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
                row = projectWholeRecord(record, table_def, rowid_alias);
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
    
    // Check karo ki kaunsa column rowid da alias aa
    // (Work out which column, if any, is the rowid alias)
    const int rowid_alias = rowIdAliasIndex(*table_def);
    
    ColumnValue record_value;
    
    if (rowid_alias >= 0 && col_index == rowid_alias) {
        // Ye column rowid da alias aa - value rowid hi aa, record vich NULL hai
        // (This column aliases the rowid: its value is the rowid, NULL in the record)
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
    // SQLite te hamesha column di declared type vichon affinity nikal ke dusre
    // operand nu convert karda hai - `WHERE str_col = 5` nu bhi.
    // (SQLite always derives an affinity from the column's declared type and
    // converts the other operand, including for `WHERE str_col = 5`.)
    return valueMatches(record_value, columnAffinity(columnTypeAt(col_index, table_def)),
                        where.op, where.value);
}

// ----------------------------------------------------------------------------
// Extract Columns - Specific columns kadho record vichon
// (Get specific columns from record)
// ----------------------------------------------------------------------------
std::vector<std::string> QueryExecutor::extractColumns(const Record& record,
                                                       const std::vector<std::string>& column_names,
                                                       const TableDefinition* table_def) {
    std::vector<std::string> values;
    
    // Check karo ki kaunsa column rowid da alias aa
    // (Work out which column, if any, is the rowid alias)
    const int rowid_alias = rowIdAliasIndex(*table_def);
    
    for (const std::string& col_name : column_names) {
        int col_index = findColumnIndex(col_name, table_def);
        
        if (col_index >= 0) {
            // Column mileya - check karo rowid alias aa ya nahi
            // (Column found - check whether it is the rowid alias)
            if (rowid_alias >= 0 && col_index == rowid_alias) {
                values.push_back(std::to_string(record.getRowId()));
            } else {
                // Record vichon kadho. Schema index te record index same hi hai -
                // rowid alias wale column da NULL placeholder usi position te baitha
                // hunda hai, isliye koi shift nahi karna.
                // (Take it from the record. The schema index and the record index
                // are the same, because the rowid alias column's NULL placeholder
                // sits at that same position, so nothing needs shifting.)
                if (col_index < static_cast<int>(record.getColumnCount())) {
                    values.push_back(columnValueToString(record.getColumnValue(col_index)));
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
        // SQLite jive render karo - 5.0 nu "5.0" dassna aa, "5" nahi
        // (Render the way SQLite does: 5.0 must print as "5.0", not "5")
        return realToText(std::get<double>(value));
    } else if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        return "<BLOB>";
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
// Column Type At - Us index te column da declared type
// (The declared type of the column at that index)
// ----------------------------------------------------------------------------
std::string QueryExecutor::columnTypeAt(int column_index, const TableDefinition* table_def) {
    if (column_index < 0 || column_index >= static_cast<int>(table_def->columns.size())) {
        return "";
    }
    
    return table_def->columns[static_cast<size_t>(column_index)].type;
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
