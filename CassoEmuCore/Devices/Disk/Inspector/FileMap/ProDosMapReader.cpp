#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/ProDosMapReader.h"





static constexpr int   s_kNext           = 0x02;
static constexpr int   s_kFirstEntry     = 0x04;
static constexpr int   s_kEntryLength    = 0x27;
static constexpr int   s_kEntriesPerBlk  = 0x0D;
static constexpr int   s_kHeaderLength   = 0x23;
static constexpr int   s_kHeaderPerBlk   = 0x24;
static constexpr int   s_kBitmapPointer  = 0x27;
static constexpr int   s_kTotalBlocks    = 0x29;
static constexpr int   s_kFileType       = 0x10;
static constexpr int   s_kKeyPointer     = 0x11;
static constexpr int   s_kBlocksUsed     = 0x13;
static constexpr int   s_kEof            = 0x15;
static constexpr int   s_kAccess         = 0x1E;
static constexpr Byte  s_kWriteEnabled   = 0x02;
static constexpr int   s_kSeedling       = 0x1;
static constexpr int   s_kSapling        = 0x2;
static constexpr int   s_kTree           = 0x3;
static constexpr int   s_kExtended       = 0x5;
static constexpr int   s_kSubdirectory   = 0xD;
static constexpr int   s_kHeader         = 0xE;
static constexpr int   s_kVolumeHeader   = 0xF;
static constexpr int   s_kResourceFork   = 0x100;
static constexpr int   s_kPointers       = 256;
static constexpr int   s_kBlockBytes     = 512;
static constexpr int   s_kBitsPerBlock   = 4096;
static constexpr int   s_kMaxDepth       = 32;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::IsFound
//
//  Storage type $F, previous block 0, entries $27 bytes long, 13 a block
//  (FR-084).
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosMapReader::IsFound (const SectorSource & source, int & outBlocks)
{
    Block  key     = {};
    bool   isFound = source.ReadBlock (kKeyBlock, key);



    if (isFound)
    {
        outBlocks = GetWord (&key[s_kTotalBlocks]);
        isFound   = GetWord (&key[0]) == 0 && (key[s_kFirstEntry] >> 4) == s_kVolumeHeader && key[s_kHeaderLength] == s_kEntryLength &&
                    key[s_kHeaderPerBlk] == s_kEntriesPerBlk;
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::Read
//
////////////////////////////////////////////////////////////////////////////////

void ProDosMapReader::Read (const SectorSource & source, FileMap & outMap)
{
    FileMapWriter  writer     (source, outMap, MapFileSystem::ProDos);
    Walk           walk       { writer, source, {}, 0 };
    Block          key        = {};
    Block          bitmap     = {};
    int            bitmapAt   = 0;
    int            total      = SectorSource::kBlocks;
    int            b          = 0;
    vector<bool>   markedUsed (static_cast<size_t> (total), false);
    bool           hasBitmap  = false;



    (void) source.ReadBlock (kKeyBlock, key);
    outMap.volumeName = L"/" + ReadName (&key[s_kFirstEntry]);
    bitmapAt          = GetWord (&key[s_kBitmapPointer]);

    writer.SetStructure (0, SectorRole::BootArea);
    writer.SetStructure (1, SectorRole::BootArea);

    //  The bitmap: one bit a block, first byte's high bit block 0, set free.
    hasBitmap = bitmapAt > kKeyBlock && bitmapAt < total && source.ReadBlock (bitmapAt, bitmap);

    for (b = 0; hasBitmap && b < total; b++)
    {
        markedUsed[b] = ((bitmap[b / 8] >> (7 - b % 8)) & 1) == 0;
    }

    for (b = bitmapAt; b < total && b < bitmapAt + (total + s_kBitsPerBlock - 1) / s_kBitsPerBlock; b++)
    {
        writer.SetStructure (b, SectorRole::VolumeBitmap);
    }

    ReadDirectory (walk, kKeyBlock, L"", SectorRole::CatalogOrDirectory, -1);
    writer.SetStructure (kKeyBlock, SectorRole::VtocOrKeyBlock);

    if (hasBitmap)
    {
        writer.SetMarkedUsed (std::move (markedUsed));
    }

    writer.Finalize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::ReadDirectory
//
//  A directory's chain of blocks and every entry in them. The volume
//  directory's blocks are the volume's own structure; a subdirectory's
//  belong to its entry. A directory block already read ends the chain, so
//  a loop through subdirectories cannot run on.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosMapReader::ReadDirectory (Walk & walk, int keyBlock, const std::wstring & prefix, SectorRole role, int owner)
{
    std::set<int>  visited;
    ChainReason    reason = ChainReason::None;
    Block          block  = {};
    int            at     = keyBlock;
    int            e      = 0;
    bool           isKey  = true;



    walk.depth++;

    while (at != 0 && walk.depth <= s_kMaxDepth)
    {
        if (walk.directoryBlocks.contains (at) || !walk.writer.Step (at, visited, reason) || !walk.source.ReadBlock (at, block))
        {
            reason = (reason == ChainReason::None) ? (walk.directoryBlocks.contains (at) ? ChainReason::Loop : walk.writer.GetResult (at) == SectorResult::Missing
                                                                                                                ? ChainReason::MissingSector : ChainReason::BadSector)
                                                   : reason;

            if (owner >= 0)
            {
                walk.writer.BreakChain (owner, reason, at);
            }
            else
            {
                walk.writer.AddFinding (FindingKind::FileChainBroken, at, L"The volume directory stops at " + walk.writer.DescribeCell (at) + L", " +
                                                                          FileMapWriter::DescribeReason (reason));
            }

            break;
        }

        walk.directoryBlocks.insert (at);

        if (owner >= 0)
        {
            walk.writer.Own (owner, at, role);
        }
        else
        {
            walk.writer.SetStructure (at, role);
        }

        for (e = isKey ? 1 : 0; e < s_kEntriesPerBlk; e++)
        {
            ReadEntry (walk, &block[s_kFirstEntry + e * s_kEntryLength], prefix);
        }

        isKey = false;
        at    = GetWord (&block[s_kNext]);
    }

    walk.depth--;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::ReadEntry
//
//  Storage type 0 with a name left in the entry is a deleted file; its
//  blocks are followed as the entry's counts suggest it was stored.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosMapReader::ReadEntry (Walk & walk, const Byte * entry, const std::wstring & prefix)
{
    int         storage = entry[0] >> 4;
    int         length  = entry[0] & 0x0F;
    int         key     = GetWord (entry + s_kKeyPointer);
    int         used    = GetWord (entry + s_kBlocksUsed);
    uint32_t    eof     = entry[s_kEof] | (entry[s_kEof + 1] << 8) | (entry[s_kEof + 2] << 16);
    int         index   = -1;
    MappedFile  file;



    if (length > 0 && storage != s_kHeader && storage != s_kVolumeHeader)
    {
        file.path          = prefix + ReadName (entry);
        file.type          = GetTypeName (entry[s_kFileType]);
        file.isLocked      = (entry[s_kAccess] & s_kWriteEnabled) == 0;
        file.recordedSize  = eof;
        file.recordedCount = used;
        file.isDeleted     = storage == 0;
        index              = walk.writer.AddFile (std::move (file));

        if (storage == 0)
        {
            storage = (used <= 1) ? s_kSeedling : (used <= s_kPointers + 1) ? s_kSapling : s_kTree;
        }

        if (storage == s_kSubdirectory)
        {
            ReadDirectory (walk, key, prefix + ReadName (entry) + L"/", SectorRole::Subdirectory, index);
        }
        else if (storage == s_kExtended)
        {
            Block           extended = {};
            std::set<int>   visited;

            if (ReadChainBlock (walk, index, key, SectorRole::IndexBlock, visited, extended))
            {
                for (int fork : { 0, s_kResourceFork })
                {
                    ReadFork (walk, index, extended[fork] & 0x0F, GetWord (&extended[fork + 1]),
                              extended[fork + 5] | (extended[fork + 6] << 8) | (extended[fork + 7] << 16));
                }
            }
        }
        else
        {
            ReadFork (walk, index, storage, key, eof);
        }

        walk.writer.CheckCount (index, L"its entry gives", L"blocks");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::ReadFork
//
//  A seedling's one block, a sapling's index and its blocks, or a tree's
//  master index and each index under it, up to the blocks its end of file
//  needs; a pointer of 0 inside that is a hole.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosMapReader::ReadFork (Walk & walk, int file, int storage, int key, uint32_t eof)
{
    int            blocks  = static_cast<int> ((eof + s_kBlockBytes - 1) / s_kBlockBytes);
    std::set<int>  visited;
    Block          master  = {};
    int            i       = 0;
    int            index   = 0;



    if (storage == s_kSeedling && key != 0)
    {
        walk.writer.Own (file, key < SectorSource::kBlocks ? key : -1, SectorRole::FileData);
    }
    else if (storage == s_kSapling)
    {
        ReadIndex (walk, file, key, std::min (std::max (blocks, 1), s_kPointers), visited);
    }
    else if (storage == s_kTree && ReadChainBlock (walk, file, key, SectorRole::IndexBlock, visited, master))
    {
        for (i = 0; i * s_kPointers < std::max (blocks, 1) && i < s_kPointers / 2; i++)
        {
            index = master[i] | (master[s_kPointers + i] << 8);

            if (index == 0)
            {
                for (int k = 0; k < std::min (s_kPointers, blocks - i * s_kPointers); k++)
                {
                    walk.writer.AddHole (file);
                }
            }
            else
            {
                ReadIndex (walk, file, index, std::min (s_kPointers, blocks - i * s_kPointers), visited);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::ReadIndex
//
//  An index block's pointers, low bytes then high bytes.
//
////////////////////////////////////////////////////////////////////////////////

void ProDosMapReader::ReadIndex (Walk & walk, int file, int index, int blocks, std::set<int> & visited)
{
    Block  bytes = {};
    int    i     = 0;
    int    data  = 0;



    if (ReadChainBlock (walk, file, index, SectorRole::IndexBlock, visited, bytes))
    {
        for (i = 0; i < blocks; i++)
        {
            data = bytes[i] | (bytes[s_kPointers + i] << 8);

            if (data == 0)
            {
                walk.writer.AddHole (file);
            }
            else if (data >= SectorSource::kBlocks)
            {
                walk.writer.BreakChain (file, ChainReason::OutsideVolume, -1);
                break;
            }
            else
            {
                walk.writer.Own (file, data, SectorRole::FileData);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::ReadChainBlock
//
//  A block of a file's own chain (an index, master index or extended key):
//  owned, then read; a block the chain cannot pass breaks it there.
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosMapReader::ReadChainBlock (Walk & walk, int file, int block, SectorRole role, std::set<int> & visited, Block & outBlock)
{
    ChainReason  reason = ChainReason::None;
    bool         isRead = walk.writer.Step (block, visited, reason);



    if (isRead)
    {
        walk.writer.Own (file, block, role);
        isRead = walk.source.ReadBlock (block, outBlock);
        reason = isRead ? ChainReason::None : (walk.writer.GetResult (block) == SectorResult::Missing ? ChainReason::MissingSector : ChainReason::BadSector);
    }

    if (!isRead)
    {
        walk.writer.BreakChain (file, reason, block);
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::ReadName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProDosMapReader::ReadName (const Byte * entry)
{
    std::wstring  name;
    int           length = entry[0] & 0x0F;



    for (int i = 0; i < length; i++)
    {
        Byte  ch = entry[1 + i];

        name.push_back ((ch >= 0x20 && ch < 0x7F) ? static_cast<wchar_t> (ch) : L'?');
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosMapReader::GetTypeName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProDosMapReader::GetTypeName (Byte type)
{
    std::wstring  name = std::format (L"${:02X}", type);



    switch (type)
    {
        case 0x04: name = L"TXT"; break;
        case 0x06: name = L"BIN"; break;
        case 0x0F: name = L"DIR"; break;
        case 0x19: name = L"ADB"; break;
        case 0x1A: name = L"AWP"; break;
        case 0x1B: name = L"ASP"; break;
        case 0xEF: name = L"PAS"; break;
        case 0xF0: name = L"CMD"; break;
        case 0xFA: name = L"INT"; break;
        case 0xFB: name = L"IVR"; break;
        case 0xFC: name = L"BAS"; break;
        case 0xFD: name = L"VAR"; break;
        case 0xFE: name = L"REL"; break;
        case 0xFF: name = L"SYS"; break;
        default:                  break;
    }

    return name;
}
