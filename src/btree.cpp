#include "btree.hpp"
#include <stdexcept>

// ============================================================================
// BTREE.CPP - SQLite B-Tree Navigator Implementation
// ============================================================================
// Oye btree.cpp vich assi B-tree navigate karde te data dhundhde aa!
// (Hey in btree.cpp we navigate B-tree and find data!)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Constructor - B-tree setup karo
// (Setup B-tree)
// ----------------------------------------------------------------------------
BTree::BTree(Database& database, uint32_t root_page_number)
    : database_(database)
    , root_page_number_(root_page_number)
{
    // Root page validate karo
    // (Validate root page)
    if (root_page_number == 0 || root_page_number > database.getPageCount()) {
        throw std::runtime_error("Invalid root page number yaar!");
        // (Invalid root page number dude!)
    }
}

// ----------------------------------------------------------------------------
// Scan All - Saare records scan karo pura tree
// (Scan all records in entire tree)
// ----------------------------------------------------------------------------
// Root ton shuru karke saare pages visit kar
// (Start from root and visit all pages)
void BTree::scanAll(std::function<void(const Record&)> callback) {
    // Root page padho
    // (Read root page)
    auto page_data = database_.readPage(root_page_number_);
    Page root_page(root_page_number_, page_data, database_.getPageSize());
    
    if (root_page.isLeaf()) {
        // Leaf page aa - sidha scan kar
        // (It's leaf page - scan directly)
        scanLeafPage(root_page_number_, callback);
    } else {
        // Interior page aa - children ko scan kar
        // (It's interior page - scan children)
        scanInteriorPage(root_page_number_, callback);
    }
}

// ----------------------------------------------------------------------------
// Scan Leaf Page - Leaf page de saare cells scan karo
// (Scan all cells of leaf page)
// ----------------------------------------------------------------------------
// Leaf vich asli data aa - har cell ek record
// (Leaf has real data - each cell is one record)
void BTree::scanLeafPage(uint32_t page_number, std::function<void(const Record&)> callback) {
    // Page padho
    // (Read page)
    auto page_data = database_.readPage(page_number);
    Page page(page_number, page_data, database_.getPageSize());
    
    if (!page.isLeaf()) {
        throw std::runtime_error("Oye ye leaf page nahi aa!");
        // (Hey this is not a leaf page!)
    }
    
    // Saare cells process karo
    // (Process all cells)
    uint16_t cell_count = page.getCellCount();
    
    for (uint16_t i = 0; i < cell_count; ++i) {
        try {
            // Cell payload kadho
            // (Get cell payload)
            auto cell_payload = page.getCellPayload(i);
            
            // Record decode karo
            // (Decode record)
            Record record = decodeTableLeafCell(cell_payload);
            
            // Callback call kar - user nu record de
            // (Call callback - give record to user)
            callback(record);
        } catch (const std::exception& e) {
            // Koi cell decode nahi hoyi - skip kar te agle te jao
            // (Some cell didn't decode - skip and go to next)
            continue;
        }
    }
}

// ----------------------------------------------------------------------------
// Scan Interior Page - Interior page de children scan karo
// (Scan children of interior page)
// ----------------------------------------------------------------------------
// Interior page pointers rakhda children page nu
// (Interior page keeps pointers to children pages)
void BTree::scanInteriorPage(uint32_t page_number, std::function<void(const Record&)> callback) {
    // Page padho
    // (Read page)
    auto page_data = database_.readPage(page_number);
    Page page(page_number, page_data, database_.getPageSize());
    
    if (!page.isInterior()) {
        throw std::runtime_error("Oye ye interior page nahi aa!");
        // (Hey this is not an interior page!)
    }
    
    // Cell pointers kadho - har cell left child nu point karda
    // (Get cell pointers - each cell points to left child)
    auto cell_pointers = page.getCellPointers();
    
    // Saare left children scan karo
    // (Scan all left children)
    for (size_t i = 0; i < cell_pointers.size(); ++i) {
        try {
            auto cell_payload = page.getCellPayload(i);
            
            // Left child page number kadho - pehle 4 bytes vich
            // (Get left child page number - in first 4 bytes)
            uint32_t left_child = getLeftChildPointer(cell_payload);
            
            // Child page padho
            // (Read child page)
            auto child_data = database_.readPage(left_child);
            Page child_page(left_child, child_data, database_.getPageSize());
            
            // Recursive scan - child leaf aa ya interior
            // (Recursive scan - child is leaf or interior)
            if (child_page.isLeaf()) {
                scanLeafPage(left_child, callback);
            } else {
                scanInteriorPage(left_child, callback);
            }
        } catch (const std::exception& e) {
            // Error aayi - skip kar (error occurred - skip)
            continue;
        }
    }
    
    // Right-most child vi scan karo - last vich
    // (Also scan right-most child - at the end)
    uint32_t rightmost = page.getRightmostPointer();
    if (rightmost > 0 && rightmost <= database_.getPageCount()) {
        try {
            auto child_data = database_.readPage(rightmost);
            Page child_page(rightmost, child_data, database_.getPageSize());
            
            if (child_page.isLeaf()) {
                scanLeafPage(rightmost, callback);
            } else {
                scanInteriorPage(rightmost, callback);
            }
        } catch (const std::exception& e) {
            // Skip on error
        }
    }
}

