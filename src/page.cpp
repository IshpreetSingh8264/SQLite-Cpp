#include "page.hpp"
#include <stdexcept>
#include <sstream>
#include <cstring>

// ============================================================================
// PAGE.CPP - SQLite Page Implementation
// ============================================================================
// Oye page.cpp vich assi pages parse karde te cells extract karde aa!
// (Hey in page.cpp we parse pages and extract cells!)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Constructor - Page bana te header parse kar
// (Create page and parse header)
// ----------------------------------------------------------------------------
Page::Page(uint32_t page_number, const std::vector<uint8_t>& data, uint16_t page_size)
    : page_number_(page_number)
    , data_(data)
    , page_size_(page_size)
    , type_(PageType::UNKNOWN)
    , freeblock_start_(0)
    , cell_count_(0)
    , cell_content_start_(0)
    , fragmented_bytes_(0)
    , rightmost_pointer_(0)
{
    // Header parse karo - page da type te cells di info kadho
    // (Parse header - get page type and cells info)
    parseHeader();
}

// ----------------------------------------------------------------------------
// Parse Header - Page header vichon saari info extract karo
// (Extract all info from page header)
// ----------------------------------------------------------------------------
void Page::parseHeader() {
    // Page 1 te database header vi aa - 100 bytes, toh page header baad vich
    // (Page 1 also has database header - 100 bytes, so page header after that)
    size_t header_offset = (page_number_ == 1) ? 100 : 0;
    
    if (data_.size() < header_offset + LEAF_PAGE_HEADER_SIZE) {
        // Data chhota aa - header hi nahi pad sakda!
        // (Data is small - can't even read header!)
        throw std::runtime_error("Page data too small hai yaar!");
        // (Page data is too small dude!)
    }
    
    const uint8_t* header = data_.data() + header_offset;
    
    // Page type - pehli byte (first byte)
    type_ = static_cast<PageType>(header[OFFSET_PAGE_TYPE]);
    
    // Freeblock start - byte 1-2
    freeblock_start_ = readBigEndian16(&header[OFFSET_FREEBLOCK_START]);
    
    // Cell count - byte 3-4 (kitne cells ne page vich)
    // (How many cells in page)
    cell_count_ = readBigEndian16(&header[OFFSET_CELL_COUNT]);
    
    // Cell content start - byte 5-6 (cells kithon shuru hunde)
    // (Where cells start)
    cell_content_start_ = readBigEndian16(&header[OFFSET_CELL_CONTENT_START]);
    
    // Agar 0 aa te page di end te (65536)
    // (If 0 then at end of page (65536))
    if (cell_content_start_ == 0) {
        cell_content_start_ = 65536;
    }
    
    // Fragmented bytes - byte 7
    fragmented_bytes_ = header[OFFSET_FRAGMENTED_BYTES];
    
    // Agar interior page aa te right-most pointer vi aa
    // (If interior page then right-most pointer also there)
    if (isInterior()) {
        rightmost_pointer_ = readBigEndian32(&header[OFFSET_RIGHT_POINTER]);
    }
}

// ----------------------------------------------------------------------------
// Is Leaf - Page leaf type da hai ya nahi
// (Is page of leaf type or not)
// ----------------------------------------------------------------------------
bool Page::isLeaf() const {
    // Leaf table ya leaf index - dono leaf ne
    // (Leaf table or leaf index - both are leaf)
    return type_ == PageType::LEAF_TABLE || type_ == PageType::LEAF_INDEX;
}

// ----------------------------------------------------------------------------
// Is Interior - Page interior type da hai ya nahi
// (Is page of interior type or not)
// ----------------------------------------------------------------------------
bool Page::isInterior() const {
    // Interior table ya interior index - dono interior ne
    // (Interior table or interior index - both are interior)
    return type_ == PageType::INTERIOR_TABLE || type_ == PageType::INTERIOR_INDEX;
}

// ----------------------------------------------------------------------------
// Get Cell Pointers - Saare cell pointers kadho
// (Get all cell pointers)
// ----------------------------------------------------------------------------
// Cell pointer array header de baad vich hundi - har cell 2 bytes
// (Cell pointer array is after header - each cell is 2 bytes)
std::vector<uint16_t> Page::getCellPointers() const {
    std::vector<uint16_t> pointers;
    pointers.reserve(cell_count_);
    
    // Header size calculate karo
    // (Calculate header size)
    size_t header_offset = (page_number_ == 1) ? 100 : 0;
    size_t header_size = isInterior() ? INTERIOR_PAGE_HEADER_SIZE : LEAF_PAGE_HEADER_SIZE;
    
    // Cell pointer array yahan ton shuru hundi
    // (Cell pointer array starts from here)
    size_t pointer_offset = header_offset + header_size;
    
    // Har cell da pointer read karo - 2 bytes each
    // (Read pointer of each cell - 2 bytes each)
    for (uint16_t i = 0; i < cell_count_; ++i) {
        if (pointer_offset + 2 > data_.size()) {
            // Data khatam hoyi - pointers incomplete ne
            // (Data ended - pointers are incomplete)
            break;
        }
        
        uint16_t pointer = readBigEndian16(&data_[pointer_offset]);
        pointers.push_back(pointer);
        pointer_offset += 2;
    }
    
    return pointers;
}

