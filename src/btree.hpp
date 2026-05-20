#pragma once

#include "page.hpp"
#include "record.hpp"
#include "database.hpp"
#include <vector>
#include <memory>
#include <functional>

// ============================================================================
// BTREE.HPP - SQLite B-Tree Navigator
// ============================================================================
// Oye btree.hpp vich assi B-tree navigate karde aa!
// (Hey in btree.hpp we navigate the B-tree!)
//
// B-tree ek rukh aa jisde vich data sorted order vich aa
// (B-tree is a tree in which data is in sorted order)
// Assi jaldi naal koi vi record dhundh sakte
// (We can quickly find any record)
//
// BTree (table) vich <payload, rowid> hunda, IndexBTree vich <key, rowid>.
// (Table B-tree stores <payload, rowid>; index B-tree stores <key, rowid>.)
// Dono de impl alag-alag files vich: btree.cpp te index_btree.cpp
// (The two implementations live in separate files: btree.cpp and index_btree.cpp)
// ============================================================================

namespace sqlite {

// ----------------------------------------------------------------------------
// BTree Class - B-tree navigate karne lai
// (For navigating B-tree)
// ----------------------------------------------------------------------------
class BTree {
public:
    // Constructor - B-tree bana database te root page lai
    // (Create B-tree for database and root page)
    BTree(Database& database, uint32_t root_page_number);
    
    // Destructor
    ~BTree() = default;
    
    // Saare records scan karo - pura table padh lo
    // (Scan all records - read entire table)
    // callback function har record lai call hoyega
    // (callback function will be called for each record)
    void scanAll(std::function<void(const Record&)> callback);
    
    // Specific key wala record dhundho - binary search use karke
    // (Find record with specific key - using binary search)
    // Index search lai use hunda - jaldi search (Used for index search - fast search)
    std::optional<Record> findByKey(int64_t key);
    
    // Count karo kitne records ne total
    // (Count how many records total)
    size_t countRecords();
    
    // Root page number kadho
    // (Get root page number)
    uint32_t getRootPageNumber() const { return root_page_number_; }
    
private:
    // Helper methods - Andar de kaam (Internal work)
    
    // Leaf page scan karo - asli data leaf pages vich hunda
    // (Scan leaf page - real data is in leaf pages)
    void scanLeafPage(uint32_t page_number, std::function<void(const Record&)> callback);
    
    // Interior page traverse karo - beech wale pages children ko point karde
    // (Traverse interior page - middle pages point to children)
    void scanInteriorPage(uint32_t page_number, std::function<void(const Record&)> callback);
    
    // Table leaf cell decode karo - cell vichon record nikalo
    // (Decode table leaf cell - extract record from cell)
    Record decodeTableLeafCell(const std::vector<uint8_t>& cell_data);
    
    // Interior cell vichon left child page number kadho
    // (Get left child page number from interior cell)
    uint32_t getLeftChildPointer(const std::vector<uint8_t>& cell_data);
    
    // Page vich key dhundho - recursive helper
    // (Find key in page - recursive helper)
    std::optional<Record> findByKeyInPage(uint32_t page_number, int64_t key);

    // Member variables - BTree de andar da data
    // (Data inside BTree)
    Database& database_;                // Database reference - file padhne lai (for reading file)
    uint32_t root_page_number_;         // Root page number - tree kithon shuru (where tree starts)
};

// ----------------------------------------------------------------------------
// Index BTree - Index pages lai special handling
// (Special handling for index pages)
// ----------------------------------------------------------------------------
// Index B-tree table B-tree ton thoda different hunda
// (Index B-tree is slightly different from table B-tree)
// Index vich key te rowid hunda, asli data nahi
// (Index has key and rowid, not real data)
class IndexBTree {
public:
    // Constructor
    IndexBTree(Database& database, uint32_t root_page_number);
    
    // Index use karke rowids dhundho for given key
    // (Find rowids using index for given key)
    std::vector<int64_t> findRowIds(const std::string& key);
    
    // Saare index entries scan karo
    // (Scan all index entries)
    void scanAll(std::function<void(const Record&)> callback);
    
private:
    // Helper - Index leaf cell decode karo
    // (Helper - decode index leaf cell)
    Record decodeIndexLeafCell(const std::vector<uint8_t>& cell_data);
    
    // Leaf page scan for index
    void scanIndexLeafPage(uint32_t page_number, std::function<void(const Record&)> callback);
    
    // Interior page scan for index
    void scanIndexInteriorPage(uint32_t page_number, std::function<void(const Record&)> callback);
    
    // B-tree search helper - specific page vich key dhundho
    // (B-tree search helper - find key in specific page)
    void findRowIdsInPage(uint32_t page_number, const std::string& key, std::vector<int64_t>& rowids);
    
    Database& database_;
    uint32_t root_page_number_;
};

} // namespace sqlite
