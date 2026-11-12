#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/PascalBlockLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PascalBlockLayout::GetByte
//
////////////////////////////////////////////////////////////////////////////////

Byte PascalBlockLayout::GetByte (const BlockImage & image, size_t at)
{
    return at < image.size() ? image[at] : (Byte) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalBlockLayout::GetName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PascalBlockLayout::GetName (const BlockImage & image, size_t at, uint32_t maxLength)
{
    constexpr Byte  kFirstPrintable = 0x20;
    constexpr Byte  kLastPrintable  = 0x7E;



    std::wstring  name;
    uint32_t      length = std::min<uint32_t> (GetByte (image, at), maxLength);
    uint32_t      i      = 0;
    Byte          ch     = 0;



    for (i = 0; i < length; i++)
    {
        ch    = GetByte (image, at + 1 + i);
        name += (ch >= kFirstPrintable && ch <= kLastPrintable) ? (wchar_t) ch : L'?';
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalBlockLayout::IsFound
//
//  A volume entry of first block 0, a last block past the directory's
//  first, kind 0, a name of 1 to 7 characters and a file count no more than
//  kMaxFiles.
//
////////////////////////////////////////////////////////////////////////////////

bool PascalBlockLayout::IsFound (const BlockImage & image, uint32_t & outBlocks)
{
    size_t    at      = kDirectoryBlock * kBlockBytes;
    Byte      length  = GetByte (image, at + kVolumeName);
    Word      last    = GetWord (image, at + kLastBlock);
    bool      isFound = image.size() >= at + kBlockBytes
                     && GetWord (image, at + kFirstBlock) == 0
                     && last > kDirectoryBlock && last <= kDirectoryBlock + kMaxDirectoryBlocks
                     && GetWord (image, at + kKind) == 0
                     && length >= 1 && length <= kVolumeNameMax
                     && GetWord (image, at + kFileCount) <= kMaxFiles;



    outBlocks = isFound ? GetWord (image, at + kVolumeBlocks) : 0;

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalBlockLayout::Walk
//
////////////////////////////////////////////////////////////////////////////////

void PascalBlockLayout::Walk (const BlockImage & image, BlockMap & outMap)
{
    size_t    directory = kDirectoryBlock * kBlockBytes;
    uint32_t  count     = (uint32_t) (image.size() / kBlockBytes);
    uint32_t  last      = std::min<uint32_t> (GetWord (image, directory + kLastBlock), kDirectoryBlock + kMaxDirectoryBlocks);
    uint32_t  files     = std::min<uint32_t> (GetWord (image, directory + kFileCount), kMaxFiles);
    uint32_t  block     = 0;
    uint32_t  i         = 0;
    size_t    at        = 0;
    uint32_t  first     = 0;
    uint32_t  end       = 0;
    int       owner     = 0;



    outMap            = BlockMap();
    outMap.fileSystem = BlockFileSystem::Pascal;
    outMap.hasFreeMap = false;
    outMap.volumeName = GetName (image, directory + kVolumeName, kVolumeNameMax);
    outMap.blocks.assign (count, BlockMapEntry());

    for (block = 0; block < std::min<uint32_t> (kDirectoryBlock, count); block++)
    {
        outMap.blocks[block].role      = SectorRole::BootArea;
        outMap.blocks[block].structure = BlockStructure::BootLoader;
        outMap.blocks[block].node      = block;
    }

    for (block = kDirectoryBlock; block < last && block < count; block++)
    {
        outMap.blocks[block].role      = block == kDirectoryBlock ? SectorRole::VtocOrKeyBlock : SectorRole::CatalogOrDirectory;
        outMap.blocks[block].structure = BlockStructure::PascalDirectory;
    }

    for (i = 1; i <= files; i++)
    {
        at    = directory + (size_t) i * kEntryBytes;
        first = GetWord (image, at + kFirstBlock);
        end   = std::min<uint32_t> (GetWord (image, at + kLastBlock), count);
        owner = (int) outMap.files.size();

        outMap.files.push_back (GetName (image, at + kFileName, kFileNameMax));

        for (block = std::max<uint32_t> (first, last); block < end; block++)
        {
            BlockMapEntry  & entry = outMap.blocks[block];

            if (entry.structure == BlockStructure::None)
            {
                entry.role      = SectorRole::FileData;
                entry.structure = BlockStructure::FileData;
                entry.node      = block - first;
            }

            entry.owners.push_back (owner);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalBlockLayout::Annotate
//
//  The directory's fields that start in this block. The directory is read
//  as one run from block 2, so an entry that starts in the block before has
//  only its later fields here.
//
////////////////////////////////////////////////////////////////////////////////

void PascalBlockLayout::Annotate (const BlockMap & map, uint32_t block, BlockBytes bytes, vector<BlockField> & outFields)
{
    struct EntryField
    {
        uint32_t  offset;
        uint32_t  length;
        LPCWSTR   label;
        bool      isLink;
    };

    static constexpr EntryField  kVolumeFields[] =
    {
        { kFirstBlock,   2, L"first block",   false },
        { kLastBlock,    2, L"directory end", false },
        { kKind,         2, L"kind",          false },
        { kVolumeName,   8, L"name",          false },
        { kVolumeBlocks, 2, L"blocks",        false },
        { kFileCount,    2, L"files",         false },
        { 18,            2, L"last access",   false },
        { 20,            2, L"last boot",     false },
    };

    static constexpr EntryField  kFileFields[] =
    {
        { kFirstBlock,   2, L"first block",          true  },
        { kLastBlock,    2, L"block after the file", false },
        { kKind,         2, L"kind",                 false },
        { kFileName,    16, L"name",                 false },
        { kLastByte,     2, L"bytes in last block",  false },
        { kModified,     2, L"modified",             false },
    };

    BlockFieldList         list (bytes, outFields);
    const BlockMapEntry  & entry  = map.blocks[block];
    uint32_t               start  = (block - kDirectoryBlock) * kBlockBytes;
    uint32_t               i      = 0;
    uint32_t               base   = 0;
    uint32_t               files  = 0;
    uint32_t               at     = 0;
    std::wstring           head;



    if (entry.structure == BlockStructure::BootLoader)
    {
        list.AddRun (0, kBlockBytes, entry.node == 0 ? L"Boot loader" : L"Boot loader, second block");
        return;
    }

    if (entry.structure != BlockStructure::PascalDirectory)
    {
        return;
    }

    files = std::min<uint32_t> ((uint32_t) map.files.size(), kMaxFiles);

    for (i = 0; i <= files; i++)
    {
        base = i * kEntryBytes;
        head = i == 0 ? std::wstring (L"Volume: ") : L"Entry " + std::to_wstring (i) + L" " + map.files[i - 1] + L": ";

        for (const EntryField & field : (i == 0 ? std::span<const EntryField> (kVolumeFields) : std::span<const EntryField> (kFileFields)))
        {
            if (base + field.offset < start || base + field.offset >= start + kBlockBytes)
            {
                continue;
            }

            at = base + field.offset - start;

            if (field.isLink)
            {
                list.AddLink (at, head + field.label, map.GetBlockCount());
            }
            else if (field.length == 2)
            {
                list.AddWord (at, head + field.label);
            }
            else
            {
                list.AddName (at + 1, std::min<uint32_t> (list.GetByte (at), field.length - 1), head + field.label, false);
            }
        }
    }
}