// ----------------------------------------------------------------------------
// Get Cell Payload - Cell da actual data kadho
// (Get actual data of cell)
// ----------------------------------------------------------------------------
std::vector<uint8_t> Page::getCellPayload(uint16_t cell_index) const {
    if (cell_index >= cell_count_) {
        // Index galat - itne cells hi nahi ne!
        // (Index wrong - there aren't that many cells!)
        throw std::out_of_range("Cell index out of range aa bai!");
        // (Cell index is out of range buddy!)
    }
    
    // Cell pointers kadho
    // (Get cell pointers)
    auto pointers = getCellPointers();
    
    if (cell_index >= pointers.size()) {
        throw std::out_of_range("Cell pointer not found yaar!");
        // (Cell pointer not found dude!)
    }
    
    uint16_t cell_offset = pointers[cell_index];
    
    if (cell_offset >= data_.size()) {
        throw std::runtime_error("Cell offset invalid hai - data vichon bahar!");
        // (Cell offset is invalid - outside data!)
    }
    
    // Cell da data extract karo
    // (Extract cell data)
    // Pehle cell vich structure aa - size depends on page type
    // (First cell has structure - size depends on page type)
    
    const uint8_t* cell_data = &data_[cell_offset];
    size_t bytes_read = 0;
    
    if (isInterior()) {
        // Interior cell format:
        // - 4 bytes: left child page number
        // - varint: key (for table lookup, not used in simple scan)
        // (Interior cell format: 4 bytes left child + key (varint))
        
        // For scanInteriorPage in btree.cpp, we only need the left child pointer
        // So just return those 4 bytes
        // (For scanning, we only need left child pointer)
        
        if (cell_offset + 4 > data_.size()) {
            throw std::runtime_error("Interior cell: cannot read left child pointer!");
        }
        
        // Return just the left child pointer (4 bytes)
        // The caller (getLeftChildPointer in btree.cpp) will extract it
        // (Return just left child pointer for btree scanning)
        return std::vector<uint8_t>(&data_[cell_offset], &data_[cell_offset + 4]);
    } else {
        // Leaf cell - return raw cell data
        // (Leaf cell - return raw cell data)
        // Cell data has: payload_size (varint), [rowid (varint) for table], payload
        // Btree will handle decoding based on type
        
        // Simple approach - return rest of page from cell start
        // (Simple approach - return rest of page from cell start)
        size_t remaining = data_.size() - cell_offset;
        
        // Read payload size to know how much to return
        // (Read payload size to know how much to return)
        uint64_t payload_size = readVarint(cell_data, bytes_read);
        size_t total_needed = bytes_read + payload_size;
        
        if (type_ == PageType::LEAF_TABLE) {
            // Table leaf also has rowid after payload size
            // (Table leaf also has rowid after payload size)
            size_t rowid_bytes = 0;
            readVarint(cell_data + bytes_read, rowid_bytes);
            total_needed += rowid_bytes;
        }
        
        // Return cell data including payload size varint
        // (Return cell data including payload size varint)
        if (total_needed > remaining) {
            total_needed = remaining;
        }
        
        return std::vector<uint8_t>(cell_data, cell_data + total_needed);
    }
}

// ----------------------------------------------------------------------------
// Read Varint - Variable length integer decode karo
// (Decode variable length integer)
// ----------------------------------------------------------------------------
// Varint 1 ton 9 bytes tak ho sakda - chhote numbers chhoti jagah
// (Varint can be 1 to 9 bytes - small numbers take small space)
// Pehle 8 bytes vich har byte da MSB continuation bit aa
// (In first 8 bytes each byte's MSB is continuation bit)
uint64_t Page::readVarint(const uint8_t* data, size_t& bytes_read) {
    uint64_t result = 0;
    bytes_read = 0;
    
    // Pehle 8 bytes process karo - har byte vich 7 bits value
    // (Process first 8 bytes - 7 bits value in each byte)
    for (int i = 0; i < 8; ++i) {
        uint8_t byte = data[i];
        bytes_read++;
        
        // 7 bits kadho (lower 7 bits)
        // (Get 7 bits (lower 7 bits))
        result = (result << 7) | (byte & 0x7F);
        
        // MSB check karo - 0 hai te varint khatam
        // (Check MSB - if 0 then varint ends)
        if ((byte & 0x80) == 0) {
            return result;
        }
    }
    
    // Agar 8 bytes de baad vi continuation aa te 9th byte puri use ho
    // (If continuation after 8 bytes then 9th byte is fully used)
    uint8_t ninth_byte = data[8];
    bytes_read++;
    result = (result << 8) | ninth_byte;
    
    return result;
}

// ----------------------------------------------------------------------------
// Page Type to String - Debug lai page type print karo
// (Print page type for debug)
// ----------------------------------------------------------------------------
std::string pageTypeToString(PageType type) {
    switch (type) {
        case PageType::INTERIOR_INDEX:
            return "Interior Index B-Tree";      // Beech wala index page (middle index page)
        case PageType::INTERIOR_TABLE:
            return "Interior Table B-Tree";      // Beech wala table page (middle table page)
        case PageType::LEAF_INDEX:
            return "Leaf Index B-Tree";          // Index da patta (index leaf)
        case PageType::LEAF_TABLE:
            return "Leaf Table B-Tree";          // Table da patta (table leaf)
        default:
            return "Unknown Page Type";          // Pata nahi ki aa (don't know what it is)
    }
}

} // namespace sqlite