// ----------------------------------------------------------------------------
// Decode Table Leaf Cell - Cell payload vichon record bana
// (Create record from cell payload)
// ----------------------------------------------------------------------------
Record BTree::decodeTableLeafCell(const std::vector<uint8_t>& cell_data) {
    if (cell_data.empty()) {
        throw std::runtime_error("Empty cell data!");
    }
    
    const uint8_t* data = cell_data.data();
    size_t offset = 0;
    size_t bytes_read = 0;
    
    // Cell format for table leaf:
    // - payload_size (varint)
    // - rowid (varint)
    // - payload (record data)
    
    // Skip payload size - getCellPayload gives us everything
    // (Skip payload size - getCellPayload gives us everything)
    uint64_t payload_size = readVarint(data + offset, bytes_read);
    offset += bytes_read;
    
    // Rowid padho - leaf table cell vich hamesha hunda
    // (Read rowid - always in leaf table cell)
    uint64_t rowid = readVarint(data + offset, bytes_read);
    offset += bytes_read;
    
    // Baaki data record aa - decode karo
    // (Remaining data is record - decode it)
    std::vector<uint8_t> record_payload(data + offset, data + cell_data.size());
    
    Record record;
    if (!record.decode(record_payload)) {
        throw std::runtime_error("Failed to decode record!");
    }
    
    record.setRowId(static_cast<int64_t>(rowid));
    
    return record;
}

// ----------------------------------------------------------------------------
// Get Left Child Pointer - Interior cell vichon left child kadho
// (Get left child from interior cell)
// ----------------------------------------------------------------------------
uint32_t BTree::getLeftChildPointer(const std::vector<uint8_t>& cell_data) {
    if (cell_data.size() < 4) {
        throw std::runtime_error("Cell data too small for pointer!");
    }
    
    // Pehle 4 bytes left child page number ne (big endian)
    // (First 4 bytes are left child page number (big endian))
    const uint8_t* data = cell_data.data();
    
    uint32_t pointer = (static_cast<uint32_t>(data[0]) << 24) |
                      (static_cast<uint32_t>(data[1]) << 16) |
                      (static_cast<uint32_t>(data[2]) << 8) |
                      data[3];
    
    return pointer;
}

// ----------------------------------------------------------------------------
// Count Records - Kitne records ne total tree vich
// (How many records total in tree)
// ----------------------------------------------------------------------------
size_t BTree::countRecords() {
    size_t count = 0;
    
    // Saare records scan kar te count kar
    // (Scan all records and count)
    scanAll([&count](const Record&) {
        count++;
    });
    
    return count;
}

// ----------------------------------------------------------------------------
// Find By Key - Specific key wala record dhundho
// (Find record with specific key)
// ----------------------------------------------------------------------------
// Binary search use karde - sorted tree aa
// (Use binary search - it's sorted tree)
std::optional<Record> BTree::findByKey(int64_t key) {
    // Simple implementation - scan karke match dhundho
    // (Simple implementation - scan and find match)
    // Production vich binary search use karna chaiye
    // (In production should use binary search)
    
    std::optional<Record> result;
    
    scanAll([&](const Record& record) {
        if (record.getRowId() == key) {
            result = record;
        }
    });
    
    return result;
}

// ============================================================================
// Index BTree Implementation
// ============================================================================

// ----------------------------------------------------------------------------
// Constructor - Index B-tree setup
// (Setup index B-tree)
// ----------------------------------------------------------------------------
IndexBTree::IndexBTree(Database& database, uint32_t root_page_number)
    : database_(database)
    , root_page_number_(root_page_number)
{
}

