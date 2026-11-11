#include "Pch.h"

#include "Machines/Apple2/Common/HfsBTreeEditor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::HfsBTreeEditor
//
////////////////////////////////////////////////////////////////////////////////

HfsBTreeEditor::HfsBTreeEditor (HfsBTree::Kind kind, std::vector<Byte> & nodes) :
    m_kind  (kind),
    m_nodes (nodes)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::Open()
{
    HRESULT   hr       = S_OK;
    bool      sized    = m_nodes.size() >= HfsBTree::kNodeBytes && m_nodes.size() % HfsBTree::kNodeBytes == 0;
    bool      isHeader = false;
    uint32_t  total    = 0;
    bool      fits     = false;



    CBREx (sized, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    isHeader = m_nodes[8] == HfsBTree::kNodeHeader;
    CBREx (isHeader, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    total = GetTotalNodes();
    fits  = total > 0 && total <= m_nodes.size() / HfsBTree::kNodeBytes && GetFreeNodes() < total;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    m_nodes.resize ((size_t) total * HfsBTree::kNodeBytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetTotalNodes
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsBTreeEditor::GetTotalNodes() const
{
    return GetHeaderLong (kHeaderTotalNodes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetFreeNodes
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsBTreeEditor::GetFreeNodes() const
{
    return GetHeaderLong (kHeaderFreeNodes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetDepth
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsBTreeEditor::GetDepth() const
{
    return ((uint32_t) m_nodes[kHeaderDepth] << 8) | m_nodes[kHeaderDepth + 1];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::SetDepth
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTreeEditor::SetDepth (uint32_t depth)
{
    HfsBTree::PutWord (m_nodes, kHeaderDepth, depth);
    m_changed.insert (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetHeaderLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsBTreeEditor::GetHeaderLong (size_t at) const
{
    return HfsBTree::GetLong (m_nodes, at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::SetHeaderLong
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTreeEditor::SetHeaderLong (size_t at, uint32_t value)
{
    HfsBTree::PutLong (m_nodes, at, value);
    m_changed.insert (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::CompareKeys
//
////////////////////////////////////////////////////////////////////////////////

int HfsBTreeEditor::CompareKeys (const std::vector<Byte> & a, const std::vector<Byte> & b) const
{
    return (m_kind == HfsBTree::Kind::Catalog) ? HfsBTree::CompareCatalogKeys (a, b) : HfsBTree::CompareExtentsKeys (a, b);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetRecordSize
//
//  A record's bytes in a node and its offset at the end of the node.
//
////////////////////////////////////////////////////////////////////////////////

size_t HfsBTreeEditor::GetRecordSize (const Record & record, bool isIndex) const
{
    return HfsBTree::GetRecordBytes (m_kind, record, isIndex) + 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetPayload
//
////////////////////////////////////////////////////////////////////////////////

size_t HfsBTreeEditor::GetPayload (const std::vector<Record> & records, bool isIndex) const
{
    size_t  bytes = 0;



    for (const Record & record : records)
    {
        bytes += GetRecordSize (record, isIndex);
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetPayload
//
////////////////////////////////////////////////////////////////////////////////

size_t HfsBTreeEditor::GetPayload (const Node & node) const
{
    return GetPayload (node.records, node.type == HfsBTree::kNodeIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetMinimumFill
//
//  Half a node's room for records, less the longest record the node can
//  have: a catalog leaf's longest key and a file record, or an index
//  record's key and pointer.
//
////////////////////////////////////////////////////////////////////////////////

size_t HfsBTreeEditor::GetMinimumFill (bool isIndex) const
{
    size_t  key     = (m_kind == HfsBTree::Kind::Catalog) ? kCatalogKeyBytes : kExtentsKeyBytes;
    size_t  data    = isIndex ? kPointerBytes : (m_kind == HfsBTree::Kind::Catalog) ? kFileRecordBytes : kExtentsDataBytes;
    size_t  longest = key + data + 2;



    return kCapacity / 2 - longest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::MakeEntry
//
//  An index record for a child whose first key is `firstKey`.
//
////////////////////////////////////////////////////////////////////////////////

HfsBTree::Record HfsBTreeEditor::MakeEntry (const std::vector<Byte> & firstKey, uint32_t child) const
{
    Record  entry { HfsBTree::MakeIndexKey (m_kind, firstKey), std::vector<Byte> (4, 0) };



    HfsBTree::PutLong (entry.data, 0, child);

    return entry;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetChildIndex
//
//  The last entry whose key is at or before `key`, or the first entry when
//  `key` sorts before them all.
//
////////////////////////////////////////////////////////////////////////////////

size_t HfsBTreeEditor::GetChildIndex (const Node & node, const std::vector<Byte> & key) const
{
    size_t  child = 0;



    for (size_t at = 1; at < node.records.size() && CompareKeys (node.records[at].key, key) <= 0; at++)
    {
        child = at;
    }

    return child;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::ReadNode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::ReadNode (uint32_t number, Node & outNode) const
{
    HRESULT  hr      = S_OK;
    size_t   base    = (size_t) number * HfsBTree::kNodeBytes;
    bool     inside  = number != 0 && number < GetTotalNodes() && base + HfsBTree::kNodeBytes <= m_nodes.size();
    size_t   count   = 0;
    bool     counted = false;
    auto     word    = [&] (size_t at) { return (size_t) ((m_nodes[base + at] << 8) | m_nodes[base + at + 1]); };



    outNode = Node();

    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    outNode.fLink  = HfsBTree::GetLong (m_nodes, base);
    outNode.bLink  = HfsBTree::GetLong (m_nodes, base + 4);
    outNode.type   = m_nodes[base + 8];
    outNode.height = m_nodes[base + 9];
    count          = word (10);
    counted        = HfsBTree::kDescriptorBytes + 2 * (count + 1) <= HfsBTree::kNodeBytes;
    CBREx (counted, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    for (size_t i = 0; i < count; i++)
    {
        size_t  start = word (HfsBTree::kNodeBytes - 2 * (i + 1));
        size_t  end   = word (HfsBTree::kNodeBytes - 2 * (i + 2));
        size_t  key   = 0;
        size_t  data  = 0;
        bool    valid = start >= HfsBTree::kDescriptorBytes && start < end && end <= HfsBTree::kNodeBytes;
        Record  record;

        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        key   = (size_t) m_nodes[base + start] + 1;
        data  = start + key + (key % 2);
        valid = data <= end;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        record.key.assign  (m_nodes.begin() + (ptrdiff_t) (base + start), m_nodes.begin() + (ptrdiff_t) (base + start + key));
        record.data.assign (m_nodes.begin() + (ptrdiff_t) (base + data),  m_nodes.begin() + (ptrdiff_t) (base + end));
        outNode.records.push_back (std::move (record));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::WriteNode
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTreeEditor::WriteNode (uint32_t number, const Node & node)
{
    std::vector<Byte>  bytes (HfsBTree::kNodeBytes, 0);
    bool               index = node.type == HfsBTree::kNodeIndex;
    size_t             at    = HfsBTree::kDescriptorBytes;



    HfsBTree::PutLong (bytes, 0, node.fLink);
    HfsBTree::PutLong (bytes, 4, node.bLink);
    bytes[8] = node.type;
    bytes[9] = node.height;
    HfsBTree::PutWord (bytes, 10, (uint32_t) node.records.size());

    for (size_t slot = 0; slot < node.records.size(); slot++)
    {
        const Record &     record = node.records[slot];
        std::vector<Byte>  key    = index ? HfsBTree::MakeIndexKey (m_kind, record.key) : record.key;

        HfsBTree::PutWord (bytes, HfsBTree::kNodeBytes - 2 * (slot + 1), (uint32_t) at);

        std::copy (key.begin(), key.end(), bytes.begin() + (ptrdiff_t) at);
        at += key.size() + (key.size() % 2);

        std::copy (record.data.begin(), record.data.end(), bytes.begin() + (ptrdiff_t) at);
        at += record.data.size();
    }

    HfsBTree::PutWord (bytes, HfsBTree::kNodeBytes - 2 * (node.records.size() + 1), (uint32_t) at);

    std::copy (bytes.begin(), bytes.end(), m_nodes.begin() + (ptrdiff_t) ((size_t) number * HfsBTree::kNodeBytes));
    m_changed.insert (number);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::FindLeaf
//
//  The leaf that contains `key`, or where a record with `key` goes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::FindLeaf (const std::vector<Byte> & key, uint32_t & outLeaf, Node & outNode) const
{
    HRESULT  hr = S_OK;



    outLeaf = GetRoot();
    CBREx (outLeaf != 0, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    for (int level = 0; level <= kMaxHeight; level++)
    {
        bool  isIndex = false;

        hr = ReadNode (outLeaf, outNode);
        CHR (hr);

        BAIL_OUT_IF (outNode.type == HfsBTree::kNodeLeaf, S_OK);

        isIndex = outNode.type == HfsBTree::kNodeIndex && !outNode.records.empty();
        CBREx (isIndex, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        outLeaf = HfsBTree::GetLong (outNode.records[GetChildIndex (outNode, key)].data, 0);
    }

    hr = HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Find
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::Find (const std::vector<Byte> & key, Record & outRecord) const
{
    HRESULT   hr   = S_OK;
    uint32_t  leaf = 0;
    Node      node;



    hr = FindLeaf (key, leaf, node);
    CHR (hr);

    for (const Record & record : node.records)
    {
        if (CompareKeys (record.key, key) == 0)
        {
            outRecord = record;
            BAIL_OUT_IF (true, S_OK);
        }
    }

    hr = HRESULT_FROM_WIN32 (ERROR_NOT_FOUND);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Scan
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::Scan (const std::vector<Byte> & from, const std::function<bool (const Record &)> & visit) const
{
    HRESULT   hr    = S_OK;
    uint32_t  leaf  = 0;
    uint32_t  steps = 0;
    Node      node;



    BAIL_OUT_IF (GetRoot() == 0, S_OK);

    hr = FindLeaf (from, leaf, node);
    CHR (hr);

    for (;;)
    {
        bool  bounded = ++steps <= GetTotalNodes();

        CBREx (bounded, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        for (const Record & record : node.records)
        {
            if (CompareKeys (record.key, from) >= 0 && !visit (record))
            {
                BAIL_OUT_IF (true, S_OK);
            }
        }

        BAIL_OUT_IF (node.fLink == 0, S_OK);

        hr = ReadNode (node.fLink, node);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::ReadAll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::ReadAll (std::vector<Record> & outRecords) const
{
    HRESULT   hr    = S_OK;
    uint32_t  next  = GetHeaderLong (kHeaderFirstLeaf);
    uint32_t  steps = 0;
    Node      node;



    outRecords.clear();

    while (next != 0)
    {
        bool  bounded = ++steps <= GetTotalNodes();

        CBREx (bounded, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        hr = ReadNode (next, node);
        CHR (hr);

        CBREx (node.type == HfsBTree::kNodeLeaf, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        outRecords.insert (outRecords.end(), node.records.begin(), node.records.end());
        next = node.fLink;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::GetMapByte
//
//  Where in the nodes the map bit of node `number` is, or SIZE_MAX when the
//  map doesn't cover it.
//
////////////////////////////////////////////////////////////////////////////////

size_t HfsBTreeEditor::GetMapByte (uint32_t number) const
{
    uint32_t  offset = number;
    uint32_t  map    = 0;
    uint32_t  steps  = 0;



    if (offset < kHeaderMapNodes)
    {
        return kHeaderMap + offset / 8;
    }

    offset -= kHeaderMapNodes;
    map     = HfsBTree::GetLong (m_nodes, 0);

    while (map != 0 && map < GetTotalNodes() && ++steps <= GetTotalNodes())
    {
        if (offset < kMapNodeNodes)
        {
            return (size_t) map * HfsBTree::kNodeBytes + HfsBTree::kDescriptorBytes + offset / 8;
        }

        offset -= kMapNodeNodes;
        map     = HfsBTree::GetLong (m_nodes, (size_t) map * HfsBTree::kNodeBytes);
    }

    return SIZE_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::IsNodeUsed
//
//  Each map covers a multiple of eight nodes, so a node's bit is its number
//  modulo eight from the high bit.
//
////////////////////////////////////////////////////////////////////////////////

bool HfsBTreeEditor::IsNodeUsed (uint32_t number) const
{
    size_t  at = GetMapByte (number);



    return at != SIZE_MAX && (m_nodes[at] & (0x80 >> (number % 8))) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::SetNodeUsed
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTreeEditor::SetNodeUsed (uint32_t number, bool used)
{
    size_t  at   = GetMapByte (number);
    Byte    mask = (Byte) (0x80 >> (number % 8));



    if (at != SIZE_MAX)
    {
        m_nodes[at] = used ? (Byte) (m_nodes[at] | mask) : (Byte) (m_nodes[at] & ~mask);
        m_changed.insert ((uint32_t) (at / HfsBTree::kNodeBytes));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::AllocateNode
//
//  The lowest free node the map covers, cleared.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::AllocateNode (uint32_t & outNumber)
{
    HRESULT  hr    = S_OK;
    bool     found = false;



    outNumber = 0;
    found     = GetFreeNodes() > 0;
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    found = false;

    for (uint32_t number = 1; number < GetTotalNodes() && !found; number++)
    {
        if (!IsNodeUsed (number) && GetMapByte (number) != SIZE_MAX)
        {
            outNumber = number;
            found     = true;
        }
    }

    CBREx (found, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    SetNodeUsed   (outNumber, true);
    SetHeaderLong (kHeaderFreeNodes, GetFreeNodes() - 1);

    std::fill_n (m_nodes.begin() + (ptrdiff_t) ((size_t) outNumber * HfsBTree::kNodeBytes), HfsBTree::kNodeBytes, (Byte) 0);
    m_changed.insert (outNumber);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::FreeNode
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTreeEditor::FreeNode (uint32_t number)
{
    SetNodeUsed   (number, false);
    SetHeaderLong (kHeaderFreeNodes, GetFreeNodes() + 1);

    std::fill_n (m_nodes.begin() + (ptrdiff_t) ((size_t) number * HfsBTree::kNodeBytes), HfsBTree::kNodeBytes, (Byte) 0);
    m_changed.insert (number);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Extend
//
//  Each map node added is the lowest free node the map already covers, or
//  the first node past it, which the new map node covers itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::Extend (uint32_t totalNodes)
{
    HRESULT   hr       = S_OK;
    uint32_t  old      = GetTotalNodes();
    uint32_t  coverage = kHeaderMapNodes;
    uint32_t  last     = 0;
    uint32_t  steps    = 0;



    BAIL_OUT_IF (totalNodes <= old, S_OK);

    m_nodes.resize ((size_t) totalNodes * HfsBTree::kNodeBytes, 0);
    SetHeaderLong (kHeaderTotalNodes, totalNodes);
    SetHeaderLong (kHeaderFreeNodes,  GetFreeNodes() + (totalNodes - old));

    for (uint32_t map = HfsBTree::GetLong (m_nodes, 0); map != 0; map = HfsBTree::GetLong (m_nodes, (size_t) map * HfsBTree::kNodeBytes))
    {
        bool  bounded = ++steps <= totalNodes;

        CBREx (bounded, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        last      = map;
        coverage += kMapNodeNodes;
    }

    while (coverage < totalNodes)
    {
        uint32_t  map  = coverage;
        size_t    base = 0;

        for (uint32_t number = coverage - 1; number >= 1; number--)
        {
            map = IsNodeUsed (number) ? map : number;
        }

        base = (size_t) map * HfsBTree::kNodeBytes;
        std::fill_n (m_nodes.begin() + (ptrdiff_t) base, HfsBTree::kNodeBytes, (Byte) 0);
        m_nodes[base + 8] = HfsBTree::kNodeMap;
        HfsBTree::PutWord (m_nodes, base + 10, 1);
        HfsBTree::PutWord (m_nodes, base + HfsBTree::kNodeBytes - 2, (uint32_t) HfsBTree::kDescriptorBytes);
        HfsBTree::PutWord (m_nodes, base + HfsBTree::kNodeBytes - 4, (uint32_t) (HfsBTree::kDescriptorBytes + HfsBTree::kMapNodeBytes));
        m_changed.insert (map);

        HfsBTree::PutLong (m_nodes, (size_t) last * HfsBTree::kNodeBytes, map);
        m_changed.insert (last);

        last      = map;
        coverage += kMapNodeNodes;

        SetNodeUsed   (map, true);
        SetHeaderLong (kHeaderFreeNodes, GetFreeNodes() - 1);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Insert
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::Insert (const Record & record)
{
    HRESULT   hr      = S_OK;
    uint32_t  root    = GetRoot();
    uint32_t  newRoot = 0;
    Change    change;
    Node      node;



    if (root == 0)
    {
        hr = AllocateNode (root);
        CHR (hr);

        node.type    = HfsBTree::kNodeLeaf;
        node.height  = 1;
        node.records = { record };
        WriteNode (root, node);

        SetHeaderLong (kHeaderRoot,      root);
        SetHeaderLong (kHeaderFirstLeaf, root);
        SetHeaderLong (kHeaderLastLeaf,  root);
        SetDepth (1);
    }
    else
    {
        hr = InsertInto (root, record, change);
        CHR (hr);

        if (change.isSplit)
        {
            Node  old;

            hr = ReadNode (root, old);
            CHR (hr);

            hr = AllocateNode (newRoot);
            CHR (hr);

            node.type    = HfsBTree::kNodeIndex;
            node.height  = (Byte) (old.height + 1);
            node.records = { MakeEntry (old.records[0].key, root), change.splitEntry };
            WriteNode (newRoot, node);

            SetHeaderLong (kHeaderRoot, newRoot);
            SetDepth (GetDepth() + 1);
        }
    }

    SetHeaderLong (kHeaderLeafRecords, GetHeaderLong (kHeaderLeafRecords) + 1);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::InsertInto
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::InsertInto (uint32_t number, const Record & record, Change & outChange)
{
    HRESULT  hr        = S_OK;
    Node     node;
    bool     isChanged = true;



    outChange = Change();

    hr = ReadNode (number, node);
    CHR (hr);

    if (node.type == HfsBTree::kNodeLeaf)
    {
        size_t  at     = 0;
        bool    unique = false;

        while (at < node.records.size() && CompareKeys (node.records[at].key, record.key) < 0)
        {
            at++;
        }

        unique = at == node.records.size() || CompareKeys (node.records[at].key, record.key) != 0;
        CBREx (unique, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

        node.records.insert (node.records.begin() + (ptrdiff_t) at, record);

        if (at == 0)
        {
            outChange.isFirstChanged = true;
            outChange.firstKey       = record.key;
        }
    }
    else
    {
        bool      isIndex = node.type == HfsBTree::kNodeIndex && !node.records.empty();
        size_t    at      = 0;
        uint32_t  child   = 0;
        Change    below;

        CBREx (isIndex, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        at    = GetChildIndex (node, record.key);
        child = HfsBTree::GetLong (node.records[at].data, 0);

        hr = InsertInto (child, record, below);
        CHR (hr);

        isChanged = below.isFirstChanged || below.isSplit;

        if (below.isFirstChanged)
        {
            node.records[at].key = HfsBTree::MakeIndexKey (m_kind, below.firstKey);

            if (at == 0)
            {
                outChange.isFirstChanged = true;
                outChange.firstKey       = below.firstKey;
            }
        }

        if (below.isSplit)
        {
            node.records.insert (node.records.begin() + (ptrdiff_t) at + 1, below.splitEntry);
        }
    }

    BAIL_OUT_IF (!isChanged, S_OK);

    if (GetPayload (node) <= kCapacity)
    {
        WriteNode (number, node);
    }
    else
    {
        hr = SplitNode (number, node, outChange);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::SplitNode
//
//  The node's later records into a new node after it on the same level.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::SplitNode (uint32_t number, Node & ioNode, Change & outChange)
{
    HRESULT   hr          = S_OK;
    bool      isIndex     = ioNode.type == HfsBTree::kNodeIndex;
    size_t    at          = FindBalancedSplit (ioNode.records, isIndex);
    uint32_t  rightNumber = 0;
    Node      right;



    hr = AllocateNode (rightNumber);
    CHR (hr);

    right.type   = ioNode.type;
    right.height = ioNode.height;
    right.fLink  = ioNode.fLink;
    right.bLink  = number;
    right.records.assign (ioNode.records.begin() + (ptrdiff_t) at, ioNode.records.end());
    ioNode.records.resize (at);

    if (ioNode.fLink != 0)
    {
        Node  next;

        hr = ReadNode (ioNode.fLink, next);
        CHR (hr);

        next.bLink = rightNumber;
        WriteNode (ioNode.fLink, next);
    }
    else if (!isIndex)
    {
        SetHeaderLong (kHeaderLastLeaf, rightNumber);
    }

    ioNode.fLink = rightNumber;

    WriteNode (number,      ioNode);
    WriteNode (rightNumber, right);

    outChange.isSplit    = true;
    outChange.splitEntry = MakeEntry (right.records[0].key, rightNumber);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::FindBalancedSplit
//
//  The first record of the second node, chosen so both nodes fit and their
//  bytes are as close to equal as they can be.
//
////////////////////////////////////////////////////////////////////////////////

size_t HfsBTreeEditor::FindBalancedSplit (const std::vector<Record> & records, bool isIndex) const
{
    size_t  total = GetPayload (records, isIndex);
    size_t  left  = 0;
    size_t  best  = records.size() / 2;
    size_t  gap   = SIZE_MAX;



    for (size_t at = 1; at < records.size(); at++)
    {
        size_t  difference = 0;

        left       += GetRecordSize (records[at - 1], isIndex);
        difference  = (2 * left > total) ? 2 * left - total : total - 2 * left;

        if (left <= kCapacity && total - left <= kCapacity && difference < gap)
        {
            best = at;
            gap  = difference;
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Remove
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::Remove (const std::vector<Byte> & key)
{
    HRESULT   hr   = S_OK;
    uint32_t  root = GetRoot();
    Change    change;
    Node      node;



    CBREx (root != 0, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = RemoveFrom (root, key, true, change);
    CHR (hr);

    hr = ReadNode (root, node);
    CHR (hr);

    if (node.type == HfsBTree::kNodeLeaf && node.records.empty())
    {
        FreeNode (root);

        SetHeaderLong (kHeaderRoot,      0);
        SetHeaderLong (kHeaderFirstLeaf, 0);
        SetHeaderLong (kHeaderLastLeaf,  0);
        SetDepth (0);
    }

    while (node.type == HfsBTree::kNodeIndex && node.records.size() == 1)
    {
        uint32_t  child = HfsBTree::GetLong (node.records[0].data, 0);

        FreeNode (root);
        root = child;

        SetHeaderLong (kHeaderRoot, root);
        SetDepth (GetDepth() - 1);

        hr = ReadNode (root, node);
        CHR (hr);
    }

    SetHeaderLong (kHeaderLeafRecords, GetHeaderLong (kHeaderLeafRecords) - 1);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::RemoveFrom
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::RemoveFrom (uint32_t number, const std::vector<Byte> & key, bool isRoot, Change & outChange)
{
    HRESULT  hr = S_OK;
    Node     node;



    outChange = Change();

    hr = ReadNode (number, node);
    CHR (hr);

    if (node.type == HfsBTree::kNodeLeaf)
    {
        size_t  at    = 0;
        bool    found = false;

        while (at < node.records.size() && CompareKeys (node.records[at].key, key) != 0)
        {
            at++;
        }

        found = at < node.records.size();
        CBREx (found, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

        node.records.erase (node.records.begin() + (ptrdiff_t) at);

        if (at == 0 && !node.records.empty())
        {
            outChange.isFirstChanged = true;
            outChange.firstKey       = node.records[0].key;
        }
    }
    else
    {
        bool      isIndex = node.type == HfsBTree::kNodeIndex && !node.records.empty();
        size_t    at      = 0;
        uint32_t  child   = 0;
        Change    below;

        CBREx (isIndex, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        at    = GetChildIndex (node, key);
        child = HfsBTree::GetLong (node.records[at].data, 0);

        hr = RemoveFrom (child, key, false, below);
        CHR (hr);

        if (below.isFirstChanged)
        {
            node.records[at].key = HfsBTree::MakeIndexKey (m_kind, below.firstKey);

            if (at == 0)
            {
                outChange.isFirstChanged = true;
                outChange.firstKey       = below.firstKey;
            }
        }

        if (below.isShort && node.records.size() > 1)
        {
            hr = Rebalance (node, at, outChange);
            CHR (hr);
        }
    }

    WriteNode (number, node);

    outChange.isShort = !isRoot && GetPayload (node) < kCapacity / 2;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Rebalance
//
//  The short child and the sibling after it, or before it for the last
//  child, merged into the first of the two when both fit in one node, and
//  otherwise their records shared out evenly between them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::Rebalance (Node & ioParent, size_t child, Change & outChange)
{
    HRESULT              hr         = S_OK;
    size_t               left       = (child + 1 < ioParent.records.size()) ? child : child - 1;
    size_t               right      = left + 1;
    uint32_t             leftNumber = HfsBTree::GetLong (ioParent.records[left].data, 0);
    uint32_t             rightNum   = HfsBTree::GetLong (ioParent.records[right].data, 0);
    bool                 isIndex    = false;
    std::vector<Record>  combined;
    std::vector<Byte>    firstKey;
    Node                 leftNode;
    Node                 rightNode;



    hr = ReadNode (leftNumber, leftNode);
    CHR (hr);

    hr = ReadNode (rightNum, rightNode);
    CHR (hr);

    isIndex  = leftNode.type == HfsBTree::kNodeIndex;
    combined = leftNode.records;
    combined.insert (combined.end(), rightNode.records.begin(), rightNode.records.end());

    if (GetPayload (combined, isIndex) <= kCapacity)
    {
        leftNode.records = std::move (combined);
        leftNode.fLink   = rightNode.fLink;

        if (rightNode.fLink != 0)
        {
            Node  next;

            hr = ReadNode (rightNode.fLink, next);
            CHR (hr);

            next.bLink = leftNumber;
            WriteNode (rightNode.fLink, next);
        }
        else if (!isIndex)
        {
            SetHeaderLong (kHeaderLastLeaf, leftNumber);
        }

        WriteNode (leftNumber, leftNode);
        FreeNode  (rightNum);

        ioParent.records.erase (ioParent.records.begin() + (ptrdiff_t) right);
    }
    else
    {
        size_t  at = FindBalancedSplit (combined, isIndex);

        leftNode.records.assign  (combined.begin(), combined.begin() + (ptrdiff_t) at);
        rightNode.records.assign (combined.begin() + (ptrdiff_t) at, combined.end());

        WriteNode (leftNumber, leftNode);
        WriteNode (rightNum,   rightNode);

        ioParent.records[right].key = HfsBTree::MakeIndexKey (m_kind, rightNode.records[0].key);
    }

    firstKey = HfsBTree::MakeIndexKey (m_kind, leftNode.records[0].key);

    if (CompareKeys (ioParent.records[left].key, firstKey) != 0)
    {
        ioParent.records[left].key = firstKey;

        if (left == 0)
        {
            outChange.isFirstChanged = true;
            outChange.firstKey       = leftNode.records[0].key;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Replace
//
//  In place when the record fits in its leaf, and otherwise removed and
//  inserted again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTreeEditor::Replace (const Record & record)
{
    HRESULT   hr    = S_OK;
    uint32_t  leaf  = 0;
    size_t    at    = 0;
    bool      found = false;
    Node      node;



    hr = FindLeaf (record.key, leaf, node);
    CHR (hr);

    while (at < node.records.size() && CompareKeys (node.records[at].key, record.key) != 0)
    {
        at++;
    }

    found = at < node.records.size();
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    node.records[at].data = record.data;

    if (GetPayload (node) <= kCapacity)
    {
        WriteNode (leaf, node);
    }
    else
    {
        hr = Remove (record.key);
        CHR (hr);

        hr = Insert (record);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::Check
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTreeEditor::Check (std::string & outProblems) const
{
    std::set<uint32_t>                  seen;
    std::set<uint32_t>                  maps;
    std::vector<std::vector<uint32_t>>  levels;
    uint32_t                            records = 0;
    uint32_t                            root    = GetRoot();
    uint32_t                            depth   = GetDepth();
    uint32_t                            used    = 1;
    uint32_t                            steps   = 0;



    outProblems.clear();

    if (m_nodes.size() < HfsBTree::kNodeBytes || m_nodes[8] != HfsBTree::kNodeHeader)
    {
        outProblems += "node 0 is not a header node\n";
        return;
    }

    if (root == 0)
    {
        if (depth != 0 || GetHeaderLong (kHeaderLeafRecords) != 0 || GetHeaderLong (kHeaderFirstLeaf) != 0 || GetHeaderLong (kHeaderLastLeaf) != 0)
        {
            outProblems += "an empty tree has a depth, records or leaves in its header\n";
        }
    }
    else
    {
        levels.resize (depth);
        CheckNode (root, (int) depth, nullptr, seen, levels, records, outProblems);
    }

    //  Each level's sibling links, in key order, and the leaves at its ends.
    for (size_t level = 0; level < levels.size(); level++)
    {
        const std::vector<uint32_t> &  nodes = levels[level];
        std::vector<Byte>              lastKey;

        for (size_t at = 0; at < nodes.size(); at++)
        {
            Node      node;
            uint32_t  before = (at > 0) ? nodes[at - 1] : 0;
            uint32_t  after  = (at + 1 < nodes.size()) ? nodes[at + 1] : 0;

            HRESULT   read   = ReadNode (nodes[at], node);

            if (FAILED (read))
            {
                continue;
            }

            if (node.bLink != before || node.fLink != after)
            {
                outProblems += std::format ("node {}: links {} back and {} forward, not {} and {}\n", nodes[at], node.bLink, node.fLink, before, after);
            }

            if (!lastKey.empty() && !node.records.empty() && CompareKeys (lastKey, node.records[0].key) >= 0)
            {
                outProblems += std::format ("node {}: its first key isn't after the previous node's last\n", nodes[at]);
            }

            lastKey = node.records.empty() ? lastKey : node.records.back().key;
        }
    }

    if (!levels.empty() && !levels[0].empty())
    {
        if (GetHeaderLong (kHeaderFirstLeaf) != levels[0].front() || GetHeaderLong (kHeaderLastLeaf) != levels[0].back())
        {
            outProblems += "the header's first or last leaf is wrong\n";
        }
    }

    if (records != GetHeaderLong (kHeaderLeafRecords))
    {
        outProblems += std::format ("the header counts {} records; the leaves have {}\n", GetHeaderLong (kHeaderLeafRecords), records);
    }

    //  The node map: the header, the map nodes and the tree's nodes in use,
    //  and no others.
    for (uint32_t map = HfsBTree::GetLong (m_nodes, 0); map != 0 && map < GetTotalNodes() && ++steps <= GetTotalNodes(); map = HfsBTree::GetLong (m_nodes, (size_t) map * HfsBTree::kNodeBytes))
    {
        maps.insert (map);
    }

    for (uint32_t number = 0; number < GetTotalNodes(); number++)
    {
        bool  inUse  = number == 0 || seen.count (number) > 0 || maps.count (number) > 0;
        bool  marked = (number == 0) ? (m_nodes[kHeaderMap] & 0x80) != 0 : IsNodeUsed (number);

        if (inUse != marked)
        {
            outProblems += std::format ("node {}: the map marks it {}\n", number, marked ? "in use, but nothing uses it" : "free, but the tree uses it");
        }

        used += (number > 0 && inUse) ? 1 : 0;
    }

    if (GetFreeNodes() != GetTotalNodes() - used)
    {
        outProblems += std::format ("the header counts {} free nodes; {} are\n", GetFreeNodes(), GetTotalNodes() - used);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTreeEditor::CheckNode
//
//  One node and the nodes below it. `lowKey` is the key of the index entry
//  that points to the node, and null for the root.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTreeEditor::CheckNode (uint32_t                              number,
                                int                                   height,
                                const std::vector<Byte>             * lowKey,
                                std::set<uint32_t>                  & ioSeen,
                                std::vector<std::vector<uint32_t>>  & ioLevels,
                                uint32_t                            & ioRecords,
                                std::string                         & outProblems) const
{
    HRESULT  read    = S_OK;
    Node     node;
    bool     isLeaf  = height == 1;
    bool     isIndex = false;



    if (height < 1 || !ioSeen.insert (number).second)
    {
        outProblems += std::format ("node {}: reached twice, or below the leaves\n", number);
        return;
    }

    read = ReadNode (number, node);

    if (FAILED (read))
    {
        outProblems += std::format ("node {}: unreadable\n", number);
        return;
    }

    isIndex = node.type == HfsBTree::kNodeIndex;

    if (node.height != height || (isLeaf ? node.type != HfsBTree::kNodeLeaf : !isIndex))
    {
        outProblems += std::format ("node {}: height {} and type {:02X}, at height {}\n", number, node.height, node.type, height);
    }

    if (node.records.empty())
    {
        outProblems += std::format ("node {}: empty\n", number);
        return;
    }

    for (size_t at = 1; at < node.records.size(); at++)
    {
        if (CompareKeys (node.records[at - 1].key, node.records[at].key) >= 0)
        {
            outProblems += std::format ("node {}: record {} isn't after record {}\n", number, at, at - 1);
        }
    }

    if (lowKey != nullptr && CompareKeys (*lowKey, node.records[0].key) != 0)
    {
        outProblems += std::format ("node {}: its index entry's key isn't its first key\n", number);
    }

    if (lowKey != nullptr && GetPayload (node) < GetMinimumFill (isIndex))
    {
        outProblems += std::format ("node {}: {} bytes of records, under half full\n", number, GetPayload (node));
    }

    ioLevels[(size_t) height - 1].push_back (number);

    if (isLeaf)
    {
        ioRecords += (uint32_t) node.records.size();
        return;
    }

    for (const Record & entry : node.records)
    {
        CheckNode (HfsBTree::GetLong (entry.data, 0), height - 1, &entry.key, ioSeen, ioLevels, ioRecords, outProblems);
    }
}
