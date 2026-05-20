#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// ============================================================================
// UTILS/DIAGNOSTICS.HPP - Cross-Cutting Error Reporting
// ============================================================================
// B-tree traversal niyan vich kade kade ek cell decode nahi hundi (corrupt page,
// truncated file, bad page number). Pehlan oh error chhupayi jandi si - row gayab
// ho jandi si te kuch pata hi nahi chalda.
// (While walking a B-tree a cell sometimes fails to decode - corrupt page,
// truncated file, bad page number. That error used to vanish: the row disappeared
// and nobody could tell why.)
//
// Hade oh har jagah report hundi hai, te bade har jagah? (Now it is reported
// everywhere, with the page number, the cell index, and the message.)
//
// Sab kuch STDERR te janda hai - STDOUT saaf rehna chahida, kyunki usde hi
// CodeCrafters harness results padhde aa. (Everything goes to STDERR - STDOUT must
// stay clean, because that is where the CodeCrafters harness reads results.)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Severity - Kitna serious hai error (How serious the problem is)
// ----------------------------------------------------------------------------
enum class Severity {
    Warning,   // Data mili par adhoori - e.g. ek cell skip ho gayi
                // (Got data but degraded - e.g. one cell was skipped)
    Error      // Kuch recover nahi hunda - caller nu loop chhorni chahidi
                // (Not recoverable - the caller must give up)
};

// ----------------------------------------------------------------------------
// Report - Ek diagnostic message STDERR te likho
// (Write one diagnostic message to STDERR)
// ----------------------------------------------------------------------------
// `context` vich likhde aa ki kithon error aaya (page number, cell index, method)
// (`context` says where the error came from: page number, cell index, method)
// `detail` vich asli reason (asli reason)
void report(Severity severity, const std::string& context, const std::string& detail);

// ----------------------------------------------------------------------------
// Cell Context - "page N, cell M, in <where>" label bana do
// (Build a "page N, cell M, in <where>" label)
// ----------------------------------------------------------------------------
// Formatting ikhde hi hoyi hai taki har call site naal apna text na likhna paye
// (Formatting lives here so no call site has to write its own label)
std::string cellContext(uint32_t page_number, uint16_t cell_index, const std::string& where);

// ----------------------------------------------------------------------------
// Page Context - "page N, in <where>" label bana do
// (Build a "page N, in <where>" label)
// ----------------------------------------------------------------------------
std::string pageContext(uint32_t page_number, const std::string& where);

// ----------------------------------------------------------------------------
// Diagnostic Count - Ethe tak kitne diagnostics report hoye
// (How many diagnostics have been reported so far)
// ----------------------------------------------------------------------------
size_t diagnosticCount();

// Warning Count sirf - Errors ohde ton alag count hoye (Warnings only, errors counted separately)
size_t warningCount();

// ----------------------------------------------------------------------------
// Print Summary - Saare diagnostics da ek chhota sa summary STDERR te
// (A one-line summary of everything reported, on STDERR)
// ----------------------------------------------------------------------------
void printSummary();

} // namespace sqlite
