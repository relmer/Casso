#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/ProDosBlockLayout.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::ReadBlock (const BlockImage & image, uint32_t block, Block & outBlock)
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
//  ProDosBlockLayout::IsFound
//
//  Previous block 0, a volume header's storage type, a name of 1 to 15
//  characters, entries $27 bytes long and 13 a block.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosBlockLayout::IsFound (const BlockImage & image, uint32_t & outBlocks)
{
    constexpr Byte  kNameLengthMask = 0x0F;
    constexpr int   kStorageShift   = 4;



    Block  key     = {};
    bool   isFound = image.size() >= (kKeyBlock + 1) * kBlockBytes;



    if (isFound)
    {
        ReadBlock (image, kKeyBlock, key);

        isFound = GetWord (&key[kPrevPointer]) == 0
               && (key[kFirstEntry] >> kStorageShift) == kVolumeHeader
               && (key[kFirstEntry] & kNameLengthMask) != 0
               && key[kFirstEntry + kOffEntryLength] == kEntryLength
               && ProDosSkeleton::IsEntriesPerBlockValid (key[kFirstEntry + kOffEntriesPerBlock]);
    }

    outBlocks = isFound ? GetWord (&key[kTotalBlocks]) : 0;

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::MarkBlock
//
//  Marks a block as part of a structure and adds its file. A block in a
//  second file is cross-linked, and its first structure stays.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::MarkBlock (
    Walker          & walker,
    uint32_t          block,
    SectorRole        role,
    BlockStructure    structure,
    int               owner,
    uint32_t          node)
{
    BlockMapEntry  * entry = nullptr;



    if (block >= walker.map.GetBlockCount())
    {
        return;
    }

    entry = &walker.map.blocks[block];

    if (entry->structure == BlockStructure::None)
    {
        entry->role      = role;
        entry->structure = structure;
        entry->node      = node;
    }

    if (owner >= 0 && std::find (entry->owners.begin(), entry->owners.end(), owner) == entry->owners.end())
    {
        entry->owners.push_back (owner);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::Walk
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::Walk (const BlockImage & image, BlockFileSystem fileSystem, BlockMap & outMap)
{
    Walker    walker  { image, outMap, {} };
    Block     key     = {};
    uint32_t  blocks  = (uint32_t) (image.size() / kBlockBytes);
    uint32_t  bitmap  = 0;
    uint32_t  count   = 0;
    uint32_t  i       = 0;



    outMap            = BlockMap();
    outMap.fileSystem = fileSystem;
    outMap.hasFreeMap = true;
    outMap.blocks.assign (blocks, BlockMapEntry());

    MarkBlock (walker, 0, SectorRole::BootArea, BlockStructure::BootLoader, -1);
    MarkBlock (walker, 1, SectorRole::BootArea, BlockStructure::BootLoader, -1, 1);

    ReadBlock (image, kKeyBlock, key);

    outMap.volumeName = ReadName (&key[kFirstEntry]);

    bitmap = GetWord (&key[kBitmapPointer]);
    count  = (GetWord (&key[kTotalBlocks]) + kBitsPerBlock - 1) / kBitsPerBlock;

    for (i = 0; i < count && IsInside (outMap, bitmap + i); i++)
    {
        MarkBlock (walker, bitmap + i, SectorRole::VolumeBitmap, BlockStructure::ProDosBitmap, -1, i);
    }

    WalkDirectory (walker, kKeyBlock, L"", -1, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::WalkDirectory
//
//  A directory's chain of blocks from its key block, each entry walked as it
//  is found. `owner` is the subdirectory's file, or -1 for the volume
//  directory.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::WalkDirectory (
    Walker              & walker,
    uint32_t              keyBlock,
    const std::wstring  & prefix,
    int                   owner,
    int                   depth)
{
    constexpr int  kStorageShift = 4;



    uint32_t        block = keyBlock;
    Block           bytes = {};
    uint32_t        entry = 0;
    const Byte    * at    = nullptr;
    bool            isKey = true;
    SectorRole      role  = SectorRole::CatalogOrDirectory;
    uint32_t        count = kEntriesPerBlock;



    if (depth > kMaxDepth)
    {
        walker.map.isCatalogComplete = false;
        return;
    }

    while (IsInside (walker.map, block))
    {
        if (walker.directoryBlocks.contains (block))
        {
            walker.map.isCatalogComplete = false;
            break;
        }

        walker.directoryBlocks.insert (block);

        role = (owner < 0) ? (isKey ? SectorRole::VtocOrKeyBlock : SectorRole::CatalogOrDirectory) : SectorRole::Subdirectory;

        MarkBlock (walker, block, role, BlockStructure::ProDosDirectory, owner);
        ReadBlock (walker.image, block, bytes);

        if (isKey)
        {
            count = (uint32_t) ProDosSkeleton::GetEntriesPerBlock (bytes[kFirstEntry + kOffEntriesPerBlock]);
        }

        for (entry = isKey ? 1 : 0; entry < count; entry++)
        {
            at = &bytes[kFirstEntry + entry * kEntryLength];

            if ((at[0] >> kStorageShift) != 0)
            {
                WalkEntry (walker, at, prefix, depth);
            }
        }

        isKey = false;
        block = GetWord (&bytes[kNextPointer]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::WalkEntry
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::WalkEntry (
    Walker              & walker,
    const Byte          * entry,
    const std::wstring  & prefix,
    int                   depth)
{
    constexpr int  kStorageShift = 4;



    Byte          storage = (Byte) (entry[0] >> kStorageShift);
    uint32_t      key     = GetWord (&entry[kOffKeyPointer]);
    std::wstring  path    = prefix + ReadName (entry);
    int           file    = (int) walker.map.files.size();
    Block         bytes   = {};



    walker.map.files.push_back (path);

    if (storage == kSubdirectory)
    {
        if (IsInside (walker.map, key))
        {
            WalkDirectory (walker, key, path + L"/", file, depth + 1);
        }

        return;
    }

    if (storage == kExtended)
    {
        if (!IsInside (walker.map, key))
        {
            return;
        }

        MarkBlock (walker, key, SectorRole::IndexBlock, BlockStructure::ProDosExtendedKey, file);
        ReadBlock (walker.image, key, bytes);

        WalkFork (walker, file, bytes[0], GetWord (&bytes[1]));
        WalkFork (walker, file, bytes[kResourceFork], GetWord (&bytes[kResourceFork + 1]));
        return;
    }

    WalkFork (walker, file, storage, key);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::WalkFork
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::WalkFork (Walker & walker, int file, Byte storage, uint32_t key)
{
    uint32_t  node   = 0;
    Block     master = {};
    uint32_t  i      = 0;
    uint32_t  index  = 0;



    if (!IsInside (walker.map, key))
    {
        return;
    }

    if (storage == kSeedling)
    {
        MarkBlock (walker, key, SectorRole::FileData, BlockStructure::FileData, file, 0);
    }
    else if (storage == kSapling)
    {
        WalkIndex (walker, file, key, node);
    }
    else if (storage == kTree)
    {
        MarkBlock (walker, key, SectorRole::IndexBlock, BlockStructure::ProDosMasterIndex, file);
        ReadBlock (walker.image, key, master);

        for (i = 0; i < kPointers; i++)
        {
            index = (uint32_t) (master[i] | (master[kPointers + i] << 8));
            node  = i * kPointers;

            if (IsInside (walker.map, index))
            {
                WalkIndex (walker, file, index, node);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::WalkIndex
//
//  An index block and the data blocks it points to. `ioNode` is the number
//  in the file of the index block's first data block.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::WalkIndex (Walker & walker, int file, uint32_t index, uint32_t & ioNode)
{
    Block     bytes = {};
    uint32_t  i     = 0;
    uint32_t  data  = 0;



    MarkBlock (walker, index, SectorRole::IndexBlock, BlockStructure::ProDosIndex, file);
    ReadBlock (walker.image, index, bytes);

    for (i = 0; i < kPointers; i++)
    {
        data = (uint32_t) (bytes[i] | (bytes[kPointers + i] << 8));

        if (IsInside (walker.map, data))
        {
            MarkBlock (walker, data, SectorRole::FileData, BlockStructure::FileData, file, ioNode + i);
        }
    }

    ioNode += kPointers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::ReadName
//
//  An entry's name, its length in the low nibble of its first byte.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProDosBlockLayout::ReadName (const Byte * entry)
{
    constexpr Byte  kNameLengthMask = 0x0F;
    constexpr Byte  kFirstPrintable = 0x20;
    constexpr Byte  kLastPrintable  = 0x7E;



    std::wstring  name;
    int           length = entry[0] & kNameLengthMask;
    int           i      = 0;
    Byte          ch     = 0;



    for (i = 0; i < length; i++)
    {
        ch    = entry[1 + i];
        name += (ch >= kFirstPrintable && ch <= kLastPrintable) ? (wchar_t) ch : L'?';
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::Annotate
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::Annotate (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields)
{
    BlockFieldList         list (bytes, outFields);
    const BlockMapEntry  & entry = map.blocks[block];



    switch (entry.structure)
    {
        case BlockStructure::BootLoader:
            list.AddRun (0, kBlockBytes, entry.node == 0 ? L"Boot loader" : L"Boot loader, second block");
            break;

        case BlockStructure::ProDosDirectory:
            AnnotateDirectory (map, list);
            break;

        case BlockStructure::ProDosBitmap:
            AnnotateBitmap (entry.node, list);
            break;

        case BlockStructure::ProDosIndex:
            AnnotateIndex (map, false, list);
            break;

        case BlockStructure::ProDosMasterIndex:
            AnnotateIndex (map, true, list);
            break;

        case BlockStructure::ProDosExtendedKey:
            AnnotateExtendedKey (map, list);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::AnnotateDirectory
//
//  The chain pointers, then the header in a key block, then each entry in
//  use. A key block is one whose previous pointer is zero.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::AnnotateDirectory (const BlockMap & map, BlockFieldList & list)
{
    constexpr int  kStorageShift = 4;



    uint32_t  count = map.GetBlockCount();
    bool      isKey = list.GetWord (kPrevPointer) == 0;
    uint32_t  i     = 0;
    uint32_t  at    = 0;



    list.AddLink (kPrevPointer, L"Previous directory block", count);
    list.AddLink (kNextPointer, L"Next directory block", count);

    for (i = 0; i < kEntriesPerBlock; i++)
    {
        at = kFirstEntry + i * kEntryLength;

        if (isKey && i == 0)
        {
            AnnotateHeader (map, at, list);
        }
        else if ((list.GetByte (at) >> kStorageShift) != 0)
        {
            AnnotateEntry (map, at, (int) i + 1, list);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::AnnotateHeader
//
//  A volume directory header or a subdirectory header. Both have a name,
//  dates, access and the entry sizes; the volume's then has the bitmap and
//  the volume's size, a subdirectory's its parent's block and entry.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::AnnotateHeader (const BlockMap & map, uint32_t at, BlockFieldList & list)
{
    constexpr int       kStorageShift = 4;
    constexpr uint32_t  kReserved     = 8;



    bool          isVolume = (list.GetByte (at) >> kStorageShift) == kVolumeHeader;
    std::wstring  head     = isVolume ? L"Volume header: " : L"Subdirectory header: ";
    uint32_t      count    = map.GetBlockCount();



    list.AddByte (at + kOffTypeName, head + L"storage type and name length");
    list.AddName (at + kOffName,     list.GetByte (at) & kNameMask, head + L"name", false);

    if (isVolume)
    {
        list.AddRun (at + kOffSubdirMarker, kReserved, head + L"reserved");
    }
    else
    {
        list.AddByte (at + kOffSubdirMarker,     head + L"$75");
        list.AddRun  (at + kOffSubdirMarker + 1, kReserved - 1, head + L"reserved");
    }

    list.AddRun  (at + kOffCreated,         kDateBytes, head + L"created");
    list.AddByte (at + kOffVersion,                      head + L"version");
    list.AddByte (at + kOffMinVersion,                   head + L"minimum version");
    list.AddByte (at + kOffAccess,          head + L"access");
    list.AddByte (at + kOffEntryLength,     head + L"entry length");
    list.AddByte (at + kOffEntriesPerBlock, head + L"entries per block");
    list.AddWord (at + kOffFileCount,       head + L"file count");

    if (isVolume)
    {
        list.AddLink (at + kOffBitmapPointer, head + L"bitmap pointer", count);
        list.AddWord (at + kOffTotalBlocks,   head + L"total blocks");
    }
    else
    {
        list.AddLink (at + kOffParentPointer,     head + L"parent pointer", count);
        list.AddByte (at + kOffParentEntryNumber, head + L"parent entry number");
        list.AddByte (at + kOffParentEntryLength, head + L"parent entry length");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::AnnotateEntry
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::AnnotateEntry (const BlockMap & map, uint32_t at, int number, BlockFieldList & list)
{
    constexpr uint32_t  kEofBytes  = 3;



    uint32_t      count  = map.GetBlockCount();
    uint32_t      length = list.GetByte (at) & kNameMask;
    std::wstring  head   = L"Entry " + std::to_wstring (number) + L" " + list.GetText (at + 1, length) + L": ";



    list.AddByte (at + kOffTypeName,      head + L"storage type and name length");
    list.AddName (at + kOffName,          length, head + L"name", false);
    list.AddByte (at + kOffFileType,      head + L"file type");
    list.AddLink (at + kOffKeyPointer,    head + L"key pointer", count);
    list.AddWord (at + kOffBlocksUsed,    head + L"blocks used");
    list.AddLong (at + kOffEof,           kEofBytes, head + L"EOF");
    list.AddRun  (at + kOffCreated,       kDateBytes, head + L"created");
    list.AddByte (at + kOffVersion,                    head + L"version");
    list.AddByte (at + kOffMinVersion,                 head + L"minimum version");
    list.AddByte (at + kOffAccess,        head + L"access");
    list.AddWord (at + kOffAuxType,       head + L"aux type");
    list.AddRun  (at + kOffModified,      kDateBytes, head + L"modified");
    list.AddLink (at + kOffHeaderPointer, head + L"header pointer", count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::AnnotateIndex
//
//  Each pointer in use, its low byte in the first half and its high byte in
//  the second.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::AnnotateIndex (const BlockMap & map, bool isMaster, BlockFieldList & list)
{
    uint32_t  i     = 0;
    Word      value = 0;



    for (i = 0; i < kPointers; i++)
    {
        value = (Word) (list.GetByte (i) | (list.GetByte (kPointers + i) << 8));

        if (value == 0)
        {
            continue;
        }

        list.AddSplitLink (i, kPointers + i, (isMaster ? L"Index block " : L"Data block ") + std::to_wstring (i), map.GetBlockCount());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::AnnotateExtendedKey
//
//  The data fork's and the resource fork's mini-entries, and the Finder
//  information between them.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::AnnotateExtendedKey (const BlockMap & map, BlockFieldList & list)
{
    constexpr uint32_t  kEofBytes = 3;



    uint32_t  count = map.GetBlockCount();



    for (const auto & [at, head] : { std::pair<uint32_t, LPCWSTR> (0, L"Data fork: "), std::pair<uint32_t, LPCWSTR> (kResourceFork, L"Resource fork: ") })
    {
        list.AddByte (at,     std::wstring (head) + L"storage type");
        list.AddLink (at + 1, std::wstring (head) + L"key block", count);
        list.AddWord (at + 3, std::wstring (head) + L"blocks used");
        list.AddLong (at + 5, kEofBytes, std::wstring (head) + L"EOF");
    }

    list.AddRun (kFinderInfo,                kFinderEntry, L"Finder information, first entry");
    list.AddRun (kFinderInfo + kFinderEntry, kFinderEntry, L"Finder information, second entry");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosBlockLayout::AnnotateBitmap
//
//  Block `index` of the bitmap has a bit for each of 4,096 blocks, set for
//  a free one.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosBlockLayout::AnnotateBitmap (uint32_t index, BlockFieldList & list)
{
    uint32_t  first = index * kBitsPerBlock;
    uint32_t  last  = first + kBitsPerBlock - 1;
    uint32_t  free  = 0;
    uint32_t  i     = 0;



    for (i = 0; i < kBlockBytes; i++)
    {
        free += (uint32_t) std::popcount ((unsigned) list.GetByte (i));
    }

    list.AddRun (0, kBlockBytes,
                 L"Volume bitmap, blocks " + std::to_wstring (first) + L" to " + std::to_wstring (last),
                 std::to_wstring (free) + L" free");
}
