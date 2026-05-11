#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <variant>
#include <optional>

// ============================================================================
// RECORD.HPP - SQLite Record Decoder
// ============================================================================
// Oye record.hpp vich assi database records decode karde aa!
// (Hey in record.hpp we decode database records!)
//
// SQLite vich har row ek record aa, te record vich column values ne
// (In SQLite each row is a record, and record has column values)
// Values different types de ho sakde - number, text, blob, NULL
// (Values can be of different types - number, text, blob, NULL)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Serial Type Constants - SQLite vich data kis type da hai ye batanda
// (In SQLite this tells what type the data is)
// ----------------------------------------------------------------------------
// Serial type numbers te unka matlab:
// (Serial type numbers and their meaning:)
//   0       = NULL value - kuch nahi (nothing)
//   1       = 8-bit twos-complement integer - chhota number (small number)
//   2       = 16-bit twos-complement integer
//   3       = 24-bit twos-complement integer
//   4       = 32-bit twos-complement integer
//   5       = 48-bit twos-complement integer - wadda number (big number)
//   6       = 64-bit twos-complement integer - bohot wadda number (very big number)
//   7       = 64-bit IEEE floating point - decimal numbers (decimal numbers)
//   8       = Integer constant 0 - hamesha 0 (always 0)
//   9       = Integer constant 1 - hamesha 1 (always 1)
//   10,11   = Reserved - use nahi hunde (not used)
//   N≥12    = BLOB ya TEXT - lambi cheez (long thing)
//             Agar N even (judda) hai te BLOB, N odd (takka) hai te TEXT
//             (If N is even then BLOB, N is odd then TEXT)
//             Length = (N-12)/2 for BLOB, (N-13)/2 for TEXT
// ----------------------------------------------------------------------------

enum class SerialType {
    NULL_VALUE = 0,     // Null - kuch nahi (nothing)
    INT8 = 1,           // 1 byte integer
    INT16 = 2,          // 2 byte integer
    INT24 = 3,          // 3 byte integer - thoda ajeeb par chalda (bit weird but works)
    INT32 = 4,          // 4 byte integer
    INT48 = 5,          // 6 byte integer
    INT64 = 6,          // 8 byte integer
    FLOAT64 = 7,        // 8 byte float - decimal lai (for decimals)
    CONST_0 = 8,        // Constant 0 - space save karne lai (to save space)
    CONST_1 = 9,        // Constant 1 - space save karne lai (to save space)
    BLOB = 12,          // Binary data - binary data hai (it's binary data)
    TEXT = 13           // Text string - text aa (it's text)
};

// ----------------------------------------------------------------------------
// Column Value - Ek column di value jo kisi bhi type di ho sakdi
// (A column value which can be of any type)
// ----------------------------------------------------------------------------
// C++ variant use karde aa - ek variable vich alag alag types rakh sakte
// (We use C++ variant - can store different types in one variable)
using ColumnValue = std::variant<
    std::monostate,     // NULL - kuch nahi (nothing)
    int64_t,            // Integer values - saare numbers (all numbers)
    double,             // Floating point - decimal numbers (decimal numbers)
    std::string,        // Text strings - text data (text data)
    std::vector<uint8_t> // Binary data (BLOB) - binary data (binary data)
>;

// ----------------------------------------------------------------------------
// Record Class - Ek database record represent karda
// (Represents one database record)
// ----------------------------------------------------------------------------
class Record {
public:
    // Constructor - Khali record bana (Create empty record)
    Record() = default;
    
    // Record decode karo raw bytes vichon
    // (Decode record from raw bytes)
    // payload = cell payload data
    // Returns true agar success, false agar fail (if success, if fail)
    bool decode(const std::vector<uint8_t>& payload);
    
    // Getters - Record vichon data kadho
    // (Get data from record)
    size_t getColumnCount() const { return values_.size(); }  // Kine columns ne (how many columns)
    
    // Column value kadho - NULL ho sakda hai (Get column value - can be NULL)
    const ColumnValue& getColumnValue(size_t index) const;
    
    // Specific type di value kadho - type cast karke
    // (Get value of specific type - with type cast)
    std::optional<int64_t> getInt(size_t index) const;
    std::optional<double> getFloat(size_t index) const;
    std::optional<std::string> getString(size_t index) const;
    std::optional<std::vector<uint8_t>> getBlob(size_t index) const;
    bool isNull(size_t index) const;
    
    // Debug lai - record print karo
    // (For debug - print record)
    std::string toString() const;
    
    // Rowid kadho - har record di unique ID (Get rowid - unique ID of each record)
    int64_t getRowId() const { return rowid_; }
    void setRowId(int64_t rowid) { rowid_ = rowid; }
    
private:
    // Helper methods - Andar de kaam (Internal work)
    
    // Serial type vichon size kadho - data kitna lamba hai
    // (Get size from serial type - how long is data)
    static size_t getSerialTypeSize(uint64_t serial_type);
    
    // Value decode karo based on serial type
    // (Decode value based on serial type)
    static ColumnValue decodeValue(const uint8_t* data, uint64_t serial_type, size_t& bytes_read);
    
    // Integer decode karo - signed integer big endian vichon
    // (Decode integer - signed integer from big endian)
    static int64_t decodeInteger(const uint8_t* data, size_t size);
    
    // Float decode karo - IEEE 754 format
    // (Decode float - IEEE 754 format)
    static double decodeFloat(const uint8_t* data);
    
    // Member variables - Record de andar da data
    // (Data inside record)
    std::vector<ColumnValue> values_;   // Saare column values (all column values)
    int64_t rowid_;                     // Row ID - unique identifier
};

// ----------------------------------------------------------------------------
// Helper Functions - Varint handling
// ============================================================================
// Varint = Variable length integer
// SQLite vich numbers variable size de hunde, 1 byte ton 9 bytes tak
// (In SQLite numbers are variable size, from 1 byte to 9 bytes)
// Chhote numbers lai chhoti jagah, wadde numbers lai waddi jagah
// (Small space for small numbers, big space for big numbers)
// ----------------------------------------------------------------------------

// Varint read karo - 1 to 9 bytes de number nu read karde aa
// (Read varint - we read 1 to 9 byte numbers)
// Returns the value, bytes_read vich kitne bytes padhey wo store hunda
// (Returns the value, stores how many bytes read in bytes_read)
uint64_t readVarint(const uint8_t* data, size_t& bytes_read);

// Varint da maximum size calculate karo
// (Calculate maximum size of varint)
constexpr size_t MAX_VARINT_SIZE = 9;  // Maximum 9 bytes ho sakde (maximum 9 bytes possible)

} // namespace sqlite
