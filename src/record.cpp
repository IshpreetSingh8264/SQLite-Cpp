#include "record.hpp"
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <cmath>

// ============================================================================
// RECORD.CPP - SQLite Record Decoder Implementation
// ============================================================================
// Oye record.cpp vich assi records decode karde aa - har row ka data!
// (Hey in record.cpp we decode records - data of each row!)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// Decode Record - Payload vichon record extract karo
// (Extract record from payload)
// ----------------------------------------------------------------------------
// Record format:
//   - Header size (varint) - header kitna wadda (how big is header)
//   - Serial types (varints) - har column da type (type of each column)
//   - Column data - asli data (actual data)
bool Record::decode(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        // Khali payload - koi data nahi
        // (Empty payload - no data)
        return false;
    }
    
    const uint8_t* data = payload.data();
    size_t offset = 0;
    size_t bytes_read = 0;
    
    // Header size padho - pehla varint (read header size - first varint)
    uint64_t header_size = readVarint(data + offset, bytes_read);
    offset += bytes_read;
    
    if (header_size > payload.size()) {
        // Header size hi payload ton wadda - galat aa!
        // (Header size bigger than payload - wrong!)
        return false;
    }
    
    // Serial types padho - header vich saare types ne
    // (Read serial types - all types are in header)
    std::vector<uint64_t> serial_types;
    
    // Header size tak serial types padhde raho
    // (Keep reading serial types till header size)
    while (offset < header_size) {
        uint64_t serial_type = readVarint(data + offset, bytes_read);
        serial_types.push_back(serial_type);
        offset += bytes_read;
    }
    
    // Hun payload ton values decode karo
    // (Now decode values from payload)
    values_.clear();
    values_.reserve(serial_types.size());
    
    for (uint64_t serial_type : serial_types) {
        if (offset > payload.size()) {
            // Data khatam hoyi - incomplete record
            // (Data ended - incomplete record)
            return false;
        }
        
        ColumnValue value = decodeValue(data + offset, serial_type, bytes_read);
        values_.push_back(value);
        offset += bytes_read;
    }
    
    return true;
}

// ----------------------------------------------------------------------------
// Get Column Value - Column di value kadho
// (Get column value)
// ----------------------------------------------------------------------------
const ColumnValue& Record::getColumnValue(size_t index) const {
    if (index >= values_.size()) {
        // Index galat - itne columns nahi ne!
        // (Index wrong - there aren't that many columns!)
        throw std::out_of_range("Column index out of range hai bai!");
        // (Column index is out of range buddy!)
    }
    return values_[index];
}

// ----------------------------------------------------------------------------
// Get Int - Integer value kadho column vichon
// (Get integer value from column)
// ----------------------------------------------------------------------------
std::optional<int64_t> Record::getInt(size_t index) const {
    if (index >= values_.size()) {
        return std::nullopt;
    }
    
    const auto& value = values_[index];
    
    // Integer hai te return kar
    // (If integer then return)
    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value);
    }
    
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Get Float - Float value kadho column vichon
// (Get float value from column)
// ----------------------------------------------------------------------------
std::optional<double> Record::getFloat(size_t index) const {
    if (index >= values_.size()) {
        return std::nullopt;
    }
    
    const auto& value = values_[index];
    
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    }
    
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Get String - String value kadho column vichon
// (Get string value from column)
// ----------------------------------------------------------------------------
std::optional<std::string> Record::getString(size_t index) const {
    if (index >= values_.size()) {
        return std::nullopt;
    }
    
    const auto& value = values_[index];
    
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Get Blob - Binary data kadho column vichon
// (Get binary data from column)
// ----------------------------------------------------------------------------
std::optional<std::vector<uint8_t>> Record::getBlob(size_t index) const {
    if (index >= values_.size()) {
        return std::nullopt;
    }
    
    const auto& value = values_[index];
    
    if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        return std::get<std::vector<uint8_t>>(value);
    }
    
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Is Null - Check karo column NULL hai ya nahi
// (Check if column is NULL or not)
// ----------------------------------------------------------------------------
bool Record::isNull(size_t index) const {
    if (index >= values_.size()) {
        return true;
    }
    
    return std::holds_alternative<std::monostate>(values_[index]);
}

// ----------------------------------------------------------------------------
// To String - Debug lai record print karo
// (Print record for debug)
// ----------------------------------------------------------------------------
std::string Record::toString() const {
    std::ostringstream oss;
    oss << "Record[";
    
    for (size_t i = 0; i < values_.size(); ++i) {
        if (i > 0) oss << ", ";
        
        const auto& value = values_[i];
        
        if (std::holds_alternative<std::monostate>(value)) {
            oss << "NULL";
        } else if (std::holds_alternative<int64_t>(value)) {
            oss << std::get<int64_t>(value);
        } else if (std::holds_alternative<double>(value)) {
            oss << std::get<double>(value);
        } else if (std::holds_alternative<std::string>(value)) {
            oss << "\"" << std::get<std::string>(value) << "\"";
        } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
            oss << "<BLOB>";
        }
    }
    
    oss << "]";
    return oss.str();
}

