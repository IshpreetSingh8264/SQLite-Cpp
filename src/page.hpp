#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <memory>

// ============================================================================
// PAGE.HPP - SQLite Page Handler
// ============================================================================
// Oye page.hpp vich assi database de pages handle karde aa!
// (Hey in page.hpp we handle database pages!)
//
// SQLite vich saara data pages vich aa, te har page da apna type hai
// (In SQLite all data is in pages, and each page has its own type)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Page Types - Database vich kitne tarah de pages hunde ne
// (How many types of pages are there in database)
// ----------------------------------------------------------------------------
enum class PageType : uint8_t {
    INTERIOR_INDEX = 0x02,      // Interior index b-tree page - beech wala index page (middle index page)
    INTERIOR_TABLE = 0x05,      // Interior table b-tree page - beech wala table page (middle table page)
    LEAF_INDEX = 0x0a,          // Leaf index b-tree page - index da patta (index leaf)
    LEAF_TABLE = 0x0d,          // Leaf table b-tree page - table da patta, asli data yahan (table leaf, real data here)
    UNKNOWN = 0x00              // Unknown - pata nahi ki aa, galat page (don't know what it is, wrong page)
};

// ----------------------------------------------------------------------------
// Page Header Size Constants - Header kitna wadda hunda different types lai
// (How big the header is for different types)
// ----------------------------------------------------------------------------
constexpr size_t LEAF_PAGE_HEADER_SIZE = 8;        // Leaf page header - 8 bytes (leaf page header)
constexpr size_t INTERIOR_PAGE_HEADER_SIZE = 12;   // Interior page header - 12 bytes (interior page header)

// Page header offsets - Header vich kithon ki value hai
// (Where each value is in the header)
constexpr size_t OFFSET_PAGE_TYPE = 0;              // Page type - pehli byte (first byte)
constexpr size_t OFFSET_FREEBLOCK_START = 1;        // First freeblock - khali jagah kithon shuru (where free space starts)
constexpr size_t OFFSET_CELL_COUNT = 3;             // Number of cells - kine cells ne (how many cells)
constexpr size_t OFFSET_CELL_CONTENT_START = 5;     // Cell content area start
constexpr size_t OFFSET_FRAGMENTED_BYTES = 7;       // Fragmented free bytes - tukde tukde khali bytes (fragmented free bytes)
constexpr size_t OFFSET_RIGHT_POINTER = 8;          // Right-most pointer (interior pages only)

// ----------------------------------------------------------------------------
// Page Class - Ek page di saari information rakhda
// (Keeps all information of one page)
// ----------------------------------------------------------------------------
class Page {
public:
    // Constructor - Page bana te initialize kar
    // (Create and initialize page)
    Page(uint32_t page_number, const std::vector<uint8_t>& data, uint16_t page_size);
    
    // Getters - Page bare info kadho
    // (Get info about page)
    PageType getType() const { return type_; }                      // Page da type ki hai (what is page type)
    uint32_t getPageNumber() const { return page_number_; }         // Page number kitna (what page number)
    uint16_t getCellCount() const { return cell_count_; }           // Kine cells ne (how many cells)
    uint16_t getCellContentStart() const { return cell_content_start_; } // Content kithon shuru (where content starts)
    bool isLeaf() const;                                            // Leaf page hai ya nahi (is it leaf page)
    bool isInterior() const;                                        // Interior page hai ya nahi (is it interior page)
    
    // Cell pointers kadho - Cell kithon shuru hunde ne page vich
    // (Get cell pointers - where cells start in page)
    std::vector<uint16_t> getCellPointers() const;
    
    // Cell payload kadho - Cell da asli data nikalo
    // (Get cell payload - extract real data of cell)
    std::vector<uint8_t> getCellPayload(uint16_t cell_index) const;
    
    // Right-most child pointer (interior pages lai)
    // (Right-most child pointer (for interior pages))
    uint32_t getRightmostPointer() const { return rightmost_pointer_; }
    
    // Raw data access - Sidha page da data chaiye te
    // (Direct data access - if you need page data directly)
    const std::vector<uint8_t>& getData() const { return data_; }
    
    // Helper - Varint decode karo (variable length integer)
    // (Helper - decode varint (variable length integer))
    // SQLite vich numbers flexible size de hunde ne, space bachane lai
    // (In SQLite numbers are flexible size, to save space)
    static uint64_t readVarint(const uint8_t* data, size_t& bytes_read);
    
    // Safe version with bounds checking
    static uint64_t readVarintSafe(const uint8_t* data, size_t max_len, size_t& bytes_read);
    
private:
    // Helper methods - Andar de kaam (Internal work)
    void parseHeader();                     // Header parse karo (parse header)
    
    // Member variables - Page de andar da data
    // (Data inside page)
    uint32_t page_number_;                  // Page number - kaunsa page aa (which page is it)
    std::vector<uint8_t> data_;             // Page da saara data (all page data)
    uint16_t page_size_;                    // Page size
    
    // Header fields - Header vichon kadhi info
    // (Info extracted from header)
    PageType type_;                         // Page type - leaf, interior, index, table
    uint16_t freeblock_start_;              // Free block start
    uint16_t cell_count_;                   // Kine cells ne (how many cells)
    uint16_t cell_content_start_;           // Content kithon shuru (where content starts)
    uint8_t fragmented_bytes_;              // Fragmented bytes - waste space (waste space)
    uint32_t rightmost_pointer_;            // Right-most pointer (interior pages only)
};

// ----------------------------------------------------------------------------
// Helper Functions - Chhote chhote kaam lai (For small tasks)
// ----------------------------------------------------------------------------

// Big endian read karo - SQLite sab kuch big endian vich rakhda
// (Read big endian - SQLite keeps everything in big endian)
// Definitions live in page.cpp - this header holds contracts only.
// (Definitions live in page.cpp - this header holds contracts only.)
uint16_t readBigEndian16(const uint8_t* data);
uint32_t readBigEndian32(const uint8_t* data);

// String representation for debugging - Debug karde waqt page type print karo
// (Print page type when debugging)
std::string pageTypeToString(PageType type);

} // namespace sqlite
