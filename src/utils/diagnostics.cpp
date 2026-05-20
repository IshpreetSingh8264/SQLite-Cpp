#include "diagnostics.hpp"
#include <iostream>

// ============================================================================
// UTILS/DIAGNOSTICS.CPP - Cross-Cutting Error Reporting Implementation
// ============================================================================

namespace sqlite {

namespace {

// Counters - process de lifetime toh jama hoye (Accumulate for the process lifetime)
size_t g_warning_count = 0;
size_t g_error_count = 0;

const char* severityLabel(Severity severity) {
    return severity == Severity::Error ? "error" : "warning";
}

} // namespace

// ----------------------------------------------------------------------------
// Report - Diagnostic likhdo STDERR te
// (Write the diagnostic to STDERR)
// ----------------------------------------------------------------------------
void report(Severity severity, const std::string& context, const std::string& detail) {
    if (severity == Severity::Error) {
        g_error_count++;
    } else {
        g_warning_count++;
    }

    // cerr te likhde aa, stdout nahi - CodeCrafters results saaf rehne chahide
    // (Written to cerr, never stdout - CodeCrafters results must stay clean)
    std::cerr << "[sqlite] " << severityLabel(severity) << ": " << context;
    if (!detail.empty()) {
        std::cerr << ": " << detail;
    }
    std::cerr << std::endl;
}

// ----------------------------------------------------------------------------
// Cell Context - Page te cell di location dasso
// (Say which page and which cell)
// ----------------------------------------------------------------------------
std::string cellContext(uint32_t page_number, uint16_t cell_index, const std::string& where) {
    return "page " + std::to_string(page_number) + ", cell " + std::to_string(cell_index) +
           ", in " + where;
}

// ----------------------------------------------------------------------------
// Page Context - Page di location dasso
// (Say which page)
// ----------------------------------------------------------------------------
std::string pageContext(uint32_t page_number, const std::string& where) {
    return "page " + std::to_string(page_number) + ", in " + where;
}

// ----------------------------------------------------------------------------
// Diagnostic Count - Total diagnostics reported
// ----------------------------------------------------------------------------
size_t diagnosticCount() {
    return g_warning_count + g_error_count;
}

// ----------------------------------------------------------------------------
// Warning Count - Sirf warnings
// ----------------------------------------------------------------------------
size_t warningCount() {
    return g_warning_count;
}

// ----------------------------------------------------------------------------
// Print Summary - Ethe tak da haal dasso
// (Report the state so far)
// ----------------------------------------------------------------------------
void printSummary() {
    if (diagnosticCount() == 0) {
        return;
    }

    std::cerr << "[sqlite] summary: " << diagnosticCount() << " diagnostic(s) reported ("
              << warningCount() << " warning(s), " << g_error_count << " error(s))"
              << std::endl;
}

} // namespace sqlite
