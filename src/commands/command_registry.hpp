#pragma once

#include "database.hpp"
#include "schema.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// COMMANDS/COMMAND_REGISTRY.HPP - The Command Table
// ============================================================================
// Assi teen commands di facility dende aa: `.dbinfo`, `.tables`, te `SELECT ...`.
// (We offer three commands: `.dbinfo`, `.tables` and `SELECT ...`.)
//
// Pehlan `main.cpp` vich ek if/else ladder si. Naal command jodna matlab ladder
// vich aur haath da rakhna si.
// (There used to be an if/else ladder inside main.cpp. Adding a command meant
// editing that ladder.)
//
// Hade oh ek registry aa - data, control flow nahi. Ek command jodna matlab ek
// entry jodna.
// (Now it is a registry - data, not control flow. Adding a command means adding
// one entry.)
//
// Rule 6: har command di signature EK hi aa, isliye dispatch vich koi special case
// nahi. (Rule 6: every command has the SAME signature, so dispatch has no
// special cases.)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Command Handler - Ek command di shape, sab de ek jaisi
// (The shape of a command - identical for all of them)
// ----------------------------------------------------------------------------
// 1. Database te Schema di references - ki ehna nu padhna aa
// (1. References to Database and Schema - what they need to read)
// 2. Command text - `.tables` ya poora SELECT statement
// (2. The command text - `.tables` or the whole SELECT statement)
using CommandHandler = std::function<void(Database&, Schema&, const std::string&)>;

// ----------------------------------------------------------------------------
// Command Registry - Command naam -> handler
// ----------------------------------------------------------------------------
using CommandRegistry = std::unordered_map<std::string, CommandHandler>;

// ----------------------------------------------------------------------------
// Command Registry Access - Iksar da registry, build vich aik hi vaar
// (The registry itself, built exactly once)
// ----------------------------------------------------------------------------
const CommandRegistry& commandRegistry();

// ----------------------------------------------------------------------------
// Command Names - Registry vich kya kya aa, '.tables' chhad ke
// (What the registry holds, excluding the implicit `.tables` listing)
// ----------------------------------------------------------------------------
std::vector<std::string> commandNames();

} // namespace sqlite
