#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/HfsBlockLayout.h"
#include "Machines/Apple2/Common/HfsVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  The fields of the boot blocks and the master directory block, from Inside
//  Macintosh: Files, 2-57 and 2-60: offset, length and label. A length of 2
//  or 4 is a big-endian number, any other a run of bytes.
//
////////////////////////////////////////////////////////////////////////////////

struct HfsFixedField
{
    uint32_t  offset;
    uint32_t  length;
    LPCWSTR   label;
};


static constexpr HfsFixedField s_kBootFields[] =
{
    {   0,  2, L"Boot block signature"       },
    {   2,  4, L"Entry point"                },
    {   6,  2, L"Boot blocks version"        },
    {   8,  2, L"Page flags"                 },
    {  10, 16, L"System file name"           },
    {  26, 16, L"Finder name"                },
    {  42, 16, L"Debugger name"              },
    {  58, 16, L"Second debugger name"       },
    {  74, 16, L"Startup screen name"        },
    {  90, 16, L"Startup program name"       },
    { 106, 16, L"Clipboard file name"        },
    { 122,  2, L"File control blocks"        },
    { 124,  2, L"Event queue size"           },
    { 126,  4, L"System heap on a 128K Mac"  },
    { 130,  4, L"System heap on a 256K Mac"  },
    { 134,  4, L"System heap size"           },
    { 140,  4, L"Extra system heap"          },
    { 144,  4, L"System heap fraction"       },
};


