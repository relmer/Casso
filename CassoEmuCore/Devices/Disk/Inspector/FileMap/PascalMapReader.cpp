#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/PascalMapReader.h"





static constexpr int  s_kEntrySize     = 26;
static constexpr int  s_kFirst         = 0;
static constexpr int  s_kNext          = 2;
static constexpr int  s_kKind          = 4;
static constexpr int  s_kNameLength    = 6;
static constexpr int  s_kName          = 7;
static constexpr int  s_kTotalBlocks   = 14;
static constexpr int  s_kFileCount     = 16;
static constexpr int  s_kLastBytes     = 22;
static constexpr int  s_kVolumeNameMax = 7;
static constexpr int  s_kFileNameMax   = 15;
static constexpr int  s_kMinBlocks     = 6;
static constexpr int  s_kBadKind       = 1;
static constexpr int  s_kBlockBytes    = 512;





////////////////////////////////////////////////////////////////////////////////
//
//  PascalMapReader::IsFound
//
//  First block 0, next block 6, file type 0, a volume name of 1 to 7
//  printable characters, at least 6 blocks and at most 77 files (FR-085).
//
////////////////////////////////////////////////////////////////////////////////

bool PascalMapReader::IsFound (const SectorSource & source, int & outBlocks)
{
    std::array<Byte, 2 * SectorSource::kBytes>  header  = {};
    bool                                        isFound = source.ReadBlock (kFirstDirectoryBlock, header);
    int                                         length  = 0;



    if (isFound)
    {
        length    = header[s_kNameLength];
        outBlocks = GetWord (&header[s_kTotalBlocks]);
        isFound   = GetWord (&header[s_kFirst]) == 0 && GetWord (&header[s_kNext]) == s_kMinBlocks && GetWord (&header[s_kKind]) == 0 &&
                    length >= 1 && length <= s_kVolumeNameMax && outBlocks >= s_kMinBlocks && GetWord (&header[s_kFileCount]) <= kMaxFiles;

        for (int i = 0; isFound && i < length; i++)
        {
            isFound = header[s_kName + i] > 0x20 && header[s_kName + i] < 0x7F;
        }
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalMapReader::Read
//
//  Each entry in turn: its run of blocks, out-of-order and out-of-range
//  entries reported, and a run of blocks two entries share left to the
//  cross-link the writer finds.
//
////////////////////////////////////////////////////////////////////////////////

void PascalMapReader::Read (const SectorSource & source, FileMap & outMap)
{
    FileMapWriter  writer    (source, outMap, MapFileSystem::Pascal);
    vector<Byte>   directory;
    int            badBlock  = -1;
    int            total     = 0;
    int            count     = 0;
    int            previous  = s_kMinBlocks;
    int            e         = 0;
    int            b         = 0;
    int            entries   = 0;



    writer.SetStructure (0, SectorRole::BootArea);
    writer.SetStructure (1, SectorRole::BootArea);

    for (b = kFirstDirectoryBlock; b < kFirstDirectoryBlock + kDirectoryBlocks; b++)
    {
        writer.SetStructure (b, SectorRole::CatalogOrDirectory);
    }

    if (!ReadDirectory (source, directory, badBlock))
    {
        outMap.isCatalogComplete = false;
        outMap.unreadableCell    = badBlock;
        writer.AddFinding (FindingKind::FileChainBroken, badBlock, L"The directory cannot be read past " + writer.DescribeCell (badBlock) + L", " +
                                                                   FileMapWriter::DescribeReason (writer.GetResult (badBlock) == SectorResult::Missing
                                                                                                  ? ChainReason::MissingSector : ChainReason::BadSector));
    }

    total             = GetWord (&directory[s_kTotalBlocks]);
    count             = std::min (GetWord (&directory[s_kFileCount]), kMaxFiles);
    outMap.volumeName = ReadName (&directory[s_kName], std::min<int> (directory[s_kNameLength], s_kVolumeNameMax)) + L":";

    for (e = 1; e <= count && (e + 1) * s_kEntrySize <= static_cast<int> (directory.size()); e++)
    {
        const Byte *  entry  = &directory[e * s_kEntrySize];
        int           first  = GetWord (entry + s_kFirst);
        int           next   = GetWord (entry + s_kNext);
        int           kind   = GetWord (entry + s_kKind) & 0x0F;
        int           length = entry[s_kNameLength];
        int           index  = -1;
        MappedFile    file;

        if (length < 1 || length > s_kFileNameMax)
        {
            continue;
        }

        entries++;
        file.path         = ReadName (entry + s_kName, length);
        file.type         = GetKindName (kind);
        file.recordedSize = (next > first) ? static_cast<uint64_t> (next - first - 1) * s_kBlockBytes + GetWord (entry + s_kLastBytes) : 0;
        index             = writer.AddFile (std::move (file));

        if (first < previous)
        {
            writer.AddFinding (FindingKind::PascalEntryOutOfOrder, first, outMap.files[index].path + L" starts at " + writer.DescribeCell (first) +
                                                                          L", before the entry ahead of it ends");
        }

        if (next <= first || next > total)
        {
            writer.AddFinding (FindingKind::PascalEntryOutOfRange, first, std::format (L"{} runs from block {} to block {}, which the volume of {} blocks cannot hold",
                                                                                       outMap.files[index].path, first, next, total));
            continue;
        }

        for (b = first; b < next; b++)
        {
            writer.Own (index, b, kind == s_kBadKind ? SectorRole::BadBlocksFile : SectorRole::FileData);
        }

        previous = next;
    }

    if (entries != GetWord (&directory[s_kFileCount]))
    {
        writer.AddFinding (FindingKind::PascalFileCountDiffers, kFirstDirectoryBlock, std::format (L"The directory gives {} files but holds {} entries",
                                                                                                   GetWord (&directory[s_kFileCount]), entries));
    }

    writer.Finalize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalMapReader::ReadDirectory
//
//  Blocks 2 to 5 end to end; a block that cannot be read is left as zeros
//  and given back.
//
////////////////////////////////////////////////////////////////////////////////

bool PascalMapReader::ReadDirectory (const SectorSource & source, vector<Byte> & outBytes, int & outBadBlock)
{
    std::array<Byte, 2 * SectorSource::kBytes>  block  = {};
    bool                                        isRead = true;



    outBytes.assign (static_cast<size_t> (kDirectoryBlocks) * block.size(), 0);

    for (int b = 0; b < kDirectoryBlocks; b++)
    {
        if (source.ReadBlock (kFirstDirectoryBlock + b, block))
        {
            std::copy (block.begin(), block.end(), outBytes.begin() + b * block.size());
        }
        else if (isRead)
        {
            isRead      = false;
            outBadBlock = kFirstDirectoryBlock + b;
        }
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalMapReader::ReadName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PascalMapReader::ReadName (const Byte * name, int length)
{
    std::wstring  text;



    for (int i = 0; i < length; i++)
    {
        text.push_back ((name[i] >= 0x20 && name[i] < 0x7F) ? static_cast<wchar_t> (name[i]) : L'?');
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalMapReader::GetKindName
//
//  The file kind as Pascal's Filer gives it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PascalMapReader::GetKindName (int kind)
{
    static constexpr LPCWSTR  kNames[] = { L"", L".BAD", L".CODE", L".TEXT", L".INFO", L".DATA", L".GRAF", L".FOTO", L".SECUREDIR" };



    std::wstring  name = std::format (L"Kind {}", kind);



    if (kind >= 1 && kind < static_cast<int> (std::size (kNames)))
    {
        name = kNames[kind];
    }

    return name;
}
