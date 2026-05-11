#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>
#include <memory>

// ============================================================================
// DATABASE.HPP - SQLite Database File Reader
// ============================================================================
// Oye, iss file vich assi database di saari core functionality rakhde aa!
// (Hey, in this file we keep all the core database functionality!)
//
// SQLite database ek file aa jisde andar saara data pages vich store hunda
// (SQLite database is a file where all data is stored in pages)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// SQLite Header Constants - Eh saare numbers magic numbers ne database vich
// (These are all magic numbers in the database)
// ----------------------------------------------------------------------------
constexpr size_t SQLITE_HEADER_SIZE = 100;              // Database header da size (header size)
constexpr size_t SQLITE_MAGIC_SIZE = 16;                // "SQLite format 3\000" magic string
constexpr size_t SQLITE_DEFAULT_PAGE_SIZE = 4096;       // Default page size - 4KB hai bai (it's 4KB buddy)

// Header offsets - Kithon ki value milni hai, sab pata hai (Where to find each value, all known)
constexpr size_t OFFSET_PAGE_SIZE = 16;                 // Page size 16th byte ton shuru (starts from 16th byte)
constexpr size_t OFFSET_FILE_FORMAT_WRITE = 18;         // Write version - kon likheya (who wrote)
constexpr size_t OFFSET_FILE_FORMAT_READ = 19;          // Read version - kon padhega (who will read)
constexpr size_t OFFSET_RESERVED_SPACE = 20;            // Reserved space per page
constexpr size_t OFFSET_MAX_PAYLOAD_FRAC = 21;          // Maximum embedded payload fraction
constexpr size_t OFFSET_MIN_PAYLOAD_FRAC = 22;          // Minimum embedded payload fraction
constexpr size_t OFFSET_LEAF_PAYLOAD_FRAC = 23;         // Leaf payload fraction
constexpr size_t OFFSET_FILE_CHANGE_COUNTER = 24;       // File change counter - kitni vaari badleya (how many times changed)
constexpr size_t OFFSET_DATABASE_SIZE = 28;             // Database size in pages
constexpr size_t OFFSET_FREELIST_TRUNK = 32;            // First freelist trunk page
constexpr size_t OFFSET_FREELIST_TOTAL = 36;            // Total freelist pages
constexpr size_t OFFSET_SCHEMA_COOKIE = 40;             // Schema cookie - schema badleya ya nahi (schema changed or not)
constexpr size_t OFFSET_SCHEMA_FORMAT = 44;             // Schema format number
constexpr size_t OFFSET_DEFAULT_CACHE = 48;             // Default page cache size
constexpr size_t OFFSET_LARGEST_ROOT_BTREE = 52;        // Largest root b-tree page number
constexpr size_t OFFSET_TEXT_ENCODING = 56;             // Text encoding - UTF-8 ya UTF-16 (UTF-8 or UTF-16)
constexpr size_t OFFSET_USER_VERSION = 60;              // User version
constexpr size_t OFFSET_INCREMENTAL_VACUUM = 64;        // Incremental vacuum mode
constexpr size_t OFFSET_APPLICATION_ID = 68;            // Application ID
constexpr size_t OFFSET_VERSION_VALID_FOR = 92;         // Version valid for number
constexpr size_t OFFSET_SQLITE_VERSION = 96;            // SQLite version number

// ----------------------------------------------------------------------------
// Text Encoding Types - Text kis tarah store hoyi hai database vich
// (How text is stored in the database)
// ----------------------------------------------------------------------------
enum class TextEncoding : uint32_t {
    UTF8 = 1,       // UTF-8 - sabse zyada common, assi vi yahi use karde (most common, we also use this)
    UTF16LE = 2,    // UTF-16 Little Endian - Windows waleya nu pasand (Windows folks like it)
    UTF16BE = 3     // UTF-16 Big Endian - kuch purane systems (some old systems)
};

// ----------------------------------------------------------------------------
// Database Class - Main dabba jisde vich saari functionality hai
// (Main container which has all functionality)
// ----------------------------------------------------------------------------
class Database {
public:
    // Constructor - Database file kholo, header padho, sab setup karo
    // (Open database file, read header, setup everything)
    explicit Database(const std::string& filename);
    
    // Destructor - File band karo, cleanup karo
    // (Close file, do cleanup)
    ~Database();
    
    // Move semantics - efficiency ke liye, copy nahi karde database nu
    // (For efficiency, we don't copy the database)
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    
    // Copy delete kar dita - database copy karna is a bad idea yaar
    // (Deleted copy - copying database is a bad idea dude)
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    // Getters - Information kadho database bare
    // (Extract information about database)
    uint16_t getPageSize() const { return page_size_; }           // Page kitna wadda hai (how big is page)
    uint32_t getPageCount() const { return page_count_; }         // Kine pages ne total (how many pages total)
    TextEncoding getTextEncoding() const { return text_encoding_; } // Text encoding ki hai (what is text encoding)
    uint32_t getSchemaVersion() const { return schema_version_; }  // Schema da version (schema version)
    
    // Page reading - Koi vi page padh sakte database vichon
    // (Can read any page from database)
    std::vector<uint8_t> readPage(uint32_t page_number);
    
    // Database valid hai ya nahi check karo
    // (Check if database is valid or not)
    bool isValid() const { return is_valid_; }
    
    // Error message kadho agar koi problem hoyi
    // (Get error message if there's a problem)
    const std::string& getError() const { return error_message_; }
    
private:
    // Private helper methods - Andar da kaam (Internal work)
    
    // Header parse karo - pehli 100 bytes vichon saari info kadho
    // (Parse header - extract all info from first 100 bytes)
    void parseHeader();
    
    // Big endian nu little endian vich convert karo
    // (Convert big endian to little endian)
    // SQLite big endian use karda, par assi little endian (Intel) vich kaam karde
    // (SQLite uses big endian, but we work in little endian (Intel))
    static uint16_t readBigEndian16(const uint8_t* data);
    static uint32_t readBigEndian32(const uint8_t* data);
    
    // Member variables - Class de andar da data
    // (Data inside the class)
    std::ifstream file_;                    // File stream - file padhne lai (for reading file)
    std::string filename_;                  // Database file da naam (database file name)
    bool is_valid_;                         // Valid hai ya nahi (is it valid or not)
    std::string error_message_;             // Error message agar koi problem (error message if problem)
    
    // Header information - Saari important cheezein header vichon
    // (All important things from header)
    uint8_t header_[SQLITE_HEADER_SIZE];    // Puri header - 100 bytes (complete header - 100 bytes)
    uint16_t page_size_;                    // Har page kitna wadda (how big each page)
    uint32_t page_count_;                   // Kine pages ne total (total pages)
    TextEncoding text_encoding_;            // Text kis encoding vich (text in which encoding)
    uint32_t schema_version_;               // Schema version number
    uint8_t reserved_space_;                // Reserved space per page - backup lai (for backup)
};

} // namespace sqlite