// ----------------------------------------------------------------------------
// Find Row IDs - Index use karke rowids dhundho
// (Find rowids using index)
// ----------------------------------------------------------------------------
std::vector<int64_t> IndexBTree::findRowIds(const std::string& key) {
    std::vector<int64_t> rowids;
    
    // Saare index entries scan kar
    // (Scan all index entries)
    scanAll([&](const Record& record) {
        // Pehla column index key aa
        // (First column is index key)
        auto index_key = record.getString(0);
        
        if (index_key && *index_key == key) {
            // Match mileya - rowid kadho (second column vich)
            // (Found match - get rowid (in second column))
            auto rowid = record.getInt(1);
            if (rowid) {
                rowids.push_back(*rowid);
            }
        }
    });
    
    return rowids;
}

// ----------------------------------------------------------------------------
// Scan All - Saari index entries scan karo
// (Scan all index entries)
// ----------------------------------------------------------------------------
void IndexBTree::scanAll(std::function<void(const Record&)> callback) {
    // Root page padho
    // (Read root page)
    auto page_data = database_.readPage(root_page_number_);
    Page root_page(root_page_number_, page_data, database_.getPageSize());
    
    if (root_page.isLeaf()) {
        scanIndexLeafPage(root_page_number_, callback);
    } else {
        scanIndexInteriorPage(root_page_number_, callback);
    }
}

// ----------------------------------------------------------------------------
// Scan Index Leaf Page - Index leaf scan karo
// (Scan index leaf)
// ----------------------------------------------------------------------------
void IndexBTree::scanIndexLeafPage(uint32_t page_number, std::function<void(const Record&)> callback) {
    auto page_data = database_.readPage(page_number);
    Page page(page_number, page_data, database_.getPageSize());
    
    uint16_t cell_count = page.getCellCount();
    
    for (uint16_t i = 0; i < cell_count; ++i) {
        try {
            auto cell_payload = page.getCellPayload(i);
            Record record = decodeIndexLeafCell(cell_payload);
            callback(record);
        } catch (const std::exception&) {
            continue;
        }
    }
}

// ----------------------------------------------------------------------------
// Scan Index Interior Page - Index interior scan karo
// (Scan index interior)
// ----------------------------------------------------------------------------
void IndexBTree::scanIndexInteriorPage(uint32_t page_number, std::function<void(const Record&)> callback) {
    auto page_data = database_.readPage(page_number);
    Page page(page_number, page_data, database_.getPageSize());
    
    // Similar to table interior - children scan karo
    // (Similar to table interior - scan children)
    auto cell_pointers = page.getCellPointers();
    
    for (size_t i = 0; i < cell_pointers.size(); ++i) {
        try {
            auto cell_payload = page.getCellPayload(i);
            
            // Left child
            const uint8_t* data = cell_payload.data();
            uint32_t left_child = (static_cast<uint32_t>(data[0]) << 24) |
                                 (static_cast<uint32_t>(data[1]) << 16) |
                                 (static_cast<uint32_t>(data[2]) << 8) |
                                 data[3];
            
            auto child_data = database_.readPage(left_child);
            Page child_page(left_child, child_data, database_.getPageSize());
            
            if (child_page.isLeaf()) {
                scanIndexLeafPage(left_child, callback);
            } else {
                scanIndexInteriorPage(left_child, callback);
            }
        } catch (const std::exception&) {
            continue;
        }
    }
    
    // Rightmost child
    uint32_t rightmost = page.getRightmostPointer();
    if (rightmost > 0) {
        try {
            auto child_data = database_.readPage(rightmost);
            Page child_page(rightmost, child_data, database_.getPageSize());
            
            if (child_page.isLeaf()) {
                scanIndexLeafPage(rightmost, callback);
            } else {
                scanIndexInteriorPage(rightmost, callback);
            }
        } catch (const std::exception&) {
        }
    }
}

// ----------------------------------------------------------------------------
// Decode Index Leaf Cell - Index cell decode karo
// (Decode index cell)
// ----------------------------------------------------------------------------
Record IndexBTree::decodeIndexLeafCell(const std::vector<uint8_t>& cell_data) {
    // Index leaf cell format:
    // - payload_size (varint)
    // - payload (record data - no rowid)
    
    if (cell_data.empty()) {
        throw std::runtime_error("Empty index cell data!");
    }
    
    const uint8_t* data = cell_data.data();
    size_t offset = 0;
    size_t bytes_read = 0;
    
    // Skip payload size
    // (Skip payload size)
    uint64_t payload_size = readVarint(data + offset, bytes_read);
    offset += bytes_read;
    
    // Rest is record payload
    // (Rest is record payload)
    std::vector<uint8_t> record_payload(data + offset, data + cell_data.size());
    
    Record record;
    if (!record.decode(record_payload)) {
        throw std::runtime_error("Failed to decode index record!");
    }
    
    return record;
}

} // namespace sqlite
