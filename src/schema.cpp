#include "schema.hpp"
#include "btree.hpp"
#include "utils/diagnostics.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

// ============================================================================
// SCHEMA.CPP - SQLite Schema Parser Implementation
// ============================================================================
// Oye schema.cpp vich assi sqlite_schema table parse karde aa!
// (Hey in schema.cpp we parse sqlite_schema table!)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Constructor - Schema object bana
// (Create schema object)
// ----------------------------------------------------------------------------
Schema::Schema(Database& database)
    : database_(database)
    , loaded_(false)
{
}

// ----------------------------------------------------------------------------
// Load - sqlite_schema table vichon schema load karo
// (Load schema from sqlite_schema table)
// ----------------------------------------------------------------------------
// sqlite_schema hamesha page 1 te hunda - first page
// (sqlite_schema is always on page 1 - first page)
void Schema::load() {
    if (loaded_) {
        // Pehle hi load aa - dubara nahi karna
        // (Already loaded - don't do again)
        return;
    }
    
    // Page 1 te sqlite_schema aa - B-tree scan karo
    // (sqlite_schema is on page 1 - scan B-tree)
    BTree schema_tree(database_, 1);
    
    schema_tree.scanAll([this](const Record& record) {
        // sqlite_schema de columns:
        // (sqlite_schema columns:)
        // 0: type (table, index, view, trigger)
        // 1: name (object naam)
        // 2: tbl_name (table naam jis naal related)
        // 3: rootpage (B-tree root page number)
        // 4: sql (CREATE statement)
        
        try {
            auto type_str = record.getString(0);
            auto name = record.getString(1);
            auto tbl_name = record.getString(2);
            auto rootpage = record.getInt(3);
            auto sql = record.getString(4);
            
            if (!type_str || !name || !rootpage) {
                // Required fields missing - skip
                return;
            }
            
            SchemaObjectType obj_type = parseObjectType(*type_str);
            
            if (obj_type == SchemaObjectType::TABLE) {
                // Table definition bana
                // (Create table definition)
                TableDefinition table_def;
                table_def.name = *name;
                table_def.root_page = static_cast<uint32_t>(*rootpage);
                table_def.sql = sql ? *sql : "";
                
                // SQL parse karke columns kadho
                // (Parse SQL and get columns)
                if (!table_def.sql.empty()) {
                    table_def.columns = parseColumns(table_def.sql);
                }
                
                tables_[table_def.name] = table_def;
                
            } else if (obj_type == SchemaObjectType::INDEX) {
                // Index definition bana
                // (Create index definition)
                IndexDefinition index_def;
                index_def.name = *name;
                index_def.table_name = tbl_name ? *tbl_name : "";
                index_def.root_page = static_cast<uint32_t>(*rootpage);
                index_def.sql = sql ? *sql : "";
                
                // SQL parse karke index columns kadho
                // (Parse SQL and get index columns)
                if (!index_def.sql.empty()) {
                    index_def.columns = parseIndexColumns(index_def.sql);
                    // UNIQUE check karo SQL vich
                    // (Check for UNIQUE in SQL)
                    std::string sql_lower = index_def.sql;
                    std::transform(sql_lower.begin(), sql_lower.end(), sql_lower.begin(), ::tolower);
                    index_def.is_unique = (sql_lower.find("unique") != std::string::npos);
                }
                
                indexes_[index_def.name] = index_def;
            }
            
        } catch (const std::exception& e) {
            // Oh sqlite_schema row parh nahi payi - oh table ya index register
            // nahi hoga. Chhad de te dasso zaroor, warna user nu "Table not found"
            // milk e oh sochega table hi nahi hai.
            // (That sqlite_schema row could not be read - the table or index will
            // not be registered. Skip it, but say so: otherwise the user gets
            // "Table not found" and thinks the table does not exist.)
            report(Severity::Error, "Schema::load(sqlite_schema row)", e.what());
            return;
        }
    });
    
    loaded_ = true;
}

// ----------------------------------------------------------------------------
// Get Table Names - Saare tables de naam
// (Names of all tables)
// ----------------------------------------------------------------------------
std::vector<std::string> Schema::getTableNames() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());
    
    for (const auto& pair : tables_) {
        names.push_back(pair.first);
    }
    
    return names;
}

