#include "btree.hpp"
#include <stdexcept>

// ============================================================================
// INDEX_BTREE.CPP - SQLite Index B-Tree Navigator Implementation
// ============================================================================
// Index B-tree di alag format hundi - key + rowid, table wala data nahi.
// (An index B-tree has a different format - key + rowid, not table data.)
// Iss file vich table wala code ton alag rakha gaya - ek concept, ek file.
// (Kept separate from the table B-tree code - one concept, one file.)
// ============================================================================

namespace sqlite {

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
    
    // Index scan karke matching entries dhundho
    // (Scan index to find matching entries)
    // Index leaf pages vich [key, rowid] stored hunda
    // (In index leaf pages, [key, rowid] is stored)
    findRowIdsInPage(root_page_number_, key, rowids);
    
    return rowids;
}

// ----------------------------------------------------------------------------
// Find Row IDs In Page - Specific page vich key dhundho
// (Find key in specific page)
// ----------------------------------------------------------------------------
void IndexBTree::findRowIdsInPage(uint32_t page_number, const std::string& key, std::vector<int64_t>& rowids) {
    auto page_data = database_.readPage(page_number);
    Page page(page_number, page_data, database_.getPageSize());
    
    uint16_t cell_count = page.getCellCount();
    
    if (page.isLeaf()) {
        // Index leaf page - cells vich key+rowid ne
        // (Index leaf page - cells have key+rowid)
        
        for (uint16_t i = 0; i < cell_count; ++i) {
            try {
                auto cell_payload = page.getCellPayload(i);
                Record record = decodeIndexLeafCell(cell_payload);
                
                // Pehla column index key aa
                // (First column is index key)
                auto index_key = record.getString(0);
                if (!index_key) continue;
                
                size_t last_col = record.getColumnCount() - 1;
                auto rowid = record.getInt(last_col);
                if (!rowid) continue;
                
                if (*index_key == key) {
                    rowids.push_back(*rowid);
                }
            } catch (const std::exception&) {
                continue;
            }
        }
    } else {
        // Index interior page - use B-tree navigation to find relevant children
        // (Index interior page - use B-tree navigation to find relevant children)
        // IMPORTANT: Index entries can be stored in interior cells (not just leaf)!
        
        // Each cell has: [left_child] [key]
        // Cell[i].key is the FIRST key in cell[i+1].left_child (or rightmost)
        // So: entries in cell[i].left_child have keys < cell[i].key
        //     entries in cell[i+1].left_child have keys >= cell[i].key and < cell[i+1].key
        
        // Find range of children that might contain our key
        // We need to visit children where key could be present
        bool found_start = false;
        bool passed_end = false;
        
        for (uint16_t i = 0; i < cell_count && !passed_end; ++i) {
            try {
                auto cell_payload = page.getCellPayload(i);
                
                if (cell_payload.size() < 4) {
                    continue;
                }
                
                // Index interior cell: [left_child (4 bytes)] [payload_size] [payload]
                const uint8_t* data = cell_payload.data();
                
                uint32_t left_child = (static_cast<uint32_t>(data[0]) << 24) |
                                     (static_cast<uint32_t>(data[1]) << 16) |
                                     (static_cast<uint32_t>(data[2]) << 8) |
                                     data[3];
                
                // Decode cell key
                std::string cell_key_str;
                if (cell_payload.size() > 4) {
                    std::vector<uint8_t> payload_data(cell_payload.begin() + 4, cell_payload.end());
                    try {
                        Record cell_record = decodeIndexLeafCell(payload_data);
                        auto cell_key = cell_record.getString(0);
                        if (cell_key) {
                            cell_key_str = *cell_key;
                            
                            // Check for exact match in interior cell
                            if (cell_key_str == key) {
                                size_t last_col = cell_record.getColumnCount() - 1;
                                auto cell_rowid = cell_record.getInt(last_col);
                                if (cell_rowid) {
                                    rowids.push_back(*cell_rowid);
                                }
                            }
                        }
                    } catch (...) {
                    }
                }
                
                // Determine if we should visit left_child
                // left_child contains entries < cell_key
                // So we visit left_child if: key < cell_key OR key == cell_key (for prefix match)
                if (!found_start && key <= cell_key_str) {
                    // First cell where our key could be in left subtree
                    found_start = true;
                }
                
                if (found_start) {
                    // Visit this child - it might contain matching entries
                    findRowIdsInPage(left_child, key, rowids);
                    
                    // If cell's key > our key, we've passed all possible matches
                    // But we still need to visit children that could have exact matches
                    if (cell_key_str > key) {
                        // After this point, no more matches possible in further children
                        // (since all keys in subsequent children are >= cell_key > key)
                        passed_end = true;
                    }
                }
            } catch (const std::exception&) {
                continue;
            }
        }
        
        // Right-most child vi visit karo if we haven't passed the end
        // (Visit right-most child too if we haven't passed the end)
        if (!passed_end) {
            uint32_t rightmost = page.getRightmostPointer();
            if (rightmost > 0 && rightmost <= database_.getPageCount()) {
                findRowIdsInPage(rightmost, key, rowids);
            }
        }
    }
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
    
    // Read payload size
    // (Read payload size)
    uint64_t payload_size = readVarint(data + offset, bytes_read);
    offset += bytes_read;
    
    // Validate bounds
    // (Validate bounds)
    if (offset + payload_size > cell_data.size()) {
        payload_size = cell_data.size() - offset;
    }
    
    // Extract record payload
    // (Extract record payload)
    std::vector<uint8_t> record_payload(data + offset, data + offset + static_cast<size_t>(payload_size));
    
    Record record;
    if (!record.decode(record_payload)) {
        throw std::runtime_error("Failed to decode index record!");
    }
    
    return record;
}

} // namespace sqlite
