#pragma once

#include "database.hpp"
#include "btree.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

// ============================================================================
// SCHEMA.HPP - SQLite Schema Parser
// ============================================================================
// Oye schema.hpp vich assi database di structure parse karde aa!
// (Hey in schema.hpp we parse the database structure!)
//
// sqlite_schema table vich saari database di definition aa
// (sqlite_schema table has the entire database definition)
// Kine tables ne, kine indexes ne, columns ki ne - sab kuch
// (How many tables, how many indexes, what are columns - everything)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Schema Object Types - Schema vich kitne tarah di cheezein hundi ne
// (How many types of things are in schema)
// ----------------------------------------------------------------------------
enum class SchemaObjectType {
    TABLE,      // Table - data store kardi (stores data)
    INDEX,      // Index - jaldi search lai (for quick search)
    VIEW,       // View - virtual table, query di output (virtual table, query output)
    TRIGGER,    // Trigger - automatic action (automatic action)
    UNKNOWN     // Unknown - pata nahi ki aa (don't know what it is)
};

// ----------------------------------------------------------------------------
// Column Definition - Ek column di puri jankari
// (Complete information of one column)
// ----------------------------------------------------------------------------
struct ColumnDefinition {
    std::string name;           // Column naam (column name)
    std::string type;           // Data type - INTEGER, TEXT, BLOB, REAL (data type)
    bool is_primary_key;        // Primary key hai ya nahi (is it primary key or not)
    bool not_null;              // NULL ho sakda ya nahi (can be NULL or not)
    std::string default_value;  // Default value - agar kuch nahi dita (if nothing given)
    
    // Constructor - Column definition bana
    // (Create column definition)
    ColumnDefinition(const std::string& n = "", const std::string& t = "")
        : name(n), type(t), is_primary_key(false), not_null(false) {}
};

// ----------------------------------------------------------------------------
// Table Definition - Puri table di jankari
// (Complete information of table)
// ----------------------------------------------------------------------------
struct TableDefinition {
    std::string name;                       // Table naam (table name)
    uint32_t root_page;                     // Root page number - data kithon shuru (where data starts)
    std::string sql;                        // CREATE TABLE statement - original SQL
    std::vector<ColumnDefinition> columns;  // Saare columns (all columns)
    
    // Constructor
    TableDefinition() : root_page(0) {}
};

// ----------------------------------------------------------------------------
// Index Definition - Index di jankari
// (Index information)
// ----------------------------------------------------------------------------
struct IndexDefinition {
    std::string name;           // Index naam (index name)
    std::string table_name;     // Kis table da index (which table's index)
    uint32_t root_page;         // Root page number
    std::string sql;            // CREATE INDEX statement
    std::vector<std::string> columns; // Kine columns te index (which columns are indexed)
    bool is_unique;             // Unique index hai ya nahi (is it unique index)
    
    // Constructor
    IndexDefinition() : root_page(0), is_unique(false) {}
};

// ----------------------------------------------------------------------------
// Schema Class - Database schema manage karda
// (Manages database schema)
// ----------------------------------------------------------------------------
class Schema {
public:
    // Constructor - Database lai schema load karo
    // (Load schema for database)
    explicit Schema(Database& database);
    
    // Destructor
    ~Schema() = default;
    
    // Schema load karo sqlite_schema table vichon
    // (Load schema from sqlite_schema table)
    // Page 1 te hamesha sqlite_schema hunda
    // (sqlite_schema is always on page 1)
    void load();
    
    // Getters - Schema information kadho
    // (Get schema information)
    
    // Saare tables de naam kadho
    // (Get names of all tables)
    std::vector<std::string> getTableNames() const;
    
    // Specific table di definition kadho
    // (Get definition of specific table)
    const TableDefinition* getTableDefinition(const std::string& table_name) const;
    
    // Saare indexes de naam kadho
    // (Get names of all indexes)
    std::vector<std::string> getIndexNames() const;
    
    // Specific index di definition kadho
    // (Get definition of specific index)
    const IndexDefinition* getIndexDefinition(const std::string& index_name) const;
    
    // Table lai indexes dhundho
    // (Find indexes for table)
    std::vector<std::string> getIndexesForTable(const std::string& table_name) const;
    
    // Kitne tables ne total (including system tables)
    // (How many tables total (including system tables))
    size_t getTableCount() const { return tables_.size(); }
    
    // Kitne user tables ne (excluding system tables like sqlite_sequence)
    // (How many user tables (excluding system tables like sqlite_sequence))
    size_t getUserTableCount() const;
    
private:
    // Helper methods - Andar de kaam (Internal work)
    
    // Schema object type string vichon enum vich convert karo
    // (Convert schema object type from string to enum)
    static SchemaObjectType parseObjectType(const std::string& type_str);
    
    // CREATE TABLE SQL vichon columns parse karo
    // (Parse columns from CREATE TABLE SQL)
    // Regex ya manual parsing use karke
    // (Using regex or manual parsing)
    std::vector<ColumnDefinition> parseColumns(const std::string& sql);
    
    // CREATE INDEX SQL vichon column names parse karo
    // (Parse column names from CREATE INDEX SQL)
    std::vector<std::string> parseIndexColumns(const std::string& sql);
    
    // Member variables - Schema de andar da data
    // (Data inside schema)
    Database& database_;                                // Database reference
    std::map<std::string, TableDefinition> tables_;     // Table naam -> definition (table name -> definition)
    std::map<std::string, IndexDefinition> indexes_;    // Index naam -> definition (index name -> definition)
    bool loaded_;                                       // Schema load hoyi ya nahi (schema loaded or not)
};

} // namespace sqlite