// ----------------------------------------------------------------------------
// Get Serial Type Size - Serial type ton size calculate karo
// (Calculate size from serial type)
// ----------------------------------------------------------------------------
size_t Record::getSerialTypeSize(uint64_t serial_type) {
    // Serial type di value ton size pata lagda
    // (Size is known from serial type value)
    if (serial_type == 0) return 0;          // NULL - no data
    if (serial_type == 1) return 1;          // 8-bit int
    if (serial_type == 2) return 2;          // 16-bit int
    if (serial_type == 3) return 3;          // 24-bit int
    if (serial_type == 4) return 4;          // 32-bit int
    if (serial_type == 5) return 6;          // 48-bit int
    if (serial_type == 6) return 8;          // 64-bit int
    if (serial_type == 7) return 8;          // Float64
    if (serial_type == 8) return 0;          // Constant 0
    if (serial_type == 9) return 0;          // Constant 1
    if (serial_type >= 12) {
        // BLOB ya TEXT - formula hai size calculate karne da
        // (BLOB or TEXT - there's a formula to calculate size)
        if (serial_type % 2 == 0) {
            // Even - BLOB (judda number - BLOB)
            return (serial_type - 12) / 2;
        } else {
            // Odd - TEXT (takka number - TEXT)
            return (serial_type - 13) / 2;
        }
    }
    
    return 0;  // Unknown serial type
}

// ----------------------------------------------------------------------------
// Decode Value - Serial type te data ton value bana
// (Create value from serial type and data)
// ----------------------------------------------------------------------------
ColumnValue Record::decodeValue(const uint8_t* data, uint64_t serial_type, size_t& bytes_read) {
    bytes_read = 0;
    
    // NULL value - kuch nahi (nothing)
    if (serial_type == 0) {
        return std::monostate{};
    }
    
    // Constant 0 - hamesha 0 (always 0)
    if (serial_type == 8) {
        return static_cast<int64_t>(0);
    }
    
    // Constant 1 - hamesha 1 (always 1)
    if (serial_type == 9) {
        return static_cast<int64_t>(1);
    }
    
    // Integers - 1 to 6 bytes
    if (serial_type >= 1 && serial_type <= 6) {
        size_t size = getSerialTypeSize(serial_type);
        bytes_read = size;
        return decodeInteger(data, size);
    }
    
    // Float - 8 bytes IEEE 754
    if (serial_type == 7) {
        bytes_read = 8;
        return decodeFloat(data);
    }
    
    // BLOB ya TEXT - 12 ton wadde serial types (12 or bigger serial types)
    if (serial_type >= 12) {
        size_t size = getSerialTypeSize(serial_type);
        bytes_read = size;
        
        if (serial_type % 2 == 0) {
            // BLOB - binary data (judda - BLOB)
            return std::vector<uint8_t>(data, data + size);
        } else {
            // TEXT - string data (takka - TEXT)
            return std::string(reinterpret_cast<const char*>(data), size);
        }
    }
    
    // Unknown serial type - NULL return kar
    // (Unknown serial type - return NULL)
    return std::monostate{};
}

// ----------------------------------------------------------------------------
// Decode Integer - Bytes nu signed integer vich convert
// (Convert bytes to signed integer)
// ----------------------------------------------------------------------------
// Big endian format - wadda byte pehle (big byte first)
// Signed integer - negative numbers vi ho sakde (negative numbers also possible)
int64_t Record::decodeInteger(const uint8_t* data, size_t size) {
    if (size == 0) return 0;
    
    int64_t result = 0;
    
    // Pehla byte - sign bit check karo
    // (First byte - check sign bit)
    bool is_negative = (data[0] & 0x80) != 0;
    
    // Saare bytes read karke number bana
    // (Read all bytes and create number)
    for (size_t i = 0; i < size; ++i) {
        result = (result << 8) | data[i];
    }
    
    // Agar negative aa te two's complement convert kar
    // (If negative then convert two's complement)
    if (is_negative && size < 8) {
        // Sign extend kar - negative bits aage lagga
        // (Sign extend - add negative bits in front)
        int64_t sign_extend = -1LL << (size * 8);
        result |= sign_extend;
    }
    
    return result;
}

// ----------------------------------------------------------------------------
// Decode Float - 8 bytes nu IEEE 754 float vich convert
// (Convert 8 bytes to IEEE 754 float)
// ----------------------------------------------------------------------------
double Record::decodeFloat(const uint8_t* data) {
    // IEEE 754 format - 1 sign bit, 11 exponent, 52 mantissa
    uint64_t bits = 0;
    
    // Big endian - pehle byte MSB aa (first byte is MSB)
    for (int i = 0; i < 8; ++i) {
        bits = (bits << 8) | data[i];
    }
    
    // Bits nu double vich convert - memcpy use kar safe tarike naal
    // (Convert bits to double - use memcpy safely)
    double result;
    std::memcpy(&result, &bits, sizeof(double));
    
    return result;
}

// ----------------------------------------------------------------------------
// Read Varint - Global helper function
// (Global helper function)
// ----------------------------------------------------------------------------
uint64_t readVarint(const uint8_t* data, size_t& bytes_read) {
    uint64_t result = 0;
    bytes_read = 0;
    
    // Pehle 8 bytes - har byte vich 7 bits value, 1 bit continuation
    // (First 8 bytes - 7 bits value in each byte, 1 bit continuation)
    for (int i = 0; i < 8; ++i) {
        uint8_t byte = data[i];
        bytes_read++;
        
        // Lower 7 bits kadho (get lower 7 bits)
        result = (result << 7) | (byte & 0x7F);
        
        // MSB 0 aa te khatam (if MSB is 0 then done)
        if ((byte & 0x80) == 0) {
            return result;
        }
    }
    
    // 9th byte - puri 8 bits use hondi (full 8 bits used)
    uint8_t ninth_byte = data[8];
    bytes_read++;
    result = (result << 8) | ninth_byte;
    
    return result;
}

} // namespace sqlite
