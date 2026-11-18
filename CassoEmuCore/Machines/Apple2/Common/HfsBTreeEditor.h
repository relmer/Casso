#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/HfsBTree.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor
//
//  Search, insert and delete on an HFS B-tree file's nodes, changing only
//  the nodes each operation touches.
//
//  An insert goes into its leaf. A leaf that overflows is split in two by
//  bytes, and an entry for the new node is added to its parent, which may
//  be split in turn. When the root is split, a new root one level higher
//  is added above it. A delete removes the record from its leaf. When a
//  node falls below half full, records are moved into it from a sibling
//  under the same parent, or the two are merged when both fit in one node.
//  A merge removes an entry from the parent, which can drop the parent
//  below half full in turn. A root index node with one child is replaced
//  by that child, and an empty root leaf is freed, which empties the tree.
//  An index entry's key is always the first key of its child, so a change
//  to a node's first key is copied to each index node above it whose first
//  entry points down to it.
//
//  Records vary in length, so "half full" is measured in bytes; after a
//  split or a redistribution a node can fall short of half by up to one
//  record's bytes.
//
//  Nodes come from the node map: the header node's map record covers the
//  first 2048 nodes and each map node, chained from the header's forward
//  link, the next 3952. An insert uses up to one new node at each level and
//  one for a new root, the count GetNodesForInsert returns. When fewer are
//  free, the caller grows the tree file with Extend before inserting.
//
////////////////////////////////////////////////////////////////////////////////

class HfsBTreeEditor
{
public:
    using Record = HfsBTree::Record;

    HfsBTreeEditor (HfsBTree::Kind kind, std::vector<Byte> & nodes);

    //  Whether node 0 is a header node, and its node counts fit the file.
    HRESULT  Open ();

    uint32_t  GetTotalNodes      () const;
    uint32_t  GetFreeNodes       () const;
    uint32_t  GetDepth           () const;
    uint32_t  GetNodesForInsert  () const { return GetDepth() + 1; }

    //  The file at `totalNodes` nodes, with map nodes added when the map
    //  covers fewer. The new nodes are free.
    HRESULT  Extend (uint32_t totalNodes);

    //  The record with a key equal to `key`, or ERROR_NOT_FOUND.
    HRESULT  Find (const std::vector<Byte> & key, Record & outRecord) const;

    //  The records from the first with a key at or after `from`, in order,
    //  until `visit` returns false.
    HRESULT  Scan (const std::vector<Byte> & from, const std::function<bool (const Record &)> & visit) const;

    //  Every leaf record, in order.
    HRESULT  ReadAll (std::vector<Record> & outRecords) const;

    //  ERROR_ALREADY_EXISTS for a key already in the tree; ERROR_DISK_FULL
    //  when no node is free for a split.
    HRESULT  Insert  (const Record & record);
    HRESULT  Remove  (const std::vector<Byte> & key);

    //  The data of the record with the record's key.
    HRESULT  Replace (const Record & record);

    //  Each broken invariant, one per line: key order in each node and
    //  along each level, leaf depth, index keys equal to their children's
    //  first keys, sibling links, the header's counts, the node map, and
    //  each node other than the root at least half full, less one record.
    void  Check (std::string & outProblems) const;

    //  The nodes written since the editor was opened.
    const std::set<uint32_t> &  GetChangedNodes () const { return m_changed; }

private:
    struct Node
    {
        uint32_t             fLink  = 0;
        uint32_t             bLink  = 0;
        Byte                 type   = 0;
        Byte                 height = 0;
        std::vector<Record>  records;
    };

    struct Change
    {
        bool               isSplit        = false;
        Record             splitEntry;
        bool               isFirstChanged = false;
        std::vector<Byte>  firstKey;
        bool               isShort        = false;
    };

    static constexpr size_t    kHeaderDepth       = 14;
    static constexpr size_t    kHeaderRoot        = 16;
    static constexpr size_t    kHeaderLeafRecords = 20;
    static constexpr size_t    kHeaderFirstLeaf   = 24;
    static constexpr size_t    kHeaderLastLeaf    = 28;
    static constexpr size_t    kHeaderTotalNodes  = 36;
    static constexpr size_t    kHeaderFreeNodes   = 40;
    static constexpr size_t    kHeaderMap         = HfsBTree::kDescriptorBytes + HfsBTree::kHeaderRecordBytes + HfsBTree::kUserRecordBytes;
    static constexpr uint32_t  kHeaderMapNodes    = (uint32_t) HfsBTree::kHeaderMapBytes * 8;
    static constexpr uint32_t  kMapNodeNodes      = (uint32_t) HfsBTree::kMapNodeBytes * 8;
    static constexpr size_t    kCapacity          = HfsBTree::kNodeBytes - HfsBTree::kDescriptorBytes - 2;
    static constexpr int       kMaxHeight         = 16;
    static constexpr size_t    kCatalogKeyBytes   = (size_t) HfsBTree::kCatalogKeyMax + 1;
    static constexpr size_t    kExtentsKeyBytes   = (size_t) HfsBTree::kExtentsKeyMax + 1;
    static constexpr size_t    kFileRecordBytes   = 102;
    static constexpr size_t    kExtentsDataBytes  = 12;
    static constexpr size_t    kPointerBytes      = 4;

    uint32_t  GetHeaderLong  (size_t at) const;
    void      SetHeaderLong  (size_t at, uint32_t value);
    uint32_t  GetRoot        () const { return GetHeaderLong (kHeaderRoot); }
    void      SetDepth       (uint32_t depth);

    int       CompareKeys    (const std::vector<Byte> & a, const std::vector<Byte> & b) const;
    size_t    GetPayload     (const Node & node) const;
    size_t    GetPayload     (const std::vector<Record> & records, bool isIndex) const;
    size_t    GetRecordSize  (const Record & record, bool isIndex) const;
    size_t    GetMinimumFill (bool isIndex) const;
    Record    MakeEntry      (const std::vector<Byte> & firstKey, uint32_t child) const;

    HRESULT   ReadNode       (uint32_t number, Node & outNode) const;
    void      WriteNode      (uint32_t number, const Node & node);
    HRESULT   FindLeaf       (const std::vector<Byte> & key, uint32_t & outLeaf, Node & outNode) const;
    HRESULT   FindChild      (uint32_t number, const std::vector<Byte> & key, Byte & outType, size_t & outIndex, uint32_t & outChild) const;

    bool      IsNodeUsed     (uint32_t number) const;
    void      SetNodeUsed    (uint32_t number, bool used);
    size_t    GetMapByte     (uint32_t number) const;
    HRESULT   AllocateNode   (uint32_t & outNumber);
    void      FreeNode       (uint32_t number);

    HRESULT   InsertInto        (uint32_t number, const Record & record, Change & outChange);
    HRESULT   SplitNode         (uint32_t number, Node & ioNode, Change & outChange);
    HRESULT   RemoveFrom        (uint32_t number, const std::vector<Byte> & key, bool isRoot, Change & outChange);
    HRESULT   Rebalance         (Node & ioParent, size_t child, Change & outChange);
    size_t    FindBalancedSplit (const std::vector<Record> & records, bool isIndex) const;

    void      CheckNode      (uint32_t number, int height, const std::vector<Byte> * lowKey, std::set<uint32_t> & ioSeen, std::vector<std::vector<uint32_t>> & ioLevels, uint32_t & ioRecords, std::string & outProblems) const;

    HfsBTree::Kind        m_kind;
    std::vector<Byte>   & m_nodes;
    std::set<uint32_t>    m_changed;
};
