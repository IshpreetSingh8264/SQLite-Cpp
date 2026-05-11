#include "database.hpp"
#include <iostream>
#include <cstring>
#include <stdexcept>

// ============================================================================
// DATABASE.CPP - SQLite Database Implementation
// ============================================================================
// Oye database.cpp vich assi file nu khol ke header parse karde aa!
// (Hey in database.cpp we open file and parse header!)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Constructor - Database file kholo te setup karo
// (Open database file and setup)
// ----------------------------------------------------------------------------
Database::Database(const std::string& filename)
    : filename_(filename)
    , is_valid_(false)
    , page_size_(0)
    , page_count_(0)
    , text_encoding_(TextEncoding::UTF8)
    , schema_version_(0)
    , reserved_space_(0)
{
    // File nu binary mode vich kholo - text mode nahi, binary chaiye
    // (Open file in binary mode - not text mode, need binary)
    file_.open(filename, std::ios::binary | std::ios::in);
    
    if (!file_.is_open()) {
        // File nahi khuli yaar - error aa gayi
        // (File didn't open dude - got error)
        error_message_ = "Oye file nahi mili! Cannot open database file: " + filename;
        // (Hey file not found! Cannot open database file)
        return;
    }
    
    // Header padho - pehli 100 bytes bohot important ne
    // (Read header - first 100 bytes are very important)
    file_.read(reinterpret_cast<char*>(header_), SQLITE_HEADER_SIZE);
    
    if (file_.gcount() != SQLITE_HEADER_SIZE) {
        // Puri header nahi mili - file chhoti lagdi
        // (Didn't get full header - file seems small)
        error_message_ = "Database header incomplete hai yaar - file corrupt ho sakdi!";
        // (Database header is incomplete dude - file might be corrupt!)
        file_.close();
        return;
    }
    
    // Magic string check karo - "SQLite format 3\000"
    // (Check magic string - "SQLite format 3\000")
    const char* expected_magic = "SQLite format 3";
    if (std::memcmp(header_, expected_magic, 15) != 0) {
        // Magic string match nahi hoyi - ye SQLite file nahi!
        // (Magic string didn't match - this is not SQLite file!)
        error_message_ = "Oye ye SQLite file nahi lagdi - magic string galat aa!";
        // (Hey this doesn't look like SQLite file - magic string is wrong!)
        file_.close();
        return;
    }
    
    // Sab kuch theek aa - header parse karo
    // (Everything is fine - parse header)
    parseHeader();
    
    is_valid_ = true;
}

// ----------------------------------------------------------------------------
// Destructor - File band kar te cleanup kar
// (Close file and cleanup)
// ----------------------------------------------------------------------------
Database::~Database() {
    if (file_.is_open()) {
        file_.close();
    }
}

// ----------------------------------------------------------------------------
// Move Constructor - Efficiency lai move semantics
// (Move semantics for efficiency)
// ----------------------------------------------------------------------------
Database::Database(Database&& other) noexcept
    : file_(std::move(other.file_))
    , filename_(std::move(other.filename_))
    , is_valid_(other.is_valid_)
    , error_message_(std::move(other.error_message_))
    , page_size_(other.page_size_)
    , page_count_(other.page_count_)
    , text_encoding_(other.text_encoding_)
    , schema_version_(other.schema_version_)
    , reserved_space_(other.reserved_space_)
{
    std::memcpy(header_, other.header_, SQLITE_HEADER_SIZE);
    other.is_valid_ = false;
}

// ----------------------------------------------------------------------------
// Move Assignment - Move assign karo
// (Move assign)
// ----------------------------------------------------------------------------
Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        if (file_.is_open()) {
            file_.close();
        }
        
        file_ = std::move(other.file_);
        filename_ = std::move(other.filename_);
        is_valid_ = other.is_valid_;
        error_message_ = std::move(other.error_message_);
        page_size_ = other.page_size_;
        page_count_ = other.page_count_;
        text_encoding_ = other.text_encoding_;
        schema_version_ = other.schema_version_;
        reserved_space_ = other.reserved_space_;
        
        std::memcpy(header_, other.header_, SQLITE_HEADER_SIZE);
        other.is_valid_ = false;
    }
    return *this;
}

