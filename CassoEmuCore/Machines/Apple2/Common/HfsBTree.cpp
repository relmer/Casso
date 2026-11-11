#include "Pch.h"

#include "Machines/Apple2/Common/HfsBTree.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::FoldName
//
//  A Mac OS Roman character's place in the Finder's order: ASCII in code
//  order with lower case folded to upper, an accented letter just after
//  its letter, accents in a fixed order, and every other character after
//  ASCII in its code's order. Returned as the low byte of a rank; see
//  CompareNames.
//
////////////////////////////////////////////////////////////////////////////////

Byte HfsBTree::FoldName (Byte character)
{
    return (character >= 'a' && character <= 'z') ? (Byte) (character - 'a' + 'A') : character;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::CompareNames
//
//  Each character is ranked: an ASCII character, folded to upper case, at
//  16 times its code; an accented letter at 16 times its letter plus its
//  accent, 1 acute, 2 grave, 3 circumflex, 4 diaeresis, 5 tilde, 6 ring and
//  7 cedilla, whatever its case; any other character above all of those.
//  A shorter name that starts the longer sorts first.
//
////////////////////////////////////////////////////////////////////////////////

int HfsBTree::CompareNames (const Byte * a, size_t aLength, const Byte * b, size_t bLength)
{
    struct Accented
    {
        Byte  code;
        Byte  letter;
        Byte  accent;
    };

    static constexpr Accented  s_kAccented[] =
    {
        { 0xE7, 'A', 1 }, { 0xCB, 'A', 2 }, { 0xE5, 'A', 3 }, { 0x80, 'A', 4 }, { 0xCC, 'A', 5 }, { 0x81, 'A', 6 }, { 0x82, 'C', 7 },
        { 0x83, 'E', 1 }, { 0xE9, 'E', 2 }, { 0xE6, 'E', 3 }, { 0xE8, 'E', 4 }, { 0xEA, 'I', 1 }, { 0xED, 'I', 2 }, { 0xEB, 'I', 3 },
        { 0xEC, 'I', 4 }, { 0x84, 'N', 5 }, { 0xEE, 'O', 1 }, { 0xF1, 'O', 2 }, { 0xEF, 'O', 3 }, { 0x85, 'O', 4 }, { 0xCD, 'O', 5 },
        { 0xF2, 'U', 1 }, { 0xF4, 'U', 2 }, { 0xF3, 'U', 3 }, { 0x86, 'U', 4 }, { 0xD9, 'Y', 4 },
        { 0x87, 'A', 1 }, { 0x88, 'A', 2 }, { 0x89, 'A', 3 }, { 0x8A, 'A', 4 }, { 0x8B, 'A', 5 }, { 0x8C, 'A', 6 }, { 0x8D, 'C', 7 },
        { 0x8E, 'E', 1 }, { 0x8F, 'E', 2 }, { 0x90, 'E', 3 }, { 0x91, 'E', 4 }, { 0x92, 'I', 1 }, { 0x93, 'I', 2 }, { 0x94, 'I', 3 },
        { 0x95, 'I', 4 }, { 0x96, 'N', 5 }, { 0x97, 'O', 1 }, { 0x98, 'O', 2 }, { 0x99, 'O', 3 }, { 0x9A, 'O', 4 }, { 0x9B, 'O', 5 },
        { 0x9C, 'U', 1 }, { 0x9D, 'U', 2 }, { 0x9E, 'U', 3 }, { 0x9F, 'U', 4 }, { 0xD8, 'Y', 4 },
    };

    static constexpr int  s_kRankScale = 16;
    static constexpr int  s_kHighRank  = 0x800;

    auto  rank = [] (Byte character)
    {
        int  value = (character < 0x80) ? FoldName (character) * s_kRankScale : s_kHighRank + character;

        for (const Accented & accented : s_kAccented)
        {
            if (accented.code == character)
            {
                value = accented.letter * s_kRankScale + accented.accent;
            }
        }

        return value;
    };

    size_t  common = std::min (aLength, bLength);



    for (size_t i = 0; i < common; i++)
    {
        int  difference = rank (a[i]) - rank (b[i]);

        if (difference != 0)
        {
            return difference;
        }
    }

    return (int) aLength - (int) bLength;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::CompareCatalogKeys
//
//  A key is its length, a reserved byte, the parent ID and the name as a
//  Pascal string.
//
////////////////////////////////////////////////////////////////////////////////

int HfsBTree::CompareCatalogKeys (const std::vector<Byte> & a, const std::vector<Byte> & b)
{
    return CompareCatalogKeys (a.data(), a.size(), b.data(), b.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::CompareCatalogKeys
//
//  Over a key's bytes where they are, in a node or a vector. Bytes past
//  `size` read as zero.
//
////////////////////////////////////////////////////////////////////////////////

int HfsBTree::CompareCatalogKeys (const Byte * a, size_t aSize, const Byte * b, size_t bSize)
{
    uint32_t  aParent = GetKeyLong (a, aSize, 2);
    uint32_t  bParent = GetKeyLong (b, bSize, 2);
    size_t    aLength = (aSize > 6) ? std::min<size_t> (a[6], aSize - 7) : 0;
    size_t    bLength = (bSize > 6) ? std::min<size_t> (b[6], bSize - 7) : 0;



    if (aParent != bParent)
    {
        return (aParent < bParent) ? -1 : 1;
    }

    return CompareNames (a + 7, aLength, b + 7, bLength);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::CompareExtentsKeys
//
//  A key is its length, the fork type, the file ID and the first allocation
//  block of the fork the record covers, compared by file, fork and block.
//
////////////////////////////////////////////////////////////////////////////////

int HfsBTree::CompareExtentsKeys (const std::vector<Byte> & a, const std::vector<Byte> & b)
{
    return CompareExtentsKeys (a.data(), a.size(), b.data(), b.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::CompareExtentsKeys
//
//  Over a key's bytes where they are. Bytes past `size` read as zero.
//
////////////////////////////////////////////////////////////////////////////////

int HfsBTree::CompareExtentsKeys (const Byte * a, size_t aSize, const Byte * b, size_t bSize)
{
    uint32_t  aFile  = GetKeyLong (a, aSize, 2);
    uint32_t  bFile  = GetKeyLong (b, bSize, 2);
    int       aFork  = (aSize > 1) ? a[1] : 0;
    int       bFork  = (bSize > 1) ? b[1] : 0;
    int       aStart = (aSize > 7) ? (a[6] << 8) | a[7] : 0;
    int       bStart = (bSize > 7) ? (b[6] << 8) | b[7] : 0;



    if (aFile != bFile)
    {
        return (aFile < bFile) ? -1 : 1;
    }

    if (aFork != bFork)
    {
        return aFork - bFork;
    }

    return aStart - bStart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::CompareKeys
//
////////////////////////////////////////////////////////////////////////////////

int HfsBTree::CompareKeys (Kind kind, const Byte * a, size_t aSize, const Byte * b, size_t bSize)
{
    return (kind == Kind::Catalog) ? CompareCatalogKeys (a, aSize, b, bSize) : CompareExtentsKeys (a, aSize, b, bSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::GetKeyLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsBTree::GetKeyLong (const Byte * key, size_t size, size_t at)
{
    return (at + 3 < size) ? ((uint32_t) key[at] << 24) | ((uint32_t) key[at + 1] << 16) | ((uint32_t) key[at + 2] << 8) | key[at + 3] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::Sort
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTree::Sort (Kind kind, std::vector<Record> & ioRecords)
{
    std::stable_sort (ioRecords.begin(), ioRecords.end(), [kind] (const Record & a, const Record & b)
    {
        return (kind == Kind::Catalog ? CompareCatalogKeys (a.key, b.key) : CompareExtentsKeys (a.key, b.key)) < 0;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::GetRecordBytes
//
//  A leaf record's key is padded to an even length; an index record's key
//  is the tree's longest, and its data the child's node number.
//
////////////////////////////////////////////////////////////////////////////////

size_t HfsBTree::GetRecordBytes (Kind kind, const Record & record, bool isIndex)
{
    size_t  key = record.key.size();



    if (isIndex)
    {
        return MakeIndexKey (kind, record.key).size() + 4;
    }

    return key + (key % 2) + record.data.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::MakeIndexKey
//
//  A catalog index key is always the longest catalog key, its name padded
//  with zeros; an extents key is one length.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> HfsBTree::MakeIndexKey (Kind kind, const std::vector<Byte> & key)
{
    std::vector<Byte>  index = key;



    if (kind == Kind::Catalog)
    {
        index.resize ((size_t) kCatalogKeyMax + 1, 0);
        index[0] = kCatalogKeyMax;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::PackLevel
//
//  The records, by index, into nodes in order, each node as full as its
//  records and their offsets allow.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTree::PackLevel (Kind kind, const std::vector<Record> & records, bool isIndex, std::vector<std::vector<size_t>> & outNodes)
{
    size_t  used = kDescriptorBytes + 2;



    outNodes.clear();

    for (size_t at = 0; at < records.size(); at++)
    {
        size_t  bytes = GetRecordBytes (kind, records[at], isIndex);

        if (outNodes.empty() || used + bytes + 2 > kNodeBytes)
        {
            outNodes.emplace_back();
            used = kDescriptorBytes + 2;
        }

        outNodes.back().push_back (at);
        used += bytes + 2;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::CountNodes
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsBTree::CountNodes (Kind kind, const std::vector<Record> & records)
{
    std::vector<std::vector<size_t>>  nodes;
    std::vector<Record>               level = records;
    uint32_t                          count = 1;
    bool                              index = false;



    while (!level.empty())
    {
        std::vector<Record>  parents;

        PackLevel (kind, level, index, nodes);
        count += (uint32_t) nodes.size();

        if (nodes.size() <= 1)
        {
            break;
        }

        for (const std::vector<size_t> & node : nodes)
        {
            parents.push_back (Record { MakeIndexKey (kind, level[node.front()].key), std::vector<Byte> (4, 0) });
        }

        level = std::move (parents);
        index = true;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::Build
//
//  Node 0 is the header, map nodes follow when more nodes than the header's
//  map counts are in the tree, then the leaves and each index level above
//  them, every node in use numbered from 0 with none free between.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HfsBTree::Build (Kind kind, const std::vector<Record> & records, uint32_t totalNodes, std::vector<Byte> & outNodes)
{
    struct Level
    {
        std::vector<Record>               records;
        std::vector<std::vector<size_t>>  nodes;
        uint32_t                          first = 0;
    };

    HRESULT             hr        = S_OK;
    uint32_t            headerMap = (uint32_t) kHeaderMapBytes * 8;
    uint32_t            mapNodes  = (totalNodes > headerMap) ? (totalNodes - headerMap + kMapNodeBytes * 8 - 1) / (uint32_t) (kMapNodeBytes * 8) : 0;
    uint32_t            next      = 1 + mapNodes;
    uint32_t            used      = 0;
    uint32_t            root      = 0;
    std::vector<Level>  levels;
    std::vector<Byte>   header (kNodeBytes, 0);



    outNodes.assign ((size_t) totalNodes * kNodeBytes, 0);

    //  Each level's nodes, from the leaves up, until one node is the root.
    if (!records.empty())
    {
        levels.push_back (Level { records, {}, 0 });

        for (;;)
        {
            Level &  level = levels.back();

            PackLevel (kind, level.records, levels.size() > 1, level.nodes);
            level.first = next;
            next       += (uint32_t) level.nodes.size();

            if (level.nodes.size() <= 1)
            {
                break;
            }

            Level  parent;

            for (size_t node = 0; node < level.nodes.size(); node++)
            {
                std::vector<Byte>  pointer (4, 0);

                PutLong (pointer, 0, level.first + (uint32_t) node);
                parent.records.push_back (Record { MakeIndexKey (kind, level.records[level.nodes[node].front()].key), pointer });
            }

            levels.push_back (std::move (parent));
        }

        root = levels.back().first;
    }

    used = next;
    CBREx (used <= totalNodes, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    //  The leaves and the index nodes.
    for (size_t height = 0; height < levels.size(); height++)
    {
        const Level &  level = levels[height];
        bool           index = height > 0;

        for (size_t node = 0; node < level.nodes.size(); node++)
        {
            uint32_t  number = level.first + (uint32_t) node;
            size_t    base   = (size_t) number * kNodeBytes;
            size_t    at     = kDescriptorBytes;
            size_t    count  = level.nodes[node].size();

            std::vector<Byte>  bytes (kNodeBytes, 0);

            PutLong (bytes, 0, (node + 1 < level.nodes.size()) ? number + 1 : 0);
            PutLong (bytes, 4, (node > 0) ? number - 1 : 0);
            bytes[8]  = index ? kNodeIndex : kNodeLeaf;
            bytes[9]  = (Byte) (height + 1);
            PutWord (bytes, 10, (uint32_t) count);

            for (size_t slot = 0; slot < count; slot++)
            {
                const Record &     record = level.records[level.nodes[node][slot]];
                std::vector<Byte>  key    = index ? MakeIndexKey (kind, record.key) : record.key;

                PutWord (bytes, kNodeBytes - 2 * (slot + 1), (uint32_t) at);

                std::copy (key.begin(), key.end(), bytes.begin() + (ptrdiff_t) at);
                at += key.size() + (index ? 0 : key.size() % 2);

                std::copy (record.data.begin(), record.data.end(), bytes.begin() + (ptrdiff_t) at);
                at += record.data.size();
            }

            PutWord (bytes, kNodeBytes - 2 * (count + 1), (uint32_t) at);

            std::copy (bytes.begin(), bytes.end(), outNodes.begin() + (ptrdiff_t) base);
        }
    }

    //  The header node: its record, the user record and the map.
    PutLong (header, 0, (mapNodes > 0) ? 1 : 0);
    header[8] = kNodeHeader;
    PutWord (header, 10, 3);

    PutWord (header, 14, (uint32_t) levels.size());
    PutLong (header, 16, root);
    PutLong (header, 20, (uint32_t) records.size());
    PutLong (header, 24, levels.empty() ? 0 : levels.front().first);
    PutLong (header, 28, levels.empty() ? 0 : levels.front().first + (uint32_t) levels.front().nodes.size() - 1);
    PutWord (header, 32, kNodeBytes);
    PutWord (header, 34, (kind == Kind::Catalog) ? kCatalogKeyMax : kExtentsKeyMax);
    PutLong (header, 36, totalNodes);
    PutLong (header, 40, totalNodes - used);

    PutWord (header, kNodeBytes - 2, kDescriptorBytes);
    PutWord (header, kNodeBytes - 4, kDescriptorBytes + kHeaderRecordBytes);
    PutWord (header, kNodeBytes - 6, kDescriptorBytes + kHeaderRecordBytes + kUserRecordBytes);
    PutWord (header, kNodeBytes - 8, kDescriptorBytes + kHeaderRecordBytes + kUserRecordBytes + kHeaderMapBytes);

    for (uint32_t node = 0; node < used && node < headerMap; node++)
    {
        header[kDescriptorBytes + kHeaderRecordBytes + kUserRecordBytes + node / 8] |= (Byte) (0x80 >> (node % 8));
    }

    std::copy (header.begin(), header.end(), outNodes.begin());

    //  Map nodes, each with a bit for the nodes past those of the one before.
    for (uint32_t map = 0; map < mapNodes; map++)
    {
        std::vector<Byte>  bytes (kNodeBytes, 0);
        uint32_t           first = headerMap + map * (uint32_t) (kMapNodeBytes * 8);

        PutLong (bytes, 0, (map + 1 < mapNodes) ? map + 2 : 0);
        bytes[8] = kNodeMap;
        PutWord (bytes, 10, 1);
        PutWord (bytes, kNodeBytes - 2, kDescriptorBytes);
        PutWord (bytes, kNodeBytes - 4, kDescriptorBytes + kMapNodeBytes);

        for (uint32_t node = first; node < used && node < first + kMapNodeBytes * 8; node++)
        {
            bytes[kDescriptorBytes + (node - first) / 8] |= (Byte) (0x80 >> ((node - first) % 8));
        }

        std::copy (bytes.begin(), bytes.end(), outNodes.begin() + (ptrdiff_t) ((size_t) (map + 1) * kNodeBytes));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::PutWord
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTree::PutWord (std::vector<Byte> & bytes, size_t at, uint32_t value)
{
    bytes[at]     = (Byte) (value >> 8);
    bytes[at + 1] = (Byte) value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::PutLong
//
////////////////////////////////////////////////////////////////////////////////

void HfsBTree::PutLong (std::vector<Byte> & bytes, size_t at, uint32_t value)
{
    PutWord (bytes, at,     value >> 16);
    PutWord (bytes, at + 2, value & 0xFFFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBTree::GetLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsBTree::GetLong (const std::vector<Byte> & bytes, size_t at)
{
    return (at + 3 < bytes.size()) ? ((uint32_t) bytes[at] << 24) | ((uint32_t) bytes[at + 1] << 16) | ((uint32_t) bytes[at + 2] << 8) | bytes[at + 3] : 0;
}
