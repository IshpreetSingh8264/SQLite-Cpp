// ============================================================================
// MAIN.CPP - SQLite Database Engine Entry Point
// ============================================================================
// Oye file sirf wiring kardi hai: arguments padho, database kholo, command
// chuno, chala do. Koi domain logic nahi - oh `commands/` te modules vich hai.
// (This file only wires things up: read the arguments, open the database, pick a
// command, run it. No domain logic - that lives in the modules under
// `commands/`.)
//
//   main.cpp ──▶ commands/command_registry ──▶ query_executor ──▶ schema / btree
//                                                                    │
//                                                         utils/ (value_compare, like)
//                                                                    │
//                                                         page ──▶ database
// ============================================================================

#include "database.hpp"
#include "schema.hpp"
#include "commands/command_registry.hpp"
#include "utils/diagnostics.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

// ----------------------------------------------------------------------------
// Is Select - Kya eh command SELECT query aa?
// (Is this command a SELECT query?)
// ----------------------------------------------------------------------------
bool isSelect(const std::string& command) {
    if (command.size() < 6) {
        return false;
    }
    
    return std::equal(command.begin(), command.begin() + 6, "select",
                      [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) == b;
                      });
}

// ----------------------------------------------------------------------------
// Print Usage - Galat invocation lai
// (For an invalid invocation)
// ----------------------------------------------------------------------------
void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " <database_file> <command>" << std::endl;
    std::cerr << "Supported commands:" << std::endl;
    std::cerr << "  .dbinfo          - database page size te table count" << std::endl;
    std::cerr << "  .tables          - saare user tables de naam" << std::endl;
    std::cerr << "  SELECT ...       - SQL query execute karo" << std::endl;
}

// ----------------------------------------------------------------------------
// Main - Program di entry point
// (The program's entry point)
// ----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Output buffering band kar - turant results dikhan
    // (Turn off output buffering - show results immediately)
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    std::cerr << "Logs from your program will appear here" << std::endl;
    
    // Do arguments chahide - database file te command
    // (Two arguments are needed: the database file and the command)
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    const std::string database_file_path = argv[1];
    const std::string command = argv[2];
    
    // Command chuno - registry vich dekho, SELECT query layi registry vich
    // "select" de jagah poora statement chahida.
    // (Pick the command: look in the registry, except that for a SELECT query
    // the registry's "select" entry is given the whole statement.)
    std::string key = command;
    if (!sqlite::commandRegistry().count(key) && isSelect(command)) {
        key = "select";
    }
    
    auto entry = sqlite::commandRegistry().find(key);
    if (entry == sqlite::commandRegistry().end()) {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    try {
        sqlite::Database db(database_file_path);
        
        if (!db.isValid()) {
            std::cerr << "Database error: " << db.getError() << std::endl;
            return 1;
        }
        
        sqlite::Schema schema(db);
        
        entry->second(db, schema, command);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        // Ki aaya si oh dass de, ohde naal
        // (Also report what we collected on the way)
        sqlite::printSummary();
        return 1;
    }
    
    // Agar kade kade koi cell decode nahi hoyi te user nu pata lage
    // (If any cell failed to decode, make sure the user finds out)
    sqlite::printSummary();
    
    return 0;
}