// ----------------------------------------------------------------------------
// Parse Header - Header vichon saari important info kadho
// (Extract all important info from header)
// ----------------------------------------------------------------------------
void Database::parseHeader() {
    // Page size kadho - byte 16-17 te aa (big endian)
    // (Get page size - at bytes 16-17 (big endian))
    page_size_ = readBigEndian16(&header_[OFFSET_PAGE_SIZE]);
    
    // Special case - agar 1 aa te actual size 65536
    // (Special case - if 1 then actual size is 65536)
    if (page_size_ == 1) {
        page_size_ = 65536;  // Maximum page size - 64KB wadda page! (64KB big page!)
    }
    
    // Database size in pages - byte 28-31 te
    // (Database size in pages - at bytes 28-31)
    page_count_ = readBigEndian32(&header_[OFFSET_DATABASE_SIZE]);
    
    // Agar 0 aa te manual calculate karna painda - file size dekh ke
    // (If 0 then need to calculate manually - by looking at file size)
    if (page_count_ == 0) {
        // File di size check karo
        // (Check file size)
        file_.seekg(0, std::ios::end);
        std::streampos file_size = file_.tellg();
        file_.seekg(0, std::ios::beg);
        
        page_count_ = static_cast<uint32_t>(file_size) / page_size_;
    }
    
    // Text encoding - byte 56-59 te (UTF-8, UTF-16LE, UTF-16BE)
    // (Text encoding - at bytes 56-59)
    uint32_t encoding = readBigEndian32(&header_[OFFSET_TEXT_ENCODING]);
    text_encoding_ = static_cast<TextEncoding>(encoding);
    
    // Schema version - byte 40-43 te
    // (Schema version - at bytes 40-43)
    schema_version_ = readBigEndian32(&header_[OFFSET_SCHEMA_COOKIE]);
    
    // Reserved space per page - byte 20 te
    // (Reserved space per page - at byte 20)
    reserved_space_ = header_[OFFSET_RESERVED_SPACE];
}

// ----------------------------------------------------------------------------
// Read Page - Specific page number di content kadho
// (Get content of specific page number)
// ----------------------------------------------------------------------------
std::vector<uint8_t> Database::readPage(uint32_t page_number) {
    if (!is_valid_) {
        // Database valid nahi - error!
        // (Database not valid - error!)
        throw std::runtime_error("Oye database valid nahi! " + error_message_);
        // (Hey database not valid!)
    }
    
    if (page_number == 0 || page_number > page_count_) {
        // Page number galat - range vichon bahar
        // (Page number wrong - out of range)
        throw std::out_of_range("Page number out of range hai bai: " + std::to_string(page_number));
        // (Page number is out of range buddy)
    }
    
    // Page position calculate karo
    // (Calculate page position)
    // Page 1 offset 0 ton shuru, page 2 offset page_size_ ton
    // (Page 1 starts from offset 0, page 2 from offset page_size_)
    std::streamoff offset = static_cast<std::streamoff>(page_number - 1) * page_size_;
    
    // File position set karo
    // (Set file position)
    file_.seekg(offset, std::ios::beg);
    
    if (!file_.good()) {
        // Seek fail hoyi - file corrupt ho sakdi
        // (Seek failed - file might be corrupt)
        throw std::runtime_error("Cannot seek to page - file problem aa gayi!");
        // (Cannot seek to page - file problem occurred!)
    }
    
    // Page da data read karo
    // (Read page data)
    std::vector<uint8_t> page_data(page_size_);
    file_.read(reinterpret_cast<char*>(page_data.data()), page_size_);
    
    if (file_.gcount() != page_size_) {
        // Pura page nahi mileya - problem aa
        // (Didn't get full page - there's a problem)
        throw std::runtime_error("Cannot read full page - incomplete data mili!");
        // (Cannot read full page - got incomplete data!)
    }
    
    return page_data;
}

// ----------------------------------------------------------------------------
// Read Big Endian 16-bit - 2 bytes nu number vich convert karo
// (Convert 2 bytes to number)
// ----------------------------------------------------------------------------
// SQLite big endian use karda - wadde byte pehle, chhote baad vich
// (SQLite uses big endian - big byte first, small byte later)
// Par Intel processors little endian ne - ulta
// (But Intel processors are little endian - opposite)
uint16_t Database::readBigEndian16(const uint8_t* data) {
    // Pehla byte shift kar left te dusre naal OR kar
    // (Shift first byte left and OR with second)
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

// ----------------------------------------------------------------------------
// Read Big Endian 32-bit - 4 bytes nu number vich convert karo
// (Convert 4 bytes to number)
// ----------------------------------------------------------------------------
uint32_t Database::readBigEndian32(const uint8_t* data) {
    // Charo bytes nu sahi position te shift karke combine
    // (Shift all four bytes to right position and combine)
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           data[3];
}

} // namespace sqlite
