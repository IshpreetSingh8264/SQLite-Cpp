// ============================================================================
// MAIN.CPP - SQLite Database Engine Entry Point
// ============================================================================
// Oye balle balle! Yahaan se saara program shuru hunda!
// (Hey awesome! The whole program starts from here!)
//
// Eh file database di darvaja aa - user de commands handle karde aa
// (This file is database's gateway - handles user commands)
// ============================================================================

#include "database.hpp"
#include "schema.hpp"
#include "sql_parser.hpp"
#include "query_executor.hpp"
#include "btree.hpp"
#include "utils/diagnostics.hpp"
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

// ----------------------------------------------------------------------------
// Handle .dbinfo command - Database di basic jankari dikha
// (Show basic database information)
// ----------------------------------------------------------------------------
// Oye eh command database de bare vich sab kuch dassda
// (Hey this command tells everything about database)
void handleDbInfo(sqlite::Database& db, sqlite::Schema& schema) {
    // Database page size - har page kitna wadda
    // (Database page size - how big each page)
    std::cout << "database page size: " << db.getPageSize() << std::endl;
    
    // Schema load kar - saare tables dhundh
    // (Load schema - find all tables)
    schema.load();
    
    // Kitne tables ne total - user tables, system tables nu chhad ke
    // (How many tables total - user tables, excluding system tables)
    size_t table_count = schema.getUserTableCount();
    std::cout << "number of tables: " << table_count << std::endl;
}

// ----------------------------------------------------------------------------
// Handle .tables command - Saare tables de naam dikha
// (Show names of all tables)
// ----------------------------------------------------------------------------
void handleTables(sqlite::Schema& schema) {
    // Schema load kar agar nahi hoyi
    // (Load schema if not loaded)
    schema.load();
    
    // Saare table names kadho
    // (Get all table names)
    std::vector<std::string> table_names = schema.getTableNames();
    
    // System tables skip kar - sqlite_ ton shuru wale
    // (Skip system tables - ones starting with sqlite_)
    for (const auto& name : table_names) {
        if (name.find("sqlite_") != 0) {
            std::cout << name << " ";
        }
    }
    std::cout << std::endl;
}

// ----------------------------------------------------------------------------
// Handle COUNT command - Table vich kitne rows ne
// (How many rows in table)
// ----------------------------------------------------------------------------
// Example: SELECT COUNT(*) FROM users
void handleCount(sqlite::Database& db, sqlite::Schema& schema, const std::string& table_name) {
    // Schema load kar
    // (Load schema)
    schema.load();
    
    // Table definition dhundho
    // (Find table definition)
    const sqlite::TableDefinition* table_def = schema.getTableDefinition(table_name);
    
    if (!table_def) {
        // Table nahi mili yaar!
        // (Table not found dude!)
        std::cerr << "Table not found: " << table_name << std::endl;
        return;
    }
    
    // B-tree scan karke count kar
    // (Scan B-tree and count)
    sqlite::BTree tree(db, table_def->root_page);
    size_t count = tree.countRecords();
    
    std::cout << count << std::endl;
}

// ----------------------------------------------------------------------------
// Handle SELECT command - Query execute kar te results dikha
// (Execute query and show results)
// ----------------------------------------------------------------------------
void handleSelect(sqlite::Database& db, sqlite::Schema& schema, const std::string& sql) {
    // Schema load kar - table definitions chaiye
    // (Load schema - need table definitions)
    schema.load();
    
    // Special case - COUNT(*) direct handle karo
    // (Special case - handle COUNT(*) directly)
    std::string sql_upper = sql;
    std::transform(sql_upper.begin(), sql_upper.end(), sql_upper.begin(), ::toupper);
    
    if (sql_upper.find("COUNT(*)") != std::string::npos) {
        // COUNT query aa - table naam dhundho
        // (It's COUNT query - find table name)
        size_t from_pos = sql_upper.find("FROM");
        if (from_pos != std::string::npos) {
            std::string table_part = sql.substr(from_pos + 4);
            // Trim spaces
            size_t start = table_part.find_first_not_of(" \t");
            if (start != std::string::npos) {
                size_t end = table_part.find_first_of(" \t;", start);
                std::string table_name = (end == std::string::npos) ? 
                    table_part.substr(start) : table_part.substr(start, end - start);
                handleCount(db, schema, table_name);
                return;
            }
        }
    }
    
    // SQL parse kar - query samajh
    // (Parse SQL - understand query)
    sqlite::SqlParser parser;
    auto query = parser.parseSelect(sql);
    
    if (!query) {
        // Query parse nahi hoyi - galat syntax!
        // (Query didn't parse - wrong syntax!)
        std::cerr << "Parse error: " << parser.getError() << std::endl;
        return;
    }
    
    // Query executor bana te execute kar
    // (Create query executor and execute)
    sqlite::QueryExecutor executor(db, schema);
    
    try {
        sqlite::QueryResult result = executor.execute(*query);
        
        // Results print kar - har row
        // (Print results - each row)
        for (const auto& row : result.rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) std::cout << "|";
                std::cout << row[i];
            }
            std::cout << std::endl;
        }
        
    } catch (const std::exception& e) {
        // Koi error aayi - user nu dassa
        // (Some error occurred - tell user)
        std::cerr << "Query execution error: " << e.what() << std::endl;
    }
}

