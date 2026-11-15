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
static constexpr int  s_kDate          = 24;
static constexpr int  s_kVolumeNameMax = 7;
static constexpr int  s_kFileNameMax   = 15;
static constexpr int  s_kMinBlocks     = 6;
static constexpr int  s_kBadKind       = 1;
static constexpr int  s_kBlockBytes    = 512;
static constexpr int  s_kNameFirst     = 0x21;
static constexpr int  s_kNameLast      = 0x7E;





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
        writer.AddFinding (FindingKind::FileChainBroken, badBlock, L"The directory can't be read: " + writer.DescribeUnreadable (badBlock));
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
        file.modified     = static_cast<uint32_t> (GetWord (entry + s_kDate));
        file.recordedSize = (next > first) ? static_cast<uint64_t> (next - first - 1) * s_kBlockBytes + GetWord (entry + s_kLastBytes) : 0;
        index             = writer.AddFile (std::move (file));

        CheckEntry (writer, entry, outMap.files[index].path, kind, first);

        if (first < previous)
        {
            writer.AddFinding (FindingKind::PascalEntryOutOfOrder, first, std::format (L"{} starts at block {}, inside the file before it, which ends at block {}", outMap.files[index].path, first, previous - 1));
        }

        if (next <= first || next > total)
        {
            writer.AddFinding (FindingKind::PascalEntryOutOfRange, first, std::format (L"{}: blocks {} to {} don't fit in the {}-block volume",
                                                                                       outMap.files[index].path, first, next - 1, total));
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
        writer.AddFinding (FindingKind::PascalFileCountDiffers, kFirstDirectoryBlock, std::format (L"Expected a directory file count of {1}, but found {0}",
                                                                                                   GetWord (&directory[s_kFileCount]), entries));
        writer.AddCount ({ kFirstDirectoryBlock, s_kFileCount, GetWord (&directory[s_kFileCount]), entries, L"the directory's file count" });
    }

    writer.Finalize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalMapReader::CheckEntry
//
//  A last block's byte count of 1 to 512, and a name the Filer accepts:
//  printable, with no space, $, =, ?, comma or colon. A .BAD file's count
//  isn't checked.
//
////////////////////////////////////////////////////////////////////////////////

void PascalMapReader::CheckEntry (FileMapWriter & writer, const Byte * entry, const std::wstring & path, int kind, int first)
{
    int  lastBytes = GetWord (entry + s_kLastBytes);



    if (kind != s_kBadKind && (lastBytes < 1 || lastBytes > s_kBlockBytes))
    {
        writer.AddFinding (FindingKind::PascalLastBytesInvalid, first, std::format (L"{}: the last block's byte count {} is outside 1 to 512", path, lastBytes));
    }

    if (!IsValidName (entry + s_kName, entry[s_kNameLength]))
    {
        writer.AddFinding (FindingKind::PascalNameInvalid, first, path + L": the file name isn't a valid Apple Pascal name");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalMapReader::IsValidName
//
////////////////////////////////////////////////////////////////////////////////

bool PascalMapReader::IsValidName (const Byte * name, int length)
{
    static constexpr std::wstring_view  kNotAllowed = L"$=?,:";



    bool  isValid = true;



    for (int i = 0; isValid && i < length; i++)
    {
        isValid = name[i] >= s_kNameFirst && name[i] <= s_kNameLast && kNotAllowed.find (static_cast<wchar_t> (name[i])) == std::wstring_view::npos;
    }

    return isValid;
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