// ----------------------------------------------------------------------------
// Get Table Definition - Table di puri definition
// (Complete definition of table)
// ----------------------------------------------------------------------------
const TableDefinition* Schema::getTableDefinition(const std::string& table_name) const {
    auto it = tables_.find(table_name);
    if (it != tables_.end()) {
        return &it->second;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Get Index Names - Saare indexes de naam
// (Names of all indexes)
// ----------------------------------------------------------------------------
std::vector<std::string> Schema::getIndexNames() const {
    std::vector<std::string> names;
    names.reserve(indexes_.size());
    
    for (const auto& pair : indexes_) {
        names.push_back(pair.first);
    }
    
    return names;
}

// ----------------------------------------------------------------------------
// Get Index Definition - Index di puri definition
// (Complete definition of index)
// ----------------------------------------------------------------------------
const IndexDefinition* Schema::getIndexDefinition(const std::string& index_name) const {
    auto it = indexes_.find(index_name);
    if (it != indexes_.end()) {
        return &it->second;
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Get Indexes For Table - Table lai saare indexes
// (All indexes for table)
// ----------------------------------------------------------------------------
std::vector<std::string> Schema::getIndexesForTable(const std::string& table_name) const {
    std::vector<std::string> table_indexes;
    
    for (const auto& pair : indexes_) {
        if (pair.second.table_name == table_name) {
            table_indexes.push_back(pair.first);
        }
    }
    
    return table_indexes;
}

// ----------------------------------------------------------------------------
// Get User Table Count - User tables (system tables nu chhad ke)
// (User tables (excluding system tables))
// ----------------------------------------------------------------------------
// sqlite_sequence, sqlite_stat1 etc system tables ne
// (sqlite_sequence, sqlite_stat1 etc are system tables)
size_t Schema::getUserTableCount() const {
    size_t count = 0;
    
    for (const auto& pair : tables_) {
        const std::string& name = pair.first;
        // System tables sqlite_ ton shuru hunde
        // (System tables start with sqlite_)
        if (name.find("sqlite_") != 0) {
            count++;
        }
    }
    
    return count;
}

// ----------------------------------------------------------------------------
// Parse Object Type - String vichon enum
// (Enum from string)
// ----------------------------------------------------------------------------
SchemaObjectType Schema::parseObjectType(const std::string& type_str) {
    std::string lower = type_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "table") return SchemaObjectType::TABLE;
    if (lower == "index") return SchemaObjectType::INDEX;
    if (lower == "view") return SchemaObjectType::VIEW;
    if (lower == "trigger") return SchemaObjectType::TRIGGER;
    
    return SchemaObjectType::UNKNOWN;
}

// ----------------------------------------------------------------------------
// Parse Columns - CREATE TABLE SQL vichon columns parse karo
// (Parse columns from CREATE TABLE SQL)
// ----------------------------------------------------------------------------
// Example: CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL)
// Columns kadho: id (INTEGER, PRIMARY KEY), name (TEXT, NOT NULL)
// (Get columns: id (INTEGER, PRIMARY KEY), name (TEXT, NOT NULL))
std::vector<ColumnDefinition> Schema::parseColumns(const std::string& sql) {
    std::vector<ColumnDefinition> columns;
    
    // Simple parser - brackets de beech vala part kadho
    // (Simple parser - get part between brackets)
    size_t start = sql.find('(');
    size_t end = sql.rfind(')');
    
    if (start == std::string::npos || end == std::string::npos || start >= end) {
        // Brackets nahi mile - empty return kar
        // (Brackets not found - return empty)
        return columns;
    }
    
    std::string columns_str = sql.substr(start + 1, end - start - 1);
    
    // Comma te split kar - har column
    // (Split on comma - each column)
    std::istringstream iss(columns_str);
    std::string column_def;
    
    while (std::getline(iss, column_def, ',')) {
        // Trim spaces
        size_t first = column_def.find_first_not_of(" \t\n\r");
        size_t last = column_def.find_last_not_of(" \t\n\r");
        
        if (first == std::string::npos) continue;
        
        column_def = column_def.substr(first, last - first + 1);
        
        // Column naam te type parse karo
        // (Parse column name and type)
        std::istringstream col_stream(column_def);
        std::string col_name, col_type;
        
        col_stream >> col_name >> col_type;
        
        if (col_name.empty()) continue;
        
        // Quotes hata column naam vichon
        // (Remove quotes from column name)
        if (col_name.front() == '"' || col_name.front() == '\'') {
            col_name = col_name.substr(1);
        }
        if (!col_name.empty() && (col_name.back() == '"' || col_name.back() == '\'')) {
            col_name = col_name.substr(0, col_name.size() - 1);
        }
        
        ColumnDefinition col(col_name, col_type);
        
        // Constraints check karo - PRIMARY KEY, NOT NULL
        // (Check constraints - PRIMARY KEY, NOT NULL)
        std::string lower_def = column_def;
        std::transform(lower_def.begin(), lower_def.end(), lower_def.begin(), ::tolower);
        
        col.is_primary_key = (lower_def.find("primary key") != std::string::npos);
        col.not_null = (lower_def.find("not null") != std::string::npos) || col.is_primary_key;
        
        columns.push_back(col);
    }
    
    return columns;
}

// ----------------------------------------------------------------------------
// Parse Index Columns - CREATE INDEX SQL vichon columns
// (Columns from CREATE INDEX SQL)
// ----------------------------------------------------------------------------
// Example: CREATE INDEX idx_name ON users(name, age)
// Columns: ["name", "age"]
std::vector<std::string> Schema::parseIndexColumns(const std::string& sql) {
    std::vector<std::string> columns;
    
    // Brackets de beech column names ne
    // (Column names are between brackets)
    size_t start = sql.find('(');
    size_t end = sql.rfind(')');
    
    if (start == std::string::npos || end == std::string::npos) {
        return columns;
    }
    
    std::string cols_str = sql.substr(start + 1, end - start - 1);
    
    // Comma te split kar
    // (Split on comma)
    std::istringstream iss(cols_str);
    std::string col;
    
    while (std::getline(iss, col, ',')) {
        // Trim te clean kar
        // (Trim and clean)
        size_t first = col.find_first_not_of(" \t\n\r");
        size_t last = col.find_last_not_of(" \t\n\r");
        
        if (first != std::string::npos) {
            col = col.substr(first, last - first + 1);
            
            // Quotes hata
            // (Remove quotes)
            if (!col.empty() && (col.front() == '"' || col.front() == '\'')) {
                col = col.substr(1);
            }
            if (!col.empty() && (col.back() == '"' || col.back() == '\'')) {
                col = col.substr(0, col.size() - 1);
            }
            
            if (!col.empty()) {
                columns.push_back(col);
            }
        }
    }
    
    return columns;
}

} // namespace sqlite