// ----------------------------------------------------------------------------
// Main Function - Program di entry point, yahaan ton shuru aa
// (Program entry point, starts from here)
// ----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Output buffering band kar - turant results dikhan
    // (Disable output buffering - show results immediately)
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    // Logs lai - debug karde waqt kaam aunde
    // (For logs - useful when debugging)
    std::cerr << "Logs from your program will appear here" << std::endl;
    
    // Arguments check kar - kamse kam 3 chaiye
    // (Check arguments - need at least 3)
    if (argc != 3) {
        std::cerr << "Oye usage galat aa! Expected two arguments yaar!" << std::endl;
        // (Hey usage is wrong! Expected two arguments dude!)
        std::cerr << "Usage: " << argv[0] << " <database_file> <command>" << std::endl;
        return 1;
    }
    
    std::string database_file_path = argv[1];  // Database file da path (database file path)
    std::string command = argv[2];             // Command - ki karna aa (command - what to do)
    
    try {
        // Database kholo - file read karo
        // (Open database - read file)
        sqlite::Database db(database_file_path);
        
        if (!db.isValid()) {
            // Database valid nahi - corrupt ya galat file
            // (Database not valid - corrupt or wrong file)
            std::cerr << "Database error: " << db.getError() << std::endl;
            return 1;
        }
        
        // Schema object bana - tables di jankari lai
        // (Create schema object - for table information)
        sqlite::Schema schema(db);
        
        // Command ki aa - check kar te handle kar
        // (What command is it - check and handle)
        
        if (command == ".dbinfo") {
            // Database info command - basic jankari dikha
            // (Database info command - show basic information)
            handleDbInfo(db, schema);
            
        } else if (command == ".tables") {
            // Tables list command - saare tables de naam
            // (Tables list command - names of all tables)
            handleTables(schema);
            
        } else if (command.find("SELECT") == 0 || command.find("select") == 0) {
            // SELECT query aa - execute kar
            // (It's SELECT query - execute it)
            handleSelect(db, schema, command);
            
        } else {
            // Unknown command - samajh nahi aaya
            // (Unknown command - didn't understand)
            std::cerr << "Oye unknown command: " << command << std::endl;
            std::cerr << "Supported commands:" << std::endl;
            std::cerr << "  .dbinfo          - Database di jankari (database information)" << std::endl;
            std::cerr << "  .tables          - Saare tables de naam (all table names)" << std::endl;
            std::cerr << "  SELECT ...       - SQL query execute karo (execute SQL query)" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        // Koi exception aayi - error handle kar
        // (Some exception occurred - handle error)
        std::cerr << "Error: " << e.what() << std::endl;
        // Ki aaya si oh dass de, ohde naal
        // (Also report what we collected on the way)
        sqlite::printSummary();
        return 1;
    }
    
    // Agar kade kade koi cell decode nahi hoyi te user nu pata lage
    // (If any cell failed to decode, make sure the user finds out)
    sqlite::printSummary();
    
    // Sab kuch theek aa - success!
    // (Everything is fine - success!)
    return 0;
}

// ============================================================================
// Shabash! Program complete aa!
// (Bravo! Program is complete!)
//
// Features implemented:
//   ✓ Database header parsing - 100 bytes di puri jankari (complete 100 byte info)
//   ✓ Page reading - saare page types (all page types)
//   ✓ B-tree navigation - tree vich ghoomna (roaming in tree)
//   ✓ Record decoding - varint, serial types, sab kuch (varint, serial types, everything)
//   ✓ Schema parsing - tables te indexes (tables and indexes)
//   ✓ SQL parsing - SELECT queries samajhna (understanding SELECT queries)
//   ✓ Query execution - full scan te index scan (full scan and index scan)
//   ✓ WHERE clause filtering - conditions check karna (checking conditions)
//   ✓ Multiple column support - kai saare columns (many columns)
//
// Assi pura SQLite bana ditta! Kaam khatam!
// (We built complete SQLite! Work done!)
// ============================================================================
