#pragma once

#include "database.hpp"
#include "schema.hpp"
#include "sql_parser.hpp"
#include "btree.hpp"
#include <string>
#include <vector>

// ============================================================================
// QUERY_EXECUTOR.HPP - SQL Query Executor
// ============================================================================
// Oye query_executor.hpp vich assi SQL queries execute karde aa!
// (Hey in query_executor.hpp we execute SQL queries!)
//
// Parser ton parsed query mildi, assi onu execute karke results dinde
// (We get parsed query from parser, we execute it and give results)
// Full table scan ya index scan - jo zaroorat howe oh use karde
// (Full table scan or index scan - use whatever is needed)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Query Result - Query di output
// (Query output)
// ----------------------------------------------------------------------------
struct QueryResult {
    std::vector<std::string> column_names;          // Column names - output de columns (output columns)
    std::vector<std::vector<std::string>> rows;     // Data rows - har row strings di list (each row is list of strings)
    size_t row_count;                               // Kitne rows ne (how many rows)
    
    // Constructor
    QueryResult() : row_count(0) {}
};

// ----------------------------------------------------------------------------
// Query Executor Class - Queries execute karda
// (Executes queries)
// ----------------------------------------------------------------------------
class QueryExecutor {
public:
    // Constructor - Database te schema chaiye execute karne lai
    // (Need database and schema to execute)
    QueryExecutor(Database& database, Schema& schema);
    
    // Destructor
    ~QueryExecutor() = default;
    
    // SELECT query execute karo
    // (Execute SELECT query)
    // Returns result agar success, throws exception agar error
    // (Returns result if success, throws exception if error)
    QueryResult execute(const SelectQuery& query);
    
    // COUNT query execute karo - kitne rows ne table vich
    // (Execute COUNT query - how many rows in table)
    size_t count(const std::string& table_name);
    
private:
    // Helper methods - Andar de kaam (Internal work)
    
    // Full table scan - puri table padh ke filter karo
    // (Full table scan - read entire table and filter)
    // Slow hai par hamesha kaam karda - jab index nahi
    // (Slow but always works - when no index)
    QueryResult fullTableScan(const SelectQuery& query, const TableDefinition* table_def);
    
    // Index scan - index use karke jaldi records dhundho
    // (Index scan - quickly find records using index)
    // Fast hai par index chaiye (Fast but needs index)
    QueryResult indexScan(const SelectQuery& query, const TableDefinition* table_def, 
                         const IndexDefinition* index_def);
    
    // WHERE condition check karo ek record te
    // (Check WHERE condition on one record)
    // Returns true agar condition match, false nahi te
    // (Returns true if condition matches, false otherwise)
    bool evaluateWhere(const Record& record, const WhereCondition& where, 
                      const TableDefinition* table_def);
    
    // Record vichon specific columns extract karo
    // (Extract specific columns from record)
    std::vector<std::string> extractColumns(const Record& record, 
                                           const std::vector<std::string>& column_names,
                                           const TableDefinition* table_def);
    
    // Column value nu string vich convert karo display lai
    // (Convert column value to string for display)
    std::string columnValueToString(const ColumnValue& value);
    
    // Column naam vichon column index dhundho table vich
    // (Find column index from column name in table)
    int findColumnIndex(const std::string& column_name, const TableDefinition* table_def);
    
    // Comparison karo do values di
    // (Compare two values)
    bool compareValues(const ColumnValue& record_value, CompareOp op, const LiteralValue& literal_value);
    
    // Check karo ki index use ho sakda query lai
    // (Check if index can be used for query)
    const IndexDefinition* findUsableIndex(const SelectQuery& query, const TableDefinition* table_def);
    
    // Member variables - Executor de andar da data
    // (Data inside executor)
    Database& database_;        // Database reference - file padhne lai (for reading file)
    Schema& schema_;            // Schema reference - table definitions lai (for table definitions)
};

// ----------------------------------------------------------------------------
// Helper Functions - Display te formatting lai
// (For display and formatting)
// ----------------------------------------------------------------------------

// Result nu pretty print karo - table format vich
// (Pretty print result - in table format)
void printResult(const QueryResult& result);

// Result nu CSV format vich print karo
// (Print result in CSV format)
void printResultCSV(const QueryResult& result);

} // namespace sqlite
