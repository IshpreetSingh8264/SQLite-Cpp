#include "command_registry.hpp"
#include "sql_parser.hpp"
#include "query_executor.hpp"
#include "btree.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

// ============================================================================
// COMMANDS/COMMAND_REGISTRY.CPP - The Command Table Implementation
// ============================================================================

namespace sqlite {

namespace {

// ----------------------------------------------------------------------------
// Handle Db Info - Database di basic jankari dikha
// (Show basic database information)
// ----------------------------------------------------------------------------
void handleDbInfo(Database& db, Schema& schema, const std::string&) {
    // Database page size - har page kitna wadda
    // (Database page size - how big each page)
    std::cout << "database page size: " << db.getPageSize() << std::endl;
    
    // Schema load kar - saare tables dhundh
    // (Load schema - find all tables)
    schema.load();
    
    // Kitne tables ne total - user tables, system tables nu chhad ke
    // (How many tables total - user tables, excluding system tables)
    std::cout << "number of tables: " << schema.getUserTableCount() << std::endl;
}

// ----------------------------------------------------------------------------
// Handle Tables - Saare tables de naam dikha
// (Show the names of all tables)
// ----------------------------------------------------------------------------
void handleTables(Database&, Schema& schema, const std::string&) {
    // Schema load kar agar nahi hoyi
    // (Load the schema if it is not loaded)
    schema.load();
    
    // System tables skip kar - sqlite_ ton shuru wale
    // (Skip system tables - the ones starting with sqlite_)
    for (const auto& name : schema.getTableNames()) {
        if (name.find("sqlite_") != 0) {
            std::cout << name << " ";
        }
    }
    std::cout << std::endl;
}

// ----------------------------------------------------------------------------
// Handle Count - Table vich kitne rows ne
// (How many rows are in the table)
// ----------------------------------------------------------------------------
void handleCount(Database& db, Schema& schema, const std::string& table_name) {
    // Table definition dhundho
    // (Find the table definition)
    const TableDefinition* table_def = schema.getTableDefinition(table_name);
    
    if (!table_def) {
        // Table nahi mili
        // (Table not found)
        std::cerr << "Table not found: " << table_name << std::endl;
        return;
    }
    
    // B-tree scan karke count kar
    // (Scan the B-tree and count)
    BTree tree(db, table_def->root_page);
    std::cout << tree.countRecords() << std::endl;
}

// ----------------------------------------------------------------------------
// Handle Select - Query execute kar te results dikha
// (Execute the query and show the results)
// ----------------------------------------------------------------------------
void handleSelect(Database& db, Schema& schema, const std::string& sql) {
    // Table definitions chaiye
    // (Table definitions are needed)
    schema.load();
    
    // Special case - COUNT(*) direct handle karo
    // (Special case: handle COUNT(*) directly)
    std::string sql_upper = sql;
    std::transform(sql_upper.begin(), sql_upper.end(), sql_upper.begin(), ::toupper);
    
    if (sql_upper.find("COUNT(*)") != std::string::npos) {
        // COUNT query aa - table naam dhundho
        // (It is a COUNT query - find the table name)
        size_t from_pos = sql_upper.find("FROM");
        if (from_pos != std::string::npos) {
            std::string table_part = sql.substr(from_pos + 4);
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
    
    // SQL parse kar
    // (Parse the SQL)
    SqlParser parser;
    auto query = parser.parseSelect(sql);
    
    if (!query) {
        // Query parse nahi hoyi
        // (The query did not parse)
        std::cerr << "Parse error: " << parser.getError() << std::endl;
        return;
    }
    
    // Query executor bana te execute kar
    // (Build the query executor and run it)
    QueryExecutor executor(db, schema);
    
    try {
        QueryResult result = executor.execute(*query);
        
        // Results print kar - har row
        // (Print the results - every row)
        for (const auto& row : result.rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) std::cout << "|";
                std::cout << row[i];
            }
            std::cout << std::endl;
        }
        
    } catch (const std::exception& e) {
        // Koi error aayi - user nu dassa
        // (Something went wrong - tell the user)
        std::cerr << "Query execution error: " << e.what() << std::endl;
    }
}

// ----------------------------------------------------------------------------
// Build Registry - Registry bana ke ikshar de
// (Build the registry and hand it out)
// ----------------------------------------------------------------------------
// Iksar da map - har call te nahi banna. (A single map - not rebuilt per call.)
// Rule 5: adding a command is adding one key here, not editing a control flow chain.
const CommandRegistry& buildRegistry() {
    static const CommandRegistry registry = {
        {".dbinfo", handleDbInfo},
        {".tables", handleTables},
        {"select", handleSelect},
    };
    
    return registry;
}

} // namespace

// ----------------------------------------------------------------------------
// Command Registry Access
// ----------------------------------------------------------------------------
const CommandRegistry& commandRegistry() {
    return buildRegistry();
}

// ----------------------------------------------------------------------------
// Command Names - Naam de list, user te message lai
// (The list of names, for the usage message)
// ----------------------------------------------------------------------------
std::vector<std::string> commandNames() {
    std::vector<std::string> names;
    
    for (const auto& entry : commandRegistry()) {
        names.push_back(entry.first);
    }
    
    // Har baar same order - unordered_map de order nahi da hunda
    // (Same order every time - unordered_map has no order)
    std::sort(names.begin(), names.end());
    
    return names;
}

} // namespace sqlite