static constexpr HfsFixedField s_kMdbFields[] =
{
    {   0,  2, L"Signature"                     },
    {   2,  4, L"Created"                       },
    {   6,  4, L"Modified"                      },
    {  10,  2, L"Attributes"                    },
    {  12,  2, L"Files in the root directory"   },
    {  16,  2, L"Next allocation search"        },
    {  18,  2, L"Allocation blocks"             },
    {  20,  4, L"Allocation block size"         },
    {  24,  4, L"Clump size"                    },
    {  30,  4, L"Next catalog node ID"          },
    {  34,  2, L"Free allocation blocks"        },
    {  36, 28, L"Volume name"                   },
    {  64,  4, L"Last backup"                   },
    {  68,  2, L"Backup sequence number"        },
    {  70,  4, L"Write count"                   },
    {  74,  4, L"Extents file clump size"       },
    {  78,  4, L"Catalog file clump size"       },
    {  82,  2, L"Directories in the root"       },
    {  84,  4, L"Files on the volume"           },
    {  88,  4, L"Directories on the volume"     },
    {  92, 32, L"Finder information"            },
    { 124,  2, L"Volume cache size"             },
    { 126,  2, L"Bitmap cache size"             },
    { 128,  2, L"Common cache size"             },
    { 130,  4, L"Extents overflow file size"    },
    { 146,  4, L"Catalog file size"             },
};





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::ReadBlock (const BlockImage & image, uint32_t block, Block & outBlock)
{
    size_t  start = (size_t) block * kBlockBytes;
    size_t  i     = 0;



    outBlock.fill (0);

    for (i = 0; i < kBlockBytes && start + i < image.size(); i++)
    {
        outBlock[i] = image[start + i];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::ReadExtents
//
//  An extent record's three extents, each a first allocation block and a
//  count, leaving out the empty ones.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::ReadExtents (const Byte * at, vector<Extent> & outExtents)
{
    uint32_t  i      = 0;
    Extent    extent;



    for (i = 0; i < kExtentCount; i++)
    {
        extent.start = GetWord (at + i * kExtentBytes);
        extent.count = GetWord (at + i * kExtentBytes + 2);

        if (extent.count != 0)
        {
            outExtents.push_back (extent);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::MarkBlock
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::MarkBlock (
    BlockMap        & map,
    uint32_t          block,
    SectorRole        role,
    BlockStructure    structure,
    int               owner,
    uint32_t          tree,
    uint32_t          node)
{
    BlockMapEntry  * entry = nullptr;



    if (block >= map.GetBlockCount())
    {
        return;
    }

    entry = &map.blocks[block];

    if (entry->structure == BlockStructure::None)
    {
        entry->role      = role;
        entry->structure = structure;
        entry->tree      = tree;
        entry->node      = node;
    }

    if (owner >= 0 && std::find (entry->owners.begin(), entry->owners.end(), owner) == entry->owners.end())
    {
        entry->owners.push_back (owner);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::Walk
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::Walk (const BlockImage & image, BlockMap & outMap)
{
    constexpr uint32_t  kBitsPerBlock = kBlockBytes * 8;
    constexpr uint32_t  kNameMax      = 27;
    constexpr int       kExtentsOwner = 0;
    constexpr int       kCatalogOwner = 1;



    Walker          walker      { image, outMap, {}, {}, {} };
    Block           mdb         = {};
    uint32_t        count       = (uint32_t) (image.size() / kBlockBytes);
    uint32_t        allocations = 0;
    uint32_t        bitmap      = 0;
    uint32_t        i           = 0;
    vector<Extent>  extents;
    vector<Extent>  catalog;



    outMap            = BlockMap();
    outMap.fileSystem = BlockFileSystem::Hfs;
    outMap.hasFreeMap = true;
    outMap.blocks.assign (count, BlockMapEntry());

    ReadBlock (image, kMdbBlock, mdb);

    allocations                = GetWord (&mdb[kAllocBlocks]);
    outMap.allocationStart     = GetWord (&mdb[kAllocStart]);
    outMap.blocksPerAllocation = std::max<uint32_t> (1, GetLong (&mdb[kAllocSize]) / kBlockBytes);
    outMap.volumeName          = DecodeName (&mdb[kVolumeName + 1], std::min<uint32_t> (mdb[kVolumeName], kNameMax));

    MarkBlock (outMap, 0,         SectorRole::BootArea,       BlockStructure::HfsBootBlock,       -1, 0, 0);
    MarkBlock (outMap, 1,         SectorRole::BootArea,       BlockStructure::HfsBootBlock,       -1, 0, 1);
    MarkBlock (outMap, kMdbBlock, SectorRole::VtocOrKeyBlock, BlockStructure::HfsMasterDirectory, -1);

    if (count > kMdbBlock + 2)
    {
        MarkBlock (outMap, count - 2, SectorRole::VtocOrKeyBlock, BlockStructure::HfsAlternateMasterDirectory, -1);
    }

    bitmap = GetWord (&mdb[kVbmStart]);

    for (i = 0; i < (allocations + kBitsPerBlock - 1) / kBitsPerBlock; i++)
    {
        MarkBlock (outMap, bitmap + i, SectorRole::VolumeBitmap, BlockStructure::HfsVolumeBitmap, -1, 0, i);
    }

    outMap.files.push_back (L"Extents overflow file");
    outMap.files.push_back (L"Catalog file");

    ReadExtents (&mdb[kExtentsExtent], extents);
    ReadExtents (&mdb[kCatalogExtent], catalog);

    MapTree (walker, extents, GetLong (&mdb[kExtentsSize]), outMap.extentsNodes);
    MapTree (walker, catalog, GetLong (&mdb[kCatalogSize]), outMap.catalogNodes);

    for (i = 0; i < outMap.extentsNodes.size(); i++)
    {
        MarkBlock (outMap, outMap.extentsNodes[i], SectorRole::CatalogOrDirectory, BlockStructure::HfsNode, kExtentsOwner, HfsVolume::kExtentsFileId, i);
    }

    for (i = 0; i < outMap.catalogNodes.size(); i++)
    {
        MarkBlock (outMap, outMap.catalogNodes[i], SectorRole::CatalogOrDirectory, BlockStructure::HfsNode, kCatalogOwner, HfsVolume::kCatalogFileId, i);
    }

    ReadOverflow (walker, outMap.extentsNodes);
    ReadCatalog  (walker, outMap.catalogNodes);

    for (const ForkRecord & fork : walker.forks)
    {
        ClaimFork (walker, fork);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::MapTree
//
//  The block of each node of a B-tree file, in node order, from its extents.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::MapTree (
    Walker                & walker,
    const vector<Extent>  & extents,
    uint32_t                treeBytes,
    vector<uint32_t>      & outNodes)
{
    uint32_t  nodes = std::min<uint32_t> (treeBytes / kBlockBytes, kMaxNodes);
    uint32_t  block = 0;
    uint32_t  i     = 0;
    uint32_t  k     = 0;



    outNodes.clear();

    for (const Extent & extent : extents)
    {
        for (i = 0; i < extent.count && outNodes.size() < nodes; i++)
        {
            for (k = 0; k < walker.map.blocksPerAllocation && outNodes.size() < nodes; k++)
            {
                block = walker.map.GetAllocationBlock (extent.start + i) + k;

                if (block >= walker.map.GetBlockCount())
                {
                    return;
                }

                outNodes.push_back (block);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::GetRecordOffset
//
//  A node's record offsets are stored back from its end, the first record's
//  in the last two bytes.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HfsBlockLayout::GetRecordOffset (const BlockFieldList & list, uint32_t record)
{
    return list.GetBigWord (kBlockBytes - 2 * (record + 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::ReadLeaves
//
//  Every record of a B-tree's leaf nodes, from the header node's first leaf
//  along the forward links, as the key's offset and the data's.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::ReadLeaves (
    Walker                                                          & walker,
    const vector<uint32_t>                                          & nodes,
    const std::function<void (const Block &, uint32_t, uint32_t)>   & visit)
{
    Block               bytes = {};
    vector<BlockField>  none;
    BlockFieldList      list (bytes, none);
    uint32_t            node  = 0;
    uint32_t            steps = 0;
    uint32_t            r     = 0;
    uint32_t            key   = 0;
    uint32_t            data  = 0;
    Word                count = 0;



    if (nodes.empty())
    {
        return;
    }

    ReadBlock (walker.image, nodes[0], bytes);

    node = GetLong (&bytes[kFirstLeaf]);

    while (node != 0 && node < nodes.size() && steps++ < nodes.size())
    {
        ReadBlock (walker.image, nodes[node], bytes);

        if (bytes[kNodeType] != kLeafNode)
        {
            walker.map.isCatalogComplete = false;
            break;
        }

        count = GetWord (&bytes[kRecordCount]);

        for (r = 0; r < count; r++)
        {
            key = GetRecordOffset (list, r);

            if (key < kDescriptor || key >= kBlockBytes)
            {
                break;
            }

            data  = key + 1 + bytes[key];
            data += data & 1;

            if (data < kBlockBytes)
            {
                visit (bytes, key, data);
            }
        }

        node = GetLong (&bytes[kForwardLink]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::ReadOverflow
//
//  Each extents overflow record is a fork type, a file ID and the first
//  allocation block of the fork's run that its three extents continue.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::ReadOverflow (Walker & walker, const vector<uint32_t> & nodes)
{
    constexpr uint32_t  kKeyFork  = 1;
    constexpr uint32_t  kKeyFile  = 2;
    constexpr uint32_t  kKeyStart = 6;
    constexpr uint32_t  kRecord   = kExtentCount * kExtentBytes;



    ReadLeaves (walker, nodes, [&] (const Block & bytes, uint32_t key, uint32_t data)
    {
        vector<Extent>  extents;

        if (data + kRecord > kBlockBytes)
        {
            return;
        }

        ReadExtents (&bytes[data], extents);

        walker.overflow[{ GetLong (&bytes[key + kKeyFile]), bytes[key + kKeyFork] }].push_back ({ GetWord (&bytes[key + kKeyStart]), extents });
    });

    for (auto & [fork, runs] : walker.overflow)
    {
        std::sort (runs.begin(), runs.end(), [] (const auto & a, const auto & b) { return a.first < b.first; });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::ReadCatalog
//
//  Each directory's ID, parent and name, then each file's two forks. A
//  catalog key is a length, a reserved byte, the parent directory's ID and
//  the name as a length and its characters.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::ReadCatalog (Walker & walker, const vector<uint32_t> & nodes)
{
    constexpr uint32_t  kKeyParent = 2;
    constexpr uint32_t  kKeyName   = 6;
    constexpr uint32_t  kFileBytes = kForkExtents + kExtentCount * kExtentBytes;
    constexpr uint32_t  kNameMax   = 31;



    struct FileRecord
    {
        uint32_t        parent = 0;
        std::wstring    name;
        uint32_t        id     = 0;
        vector<Extent>  data;
        vector<Extent>  fork;
    };

    vector<FileRecord>  files;



    ReadLeaves (walker, nodes, [&] (const Block & bytes, uint32_t key, uint32_t data)
    {
        uint32_t      parent = GetLong (&bytes[key + kKeyParent]);
        uint32_t      length = std::min<uint32_t> (bytes[key + kKeyName], kNameMax);
        std::wstring  name   = DecodeName (&bytes[key + kKeyName + 1], length);
        Byte          type   = bytes[data];
        FileRecord    file;

        if (type == kDirectory && data + kDirId + 4 <= kBlockBytes)
        {
            walker.folders[GetLong (&bytes[data + kDirId])] = { parent, name };
        }
        else if (type == kFile && data + kFileBytes <= kBlockBytes)
        {
            file.parent = parent;
            file.name   = name;
            file.id     = GetLong (&bytes[data + kFileId]);

            ReadExtents (&bytes[data + kDataExtents], file.data);
            ReadExtents (&bytes[data + kForkExtents], file.fork);

            files.push_back (std::move (file));
        }
    });

    for (FileRecord & file : files)
    {
        int  index = (int) walker.map.files.size();

        walker.map.files.push_back (GetPath (walker, file.parent, file.name));

        for (const auto & [forkType, first] : { std::pair<Byte, vector<Extent> *> (kDataFork, &file.data), std::pair<Byte, vector<Extent> *> (kResourceFork, &file.fork) })
        {
            ForkRecord  fork;
            auto        found = walker.overflow.find ({ file.id, forkType });

            fork.fileId  = file.id;
            fork.fork    = forkType;
            fork.file    = index;
            fork.extents = *first;

            if (found != walker.overflow.end())
            {
                for (const auto & run : found->second)
                {
                    fork.extents.insert (fork.extents.end(), run.second.begin(), run.second.end());
                }
            }

            walker.forks.push_back (std::move (fork));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::GetPath
//
//  A file's path from the root directory, whose name is the volume's and
//  isn't part of the path.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HfsBlockLayout::GetPath (const Walker & walker, uint32_t parent, const std::wstring & name)
{
    std::wstring  path  = name;
    uint32_t      at    = parent;
    uint32_t      depth = 0;



    while (at != kRootId && depth++ < kMaxDepth)
    {
        auto  found = walker.folders.find (at);

        if (found == walker.folders.end())
        {
            break;
        }

        path = found->second.name + L"/" + path;
        at   = found->second.parent;
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::ClaimFork
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::ClaimFork (Walker & walker, const ForkRecord & fork)
{
    uint32_t  node  = 0;
    uint32_t  i     = 0;
    uint32_t  k     = 0;
    uint32_t  limit = walker.map.GetBlockCount();



    for (const Extent & extent : fork.extents)
    {
        for (i = 0; i < extent.count && node < limit; i++)
        {
            for (k = 0; k < walker.map.blocksPerAllocation; k++)
            {
                MarkBlock (walker.map, walker.map.GetAllocationBlock (extent.start + i) + k, SectorRole::FileData, BlockStructure::FileData, fork.file, fork.fork, node++);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::DecodeName
//
//  Mac OS Roman's ASCII half as it is; the rest as '?'.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HfsBlockLayout::DecodeName (const Byte * at, size_t length)
{
    constexpr Byte  kFirstPrintable = 0x20;
    constexpr Byte  kLastPrintable  = 0x7E;



    std::wstring  name;
    size_t        i    = 0;



    for (i = 0; i < length; i++)
    {
        name += (at[i] >= kFirstPrintable && at[i] <= kLastPrintable) ? (wchar_t) at[i] : L'?';
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::Annotate
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::Annotate (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields)
{
    constexpr uint32_t  kBitsPerBlock = kBlockBytes * 8;



    BlockFieldList         list (bytes, outFields);
    const BlockMapEntry  & entry = map.blocks[block];
    uint32_t               first = 0;
    uint32_t               used  = 0;



    switch (entry.structure)
    {
        case BlockStructure::HfsBootBlock:
            if (entry.node == 0)
            {
                AnnotateBoot (list);
            }
            else
            {
                list.AddRun (0, kBlockBytes, L"Boot blocks, second block");
            }

            break;

        case BlockStructure::HfsMasterDirectory:
        case BlockStructure::HfsAlternateMasterDirectory:
            AnnotateMdb (map, list);
            break;

        case BlockStructure::HfsVolumeBitmap:
            first = entry.node * kBitsPerBlock;

            for (uint32_t i = 0; i < kBlockBytes; i++)
            {
                used += (uint32_t) std::popcount ((unsigned) list.GetByte (i));
            }

            list.AddRun (0, kBlockBytes,
                         L"Volume bitmap, allocation blocks " + std::to_wstring (first) + L" to " + std::to_wstring (first + kBitsPerBlock - 1),
                         std::to_wstring (used) + L" used");
            break;

        case BlockStructure::HfsNode:
            AnnotateNode (map, entry, list);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::AnnotateBoot
//
//  A volume that isn't startable has zeros for its boot blocks.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::AnnotateBoot (BlockFieldList & list)
{
    constexpr Word      kBootSignature = 0x4C4B;   // 'LK'
    constexpr uint32_t  kWordBytes     = 2;
    constexpr uint32_t  kLongBytes     = 4;



    if (list.GetBigWord (0) != kBootSignature)
    {
        list.AddRun (0, kBlockBytes, L"Boot blocks, no boot code");
        return;
    }

    for (const HfsFixedField & field : s_kBootFields)
    {
        if (field.length == kWordBytes)
        {
            list.AddBigWord (field.offset, field.label);
        }
        else if (field.length == kLongBytes)
        {
            list.AddBigLong (field.offset, field.label);
        }
        else
        {
            list.AddName (field.offset + 1, field.length - 1, field.label, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::AnnotateMdb
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::AnnotateMdb (const BlockMap & map, BlockFieldList & list)
{
    constexpr uint32_t  kWordBytes = 2;
    constexpr uint32_t  kLongBytes = 4;



    for (const HfsFixedField & field : s_kMdbFields)
    {
        if (field.length == kWordBytes)
        {
            list.AddBigWord (field.offset, field.label);
        }
        else if (field.length == kLongBytes)
        {
            list.AddBigLong (field.offset, field.label);
        }
        else if (field.offset == kVolumeName)
        {
            list.AddName (field.offset + 1, list.GetByte (field.offset), field.label, false);
        }
        else
        {
            list.AddRun (field.offset, field.length, field.label);
        }
    }

    list.AddBigLink (kVbmStart,  kWordBytes, L"Volume bitmap start", list.GetBigWord (kVbmStart));
    list.AddBigLink (kAllocStart, kWordBytes, L"First allocation block", list.GetBigWord (kAllocStart));

    AnnotateExtentRecord (map, kExtentsExtent, L"Extents overflow file", list);
    AnnotateExtentRecord (map, kCatalogExtent, L"Catalog file", list);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::AnnotateExtentRecord
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::AnnotateExtentRecord (const BlockMap & map, uint32_t at, const std::wstring & head, BlockFieldList & list)
{
    constexpr uint32_t  kWordBytes = 2;



    uint32_t  i      = 0;
    Word      start  = 0;
    Word      count  = 0;
    int64_t   link   = BlockField::kNoLink;



    for (i = 0; i < kExtentCount; i++)
    {
        start = list.GetBigWord (at + i * kExtentBytes);
        count = list.GetBigWord (at + i * kExtentBytes + kWordBytes);
        link  = (count != 0 && map.GetAllocationBlock (start) < map.GetBlockCount()) ? (int64_t) map.GetAllocationBlock (start) : BlockField::kNoLink;

        list.AddBigLink (at + i * kExtentBytes,              kWordBytes, head + L", extent " + std::to_wstring (i + 1) + L" start", link);
        list.AddBigWord (at + i * kExtentBytes + kWordBytes, head + L", extent " + std::to_wstring (i + 1) + L" length");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::AnnotateNode
//
//  The node descriptor, the record offsets at the node's end, and each
//  record by the node's type.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::AnnotateNode (const BlockMap & map, const BlockMapEntry & entry, BlockFieldList & list)
{
    constexpr uint32_t  kLongBytes  = 4;
    constexpr uint32_t  kMaxRecords = (kBlockBytes - kDescriptor) / 2;



    uint32_t  forward = list.GetBigLong (kForwardLink);
    uint32_t  back    = list.GetBigLong (kBackLink);
    Byte      type    = list.GetByte (kNodeType);
    uint32_t  records = std::min<uint32_t> (list.GetBigWord (kRecordCount), kMaxRecords);
    uint32_t  r       = 0;
    uint32_t  at      = 0;
    uint32_t  end     = 0;
    LPCWSTR   name    = L"unknown";
    uint32_t  linked  = 0;



    switch (type)
    {
        case kIndexNode:  name = L"index";  break;
        case kHeaderNode: name = L"header"; break;
        case kMapNode:    name = L"map";    break;
        case kLeafNode:   name = L"leaf";   break;
        default:                            break;
    }

    linked = map.GetNodeBlock (entry.tree, forward);
    list.AddBigLink (kForwardLink, kLongBytes, L"Forward link", forward != 0 && linked != BlockMap::kNoBlock ? (int64_t) linked : BlockField::kNoLink);

    linked = map.GetNodeBlock (entry.tree, back);
    list.AddBigLink (kBackLink,    kLongBytes, L"Back link",    back != 0 && linked != BlockMap::kNoBlock ? (int64_t) linked : BlockField::kNoLink);

    list.AddRun     (kNodeType,    1, L"Node type", BlockFieldList::FormatHex (type, 2) + L" (" + name + L")");
    list.AddByte    (kNodeHeight,  L"Node height");
    list.AddBigWord (kRecordCount, L"Records");

    for (r = 0; r <= records; r++)
    {
        list.AddBigWord (kBlockBytes - 2 * (r + 1), r < records ? L"Offset of record " + std::to_wstring (r) : std::wstring (L"Offset of free space"));
    }

    if (type == kHeaderNode)
    {
        AnnotateHeader (map, entry.tree, list);
        return;
    }

    if (type == kMapNode)
    {
        list.AddRun (kDescriptor, GetRecordOffset (list, 1) - kDescriptor, L"Map record");
        return;
    }

    for (r = 0; r < records; r++)
    {
        at  = GetRecordOffset (list, r);
        end = GetRecordOffset (list, r + 1);

        if (at < kDescriptor || at >= kBlockBytes || end <= at || end > kBlockBytes)
        {
            break;
        }

        AnnotateRecord (map, entry.tree, type, r, at, end, list);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::AnnotateHeader
//
//  The header record, the user data record and the map record.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::AnnotateHeader (const BlockMap & map, uint32_t tree, BlockFieldList & list)
{
    constexpr uint32_t  kLongBytes = 4;
    constexpr uint32_t  kReserved  = 76;



    uint32_t  root  = list.GetBigLong (kDescriptor + 2);
    uint32_t  first = list.GetBigLong (kDescriptor + 10);
    uint32_t  last  = list.GetBigLong (kDescriptor + 14);
    uint32_t  user  = GetRecordOffset (list, 1);
    uint32_t  bits  = GetRecordOffset (list, 2);
    uint32_t  end   = GetRecordOffset (list, 3);

    auto  toLink = [&] (uint32_t node) { uint32_t b = map.GetNodeBlock (tree, node); return node != 0 && b != BlockMap::kNoBlock ? (int64_t) b : BlockField::kNoLink; };



    list.AddBigWord (kDescriptor,      L"Tree depth");
    list.AddBigLink (kDescriptor + 2,  kLongBytes, L"Root node", toLink (root));
    list.AddBigLong (kDescriptor + 6,  L"Leaf records");
    list.AddBigLink (kDescriptor + 10, kLongBytes, L"First leaf node", toLink (first));
    list.AddBigLink (kDescriptor + 14, kLongBytes, L"Last leaf node", toLink (last));
    list.AddBigWord (kDescriptor + 18, L"Node size");
    list.AddBigWord (kDescriptor + 20, L"Maximum key length");
    list.AddBigLong (kDescriptor + 22, L"Total nodes");
    list.AddBigLong (kDescriptor + 26, L"Free nodes");
    list.AddRun     (kDescriptor + 30, kReserved, L"Reserved");

    if (user > kDescriptor && bits > user && bits < kBlockBytes)
    {
        list.AddRun (user, bits - user, L"User data record");
    }

    if (bits > kDescriptor && end > bits && end <= kBlockBytes)
    {
        list.AddRun (bits, end - bits, L"Map record");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HfsBlockLayout::AnnotateRecord
//
//  One record of an index or leaf node: its key, then for an index node the
//  child node it points to, and for a leaf the catalog or extents record.
//
////////////////////////////////////////////////////////////////////////////////

void HfsBlockLayout::AnnotateRecord (
    const BlockMap  & map,
    uint32_t          tree,
    Byte              type,
    uint32_t          number,
    uint32_t          at,
    uint32_t          end,
    BlockFieldList  & list)
{
    constexpr uint32_t  kLongBytes = 4;
    constexpr uint32_t  kNameMax   = 31;



    std::wstring  head   = L"Record " + std::to_wstring (number) + L": ";
    uint32_t      data   = at + 1 + list.GetByte (at);
    uint32_t      child  = 0;
    uint32_t      linked = 0;
    Byte          kind   = 0;



    data += data & 1;

    list.AddByte (at, head + L"key length");

    if (tree == HfsVolume::kCatalogFileId)
    {
        list.AddBigLong (at + 2, head + L"parent ID");
        list.AddName    (at + 7, std::min<uint32_t> (list.GetByte (at + 6), kNameMax), head + L"name", false);
    }
    else
    {
        list.AddByte    (at + 1, head + L"fork type");
        list.AddBigLong (at + 2, head + L"file ID");
        list.AddBigWord (at + 6, head + L"first allocation block of the run");
    }

    if (data >= end)
    {
        return;
    }

    if (type == kIndexNode)
    {
        child  = list.GetBigLong (data);
        linked = map.GetNodeBlock (tree, child);

        list.AddBigLink (data, kLongBytes, head + L"child node", linked != BlockMap::kNoBlock ? (int64_t) linked : BlockField::kNoLink);
        return;
    }

    if (tree == HfsVolume::kExtentsFileId)
    {
        AnnotateExtentRecord (map, data, head + L"extents", list);
        return;
    }

    kind = list.GetByte (data);

    list.AddByte (data, head + L"record type");

    if (kind == kDirectory)
    {
        list.AddBigWord (data + kValence, head + L"valence");
        list.AddBigLong (data + kDirId,   head + L"directory ID");
    }
    else if (kind == kFile)
    {
        list.AddBigLong (data + kFileId,   head + L"file ID");
        list.AddBigLong (data + kDataSize, head + L"data fork size");
        list.AddBigLong (data + kForkSize, head + L"resource fork size");

        AnnotateExtentRecord (map, data + kDataExtents, head + L"data fork", list);
        AnnotateExtentRecord (map, data + kForkExtents, head + L"resource fork", list);
    }
    else if (kind == kDirThread || kind == kFileThread)
    {
        list.AddBigLong (data + kThreadParent,     head + L"parent ID");
        list.AddName    (data + kThreadName + 1, std::min<uint32_t> (list.GetByte (data + kThreadName), kNameMax), head + L"name", false);
    }
}
