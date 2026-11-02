#include "Pch.h"

#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





static constexpr size_t  s_kEntryBytes      = 26;
static constexpr size_t  s_kOffFirst        = 0;
static constexpr size_t  s_kOffNext         = 2;
static constexpr size_t  s_kOffKind         = 4;
static constexpr size_t  s_kOffNameLength   = 6;
static constexpr size_t  s_kOffName         = 7;
static constexpr size_t  s_kOffVolumeBlocks = 14;
static constexpr size_t  s_kOffFileCount    = 16;
static constexpr size_t  s_kOffLastBytes    = 22;
static constexpr size_t  s_kOffDate         = 24;
static constexpr size_t  s_kOffVolumeDate   = 20;
static constexpr size_t  s_kVolumeNameMax   = 7;
static constexpr size_t  s_kFileNameMax     = 15;
static constexpr size_t  s_kDirectoryBytes  = 4 * PascalVolume::kBlockBytes;
static constexpr Byte    s_kKindMask        = 0x0F;
static constexpr Byte    s_kDataLinkEscape  = 0x10;
static constexpr Byte    s_kIndentBase      = 0x20;
static constexpr size_t  s_kTextPageBytes   = 2 * PascalVolume::kBlockBytes;
static constexpr size_t  s_kMaxIndent       = 0xFF - s_kIndentBase;





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::PascalVolume
//
////////////////////////////////////////////////////////////////////////////////

PascalVolume::PascalVolume (const vector<Byte> & sectors)
    : m_sectors (sectors)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::GetWord
//
////////////////////////////////////////////////////////////////////////////////

Word PascalVolume::GetWord (std::span<const Byte> bytes, size_t offset)
{
    return (Word) (bytes[offset] | (bytes[offset + 1] << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::SetWord
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::SetWord (std::span<Byte> bytes, size_t offset, Word value)
{
    bytes[offset]     = (Byte) (value & 0xFF);
    bytes[offset + 1] = (Byte) (value >> 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool PascalVolume::IsFound (const vector<Byte> & sectors)
{
    std::array<Byte, kBlockBytes>  block   = {};
    bool                           isFound = sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    size_t                         half    = 0;
    size_t                         length  = 0;
    int                            total   = 0;



    for (half = 0; isFound && half < block.size(); half += (size_t) NibblizationLayer::kSectorByteSize)
    {
        std::copy_n (sectors.begin() + (ptrdiff_t) ProDosSkeleton::GetBlockByteOffset (kFirstDirectoryBlock, half), NibblizationLayer::kSectorByteSize, block.begin() + (ptrdiff_t) half);
    }

    if (isFound)
    {
        length  = block[s_kOffNameLength];
        total   = GetWord (block, s_kOffVolumeBlocks);
        isFound = GetWord (block, s_kOffFirst) == 0 && GetWord (block, s_kOffNext) == kFirstDataBlock && GetWord (block, s_kOffKind) == 0
               && length >= 1 && length <= s_kVolumeNameMax
               && total >= kFirstDataBlock && total <= (int) kUnitCount
               && GetWord (block, s_kOffFileCount) <= kMaxFiles;
    }

    for (size_t i = 0; isFound && i < length; i++)
    {
        isFound = block[s_kOffName + i] > ' ' && block[s_kOffName + i] < 0x7F;
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::IsValidName
//
////////////////////////////////////////////////////////////////////////////////

bool PascalVolume::IsValidName (const std::string & name)
{
    bool  isValid = !name.empty() && name.size() <= s_kFileNameMax;



    for (size_t i = 0; isValid && i < name.size(); i++)
    {
        isValid = name[i] > ' ' && name[i] < 0x7F && strchr ("$=?,:[]", name[i]) == nullptr;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::IsValidVolumeName
//
////////////////////////////////////////////////////////////////////////////////

bool PascalVolume::IsValidVolumeName (const std::string & name)
{
    return name.size() <= s_kVolumeNameMax && IsValidName (name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::MakeDate
//
////////////////////////////////////////////////////////////////////////////////

Word PascalVolume::MakeDate (int year, int month, int day)
{
    constexpr int  kCentury = 100;



    return (Word) (((year % kCentury) << 9) | (day << 4) | month);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::WriteBlank
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::WriteBlank (const std::string & volumeName, Word date, vector<Byte> & ioSectors)
{
    std::array<Byte, kBlockBytes>  header = {};
    std::array<Byte, kBlockBytes>  empty  = {};



    SetWord (header, s_kOffNext,         (Word) kFirstDataBlock);
    SetWord (header, s_kOffVolumeBlocks, (Word) kUnitCount);
    SetWord (header, s_kOffVolumeDate,   date);

    header[s_kOffNameLength] = (Byte) volumeName.size();

    for (size_t i = 0; i < volumeName.size(); i++)
    {
        header[s_kOffName + i] = (Byte) toupper ((unsigned char) volumeName[i]);
    }

    WriteBlockPart (ioSectors, kFirstDirectoryBlock, 0, header);

    for (int block = kFirstDirectoryBlock + 1; block < kFirstDataBlock; block++)
    {
        WriteBlockPart (ioSectors, block, 0, empty);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::DescribeMissingSystem
//
////////////////////////////////////////////////////////////////////////////////

std::string PascalVolume::DescribeMissingSystem (const vector<Byte> & sectors)
{
    PascalVolume                       volume (sectors);
    std::array<Byte, 2 * kBlockBytes>  loader    = {};
    VolumeListing                      listing;
    HRESULT                            hr        = volume.Enumerate (listing);
    bool                               hasRun    = false;
    bool                               hasApple  = false;
    bool                               hasPascal = false;



    if (FAILED (hr))
    {
        return "its directory can't be read";
    }

    volume.ReadBlock (0, std::span<Byte> (loader).subspan (0, kBlockBytes));
    volume.ReadBlock (1, std::span<Byte> (loader).subspan (kBlockBytes, kBlockBytes));

    hasRun = std::any_of (loader.begin(), loader.end(), [&loader] (Byte value) { return value != loader[0]; });

    for (const FileEntry & entry : listing.entries)
    {
        hasApple  = hasApple  || _stricmp (entry.name.c_str(), "SYSTEM.APPLE")  == 0;
        hasPascal = hasPascal || _stricmp (entry.name.c_str(), "SYSTEM.PASCAL") == 0;
    }

    return !hasRun    ? "it has no boot loader in blocks 0 and 1"
         : !hasApple  ? "it has no SYSTEM.APPLE"
         : !hasPascal ? "it has no SYSTEM.PASCAL"
                      : "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::InstallSystem
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::InstallSystem (const vector<Byte> & source, vector<Byte> & ioSectors)
{
    HRESULT                        hr     = S_OK;
    PascalVolume                   from (source);
    std::array<Byte, kBlockBytes>  block  = {};
    VolumeListing                  listing;



    hr = from.Enumerate (listing);
    CHR (hr);

    for (int i = 0; i < kFirstDirectoryBlock; i++)
    {
        from.ReadBlock (i, block);
        WriteBlockPart (ioSectors, i, 0, block);
    }

    for (const FileEntry & entry : listing.entries)
    {
        FilePath      path = FilePath::Parse (entry.name);
        FilePayload   payload;
        vector<Byte>  next;
        bool          isSystem = _strnicmp (entry.name.c_str(), "SYSTEM.", strlen ("SYSTEM.")) == 0
                              && _strnicmp (entry.name.c_str(), "SYSTEM.WRK", strlen ("SYSTEM.WRK")) != 0
                              && _strnicmp (entry.name.c_str(), "SYSTEM.LST", strlen ("SYSTEM.LST")) != 0;

        if (!isSystem)
        {
            continue;
        }

        hr = from.Read (path, payload);
        CHR (hr);

        hr = PascalVolume (ioSectors).Write (path, payload, next);
        CHR (hr);

        ioSectors = std::move (next);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::GetKindFromName
//
////////////////////////////////////////////////////////////////////////////////

Byte PascalVolume::GetKindFromName (const std::string & name)
{
    static constexpr struct { const char * suffix; Byte kind; }  kSuffixes[] =
    {
        { ".TEXT", kKindText },
        { ".CODE", kKindCode },
        { ".DATA", kKindData },
        { ".BAD",  kKindBad  },
        { ".INFO", kKindInfo },
        { ".GRAF", kKindGraf },
        { ".FOTO", kKindFoto },
    };



    for (const auto & row : kSuffixes)
    {
        size_t  length = strlen (row.suffix);

        if (name.size() > length && _stricmp (name.c_str() + (name.size() - length), row.suffix) == 0)
        {
            return row.kind;
        }
    }

    return kKindData;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::ToProDosDate
//
//  Year, month and day in ProDOS's bit order, which the rest of the tree reads.
//
////////////////////////////////////////////////////////////////////////////////

Word PascalVolume::ToProDosDate (Word date)
{
    return (Word) (((date >> 9) << 9) | ((date & 0x0F) << 5) | ((date >> 4) & 0x1F));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::FromProDosDate
//
////////////////////////////////////////////////////////////////////////////////

Word PascalVolume::FromProDosDate (Word date)
{
    return (Word) (((date >> 9) << 9) | ((date & 0x1F) << 4) | ((date >> 5) & 0x0F));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::ReadBlock (int block, std::span<Byte> outBytes) const
{
    constexpr size_t  kSectorBytes = (size_t) NibblizationLayer::kSectorByteSize;



    for (size_t half = 0; half < kBlockBytes; half += kSectorBytes)
    {
        size_t  at = ProDosSkeleton::GetBlockByteOffset (block, half);

        if (m_readObserver != nullptr)
        {
            m_readObserver->OnSectorRead (at / kSectorBytes);
        }

        std::copy_n (m_sectors.begin() + (ptrdiff_t) at, kSectorBytes, outBytes.begin() + (ptrdiff_t) half);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::ReadDirectory
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::ReadDirectory (vector<Byte> & outDirectory) const
{
    outDirectory.assign (s_kDirectoryBytes, 0);

    for (int i = 0; i < 4; i++)
    {
        ReadBlock (kFirstDirectoryBlock + i, std::span<Byte> (outDirectory).subspan ((size_t) i * kBlockBytes, kBlockBytes));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::ReadEntries
//
//  Every entry the volume's file count covers. One with a bad name or a run
//  of blocks that can't fit the volume is marked invalid and reported as damage.
//  A rewrite of the directory doesn't remove it.
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::ReadEntries (const vector<Byte> & directory, vector<RawEntry> & outEntries, vector<std::string> & outDamage) const
{
    int   total    = GetWord (directory, s_kOffVolumeBlocks);
    int   count    = (std::min) ((int) GetWord (directory, s_kOffFileCount), kMaxFiles);
    int   previous = kFirstDataBlock;



    outEntries.clear();

    for (int slot = 1; slot <= count; slot++)
    {
        RawEntry  entry;
        size_t    at     = (size_t) slot * s_kEntryBytes;
        size_t    length = directory[at + s_kOffNameLength];

        std::copy_n (directory.begin() + (ptrdiff_t) at, s_kEntryBytes, entry.bytes.begin());

        entry.slot      = slot;
        entry.first     = GetWord (entry.bytes, s_kOffFirst);
        entry.next      = GetWord (entry.bytes, s_kOffNext);
        entry.kind      = (Byte) (entry.bytes[s_kOffKind] & s_kKindMask);
        entry.lastBytes = GetWord (entry.bytes, s_kOffLastBytes);
        entry.date      = GetWord (entry.bytes, s_kOffDate);
        entry.isValid   = length >= 1 && length <= s_kFileNameMax
                       && entry.first >= kFirstDataBlock && entry.next > entry.first && entry.next <= total && total <= (int) kUnitCount;

        if (length >= 1 && length <= s_kFileNameMax)
        {
            entry.name.assign ((const char *) entry.bytes.data() + s_kOffName, length);
        }

        if (!entry.isValid)
        {
            outDamage.push_back (std::format ("directory entry {} isn't a valid file entry", slot));
        }
        else if (entry.first < previous)
        {
            outDamage.push_back (std::format ("{} starts at block {}, before the end of the file ahead of it", entry.name, entry.first));
        }

        previous = entry.isValid ? (std::max) (previous, entry.next) : previous;

        outEntries.push_back (entry);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::FillEntry
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::FillEntry (const RawEntry & raw, size_t index, FileEntry & outEntry)
{
    uint32_t  blocks = (uint32_t) (raw.next - raw.first);
    uint32_t  last   = (std::min) ((uint32_t) raw.lastBytes, (uint32_t) kBlockBytes);



    outEntry              = FileEntry();
    outEntry.name         = raw.name;
    outEntry.type         = raw.kind;
    outEntry.sizeUnits    = blocks;
    outEntry.eofBytes     = (blocks - 1) * (uint32_t) kBlockBytes + last;
    outEntry.hasEofBytes  = true;
    outEntry.catalogIndex = index;
    outEntry.hasModified  = ProDosVolume::TryToUnixTime (ToProDosDate (raw.date), 0, outEntry.modifiedUnix);
    outEntry.firstBlock     = raw.first;
    outEntry.lastBlockBytes = raw.lastBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::Enumerate
//
//  Free space is the volume's blocks past the directory that no entry covers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::Enumerate (VolumeListing & outListing) const
{
    HRESULT                hr        = S_OK;
    bool                   whole     = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    size_t                 nameBytes = 0;
    vector<Byte>           directory;
    vector<RawEntry>       entries;
    VolumeIntegrityReport  report;
    uint32_t               used      = 0;



    outListing = VolumeListing();

    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    ReadEntries   (directory, entries, outListing.damage);

    nameBytes = (std::min) ((size_t) directory[s_kOffNameLength], s_kVolumeNameMax);

    outListing.volumeName.assign ((const char *) directory.data() + s_kOffName, nameBytes);
    outListing.hasVolumeName = true;
    outListing.totalUnits    = (std::min) ((uint32_t) GetWord (directory, s_kOffVolumeBlocks), kUnitCount);
    outListing.directoryFiles = (uint32_t) entries.size();

    for (const RawEntry & raw : entries)
    {
        FileEntry  entry;

        if (raw.isValid)
        {
            FillEntry (raw, outListing.entries.size(), entry);
            outListing.entries.push_back (entry);
        }
    }

    hr = BuildIntegrityReport (report);
    CHR (hr);

    for (uint32_t unit = 0; unit < outListing.totalUnits; unit++)
    {
        used += report.IsClaimed (unit) ? 1u : 0u;
    }

    outListing.freeUnits = outListing.totalUnits - used;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::TryFindEntry
//
////////////////////////////////////////////////////////////////////////////////

bool PascalVolume::TryFindEntry (const vector<RawEntry> & entries, const std::string & name, size_t & outIndex)
{
    for (size_t i = 0; i < entries.size(); i++)
    {
        if (_stricmp (entries[i].name.c_str(), name.c_str()) == 0)
        {
            outIndex = i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT             hr     = S_OK;
    bool                whole  = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                found  = false;
    size_t              index  = 0;
    vector<Byte>        directory;
    vector<RawEntry>    entries;
    vector<std::string> damage;
    FileEntry           entry;



    outPayload = FilePayload();

    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    ReadEntries   (directory, entries, damage);

    found = TryFindEntry (entries, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    found = entries[index].isValid;
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    FillEntry (entries[index], index, entry);

    outPayload.bytes.assign ((size_t) entry.sizeUnits * kBlockBytes, 0);

    for (uint32_t i = 0; i < entry.sizeUnits; i++)
    {
        ReadBlock (entries[index].first + (int) i, std::span<Byte> (outPayload.bytes).subspan ((size_t) i * kBlockBytes, kBlockBytes));
    }

    outPayload.bytes.resize (entry.eofBytes);

    outPayload.type     = entries[index].kind;
    outPayload.textForm = PayloadTextForm::PascalPages;

    if (entries[index].date != 0)
    {
        outPayload.modifiedDate = ToProDosDate (entries[index].date);
        outPayload.createdDate  = outPayload.modifiedDate;
        outPayload.hasDates     = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::BuildIntegrityReport
//
//  With no free map, an allocated block is one the catalog covers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT              hr    = S_OK;
    bool                 whole = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    vector<Byte>         directory;
    vector<RawEntry>     entries;
    vector<std::string>  damage;



    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    ReadEntries   (directory, entries, damage);

    outReport.Reset (kUnitCount);
    outReport.SetCatalogFullyParsed (damage.empty());

    for (uint32_t unit = 0; unit < (uint32_t) kFirstDataBlock; unit++)
    {
        outReport.AddClaim (unit, VolumeIntegrityReport::kVolumeOwner);
    }

    for (size_t i = 0; i < entries.size(); i++)
    {
        if (!entries[i].isValid)
        {
            outReport.MarkChainUnfollowable ((uint16_t) i);
            continue;
        }

        for (int block = entries[i].first; block < entries[i].next; block++)
        {
            outReport.AddClaim ((uint32_t) block, (uint16_t) i);
        }
    }

    for (uint32_t unit = 0; unit < kUnitCount; unit++)
    {
        outReport.SetAllocatedInFreeMap (unit, outReport.IsClaimed (unit));
    }

    outReport.Finish();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::HandBackResult
//
//  A computed buffer is returned only after it passes the same integrity pass
//  as the volume it came from.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::HandBackResult (const VolumeIntegrityReport & before, const vector<Byte> & result, vector<Byte> & outBuffer)
{
    HRESULT                hr     = S_OK;
    PascalVolume           volume (result);
    bool                   safe   = false;
    VolumeIntegrityReport  after;



    hr = volume.BuildIntegrityReport (after);
    CHRA (hr);

    safe = after.IsSafeToCommitAfter (before);
    CBRAEx (safe, E_UNEXPECTED);

    outBuffer = result;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::WriteBlockPart
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::WriteBlockPart (vector<Byte> & buffer, int block, size_t offset, std::span<const Byte> bytes)
{
    for (size_t i = 0; i < bytes.size(); i++)
    {
        buffer[ProDosSkeleton::GetBlockByteOffset (block, offset + i)] = bytes[i];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::WriteDirectory
//
//  The entries in order from slot 1, the file count, and the four blocks back
//  into the buffer. Slots past the last entry are cleared.
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::WriteDirectory (vector<Byte> & buffer, vector<Byte> & directory, const vector<RawEntry> & entries, int previousCount)
{
    int  slot = 1;



    for (const RawEntry & entry : entries)
    {
        std::copy (entry.bytes.begin(), entry.bytes.end(), directory.begin() + (ptrdiff_t) ((size_t) slot * s_kEntryBytes));
        slot++;
    }

    for (; slot <= previousCount; slot++)
    {
        std::fill_n (directory.begin() + (ptrdiff_t) ((size_t) slot * s_kEntryBytes), s_kEntryBytes, (Byte) 0);
    }

    SetWord (directory, s_kOffFileCount, (Word) entries.size());

    for (int i = 0; i < 4; i++)
    {
        WriteBlockPart (buffer, kFirstDirectoryBlock + i, 0, std::span<const Byte> (directory).subspan ((size_t) i * kBlockBytes, kBlockBytes));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::FindGap
//
//  The first run of free blocks long enough, or -1. A file is contiguous, so a
//  volume with enough free blocks in pieces has no room for it.
//
////////////////////////////////////////////////////////////////////////////////

int PascalVolume::FindGap (const vector<RawEntry> & entries, int total, int blocks)
{
    vector<std::pair<int, int>>  runs;
    int                          at = kFirstDataBlock;



    for (const RawEntry & entry : entries)
    {
        if (entry.isValid)
        {
            runs.push_back ({ entry.first, entry.next });
        }
    }

    std::sort (runs.begin(), runs.end());

    for (const auto & [first, next] : runs)
    {
        if (first - at >= blocks)
        {
            return at;
        }

        at = (std::max) (at, next);
    }

    return (total - at >= blocks) ? at : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::Krunch
//
//  Blocks are copied out of the unchanged source into the result, so a file
//  moved onto where the file ahead of it was reads as it did.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::Krunch (vector<Byte> & outBuffer, int & outMoved) const
{
    HRESULT                hr       = S_OK;
    bool                   whole    = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   isClean  = false;
    int                    at       = kFirstDataBlock;
    vector<Byte>           directory;
    vector<Byte>           result;
    vector<RawEntry>       entries;
    vector<std::string>    damage;
    VolumeIntegrityReport  report;



    outMoved = 0;

    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    ReadEntries   (directory, entries, damage);

    isClean = damage.empty();
    CBREx (isClean, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = BuildIntegrityReport (report);
    CHR (hr);

    result = m_sectors;

    for (RawEntry & entry : entries)
    {
        int  blocks = entry.next - entry.first;

        if (entry.first != at)
        {
            for (int i = 0; i < blocks; i++)
            {
                std::array<Byte, kBlockBytes>  block = {};

                ReadBlock (entry.first + i, block);
                WriteBlockPart (result, at + i, 0, block);
            }

            SetWord (entry.bytes, s_kOffFirst, (Word) at);
            SetWord (entry.bytes, s_kOffNext,  (Word) (at + blocks));

            entry.first = at;
            entry.next  = at + blocks;

            outMoved++;
        }

        at += blocks;
    }

    WriteDirectory (result, directory, entries, (int) entries.size());

    hr = HandBackResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::SetName
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::SetName (RawEntry & entry, const std::string & name)
{
    std::fill_n (entry.bytes.begin() + (ptrdiff_t) s_kOffName, s_kFileNameMax, (Byte) 0);

    entry.bytes[s_kOffNameLength] = (Byte) name.size();
    entry.name                    = name;

    for (size_t i = 0; i < name.size(); i++)
    {
        entry.bytes[s_kOffName + i] = (Byte) toupper ((unsigned char) name[i]);
        entry.name[i]               = (char) entry.bytes[s_kOffName + i];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::Write
//
//  A file of the same name is removed from a copy of the directory first, so
//  its blocks are free for the new one. Nothing is returned until the whole
//  result is finished. The last block is zero filled.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    HRESULT                hr         = S_OK;
    std::string            name       = path.ToString();
    bool                   whole      = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   nameOk     = IsValidName (name);
    bool                   hasRoom    = false;
    size_t                 size       = payload.bytes.size();
    int                    blocks     = (std::max) (1, (int) ((size + kBlockBytes - 1) / kBlockBytes));
    int                    total      = 0;
    int                    first      = -1;
    int                    previous   = 0;
    int                    freeBlocks = 0;
    size_t                 index      = 0;
    Byte                   kind       = (payload.type >= kKindBad && payload.type <= kKindSecureDir) ? payload.type : GetKindFromName (name);
    vector<Byte>           directory;
    vector<Byte>           result;
    vector<RawEntry>       entries;
    vector<std::string>    damage;
    VolumeIntegrityReport  report;
    RawEntry               entry;



    CBREx (whole,  E_INVALIDARG);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    ReadDirectory (directory);
    ReadEntries   (directory, entries, damage);

    previous = (int) entries.size();
    total    = (std::min) ((int) GetWord (directory, s_kOffVolumeBlocks), (int) kUnitCount);

    hr = BuildIntegrityReport (report);
    CHR (hr);

    if (TryFindEntry (entries, name, index))
    {
        entries.erase (entries.begin() + (ptrdiff_t) index);
    }

    hasRoom = (int) entries.size() < kMaxFiles;
    CBREx (hasRoom, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    first   = FindGap (entries, total, blocks);
    hasRoom = first >= 0;

    for (const RawEntry & other : entries)
    {
        freeBlocks -= other.isValid ? other.next - other.first : 0;
    }

    freeBlocks += total - kFirstDataBlock;

    CBREx (hasRoom, HRESULT_FROM_WIN32 ((freeBlocks >= blocks) ? ERROR_DISK_TOO_FRAGMENTED : ERROR_DISK_FULL));

    entry.first = first;
    entry.next  = first + blocks;

    SetWord (entry.bytes, s_kOffFirst, (Word) first);
    SetWord (entry.bytes, s_kOffNext,  (Word) (first + blocks));
    SetWord (entry.bytes, s_kOffKind,  kind);
    SetWord (entry.bytes, s_kOffLastBytes, (size == 0) ? (Word) 0 : (Word) (size - (size_t) (blocks - 1) * kBlockBytes));
    SetWord (entry.bytes, s_kOffDate,  payload.hasDates ? FromProDosDate (payload.modifiedDate) : (Word) 0);
    SetName (entry, name);

    entry.isValid = true;

    result = m_sectors;

    for (int i = 0; i < blocks; i++)
    {
        std::array<Byte, kBlockBytes>  block = {};
        size_t                         from  = (size_t) i * kBlockBytes;
        size_t                         count = (std::min) (kBlockBytes, size - (std::min) (size, from));

        if (count > 0)
        {
            std::copy_n (payload.bytes.begin() + (ptrdiff_t) from, count, block.begin());
        }

        WriteBlockPart (result, first + i, 0, block);
    }

    entries.insert (std::find_if (entries.begin(), entries.end(), [first] (const RawEntry & other) { return other.first > first; }), entry);

    WriteDirectory (result, directory, entries, previous);

    hr = HandBackResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    DeleteOutcome  ignored;



    return Delete (path, outBuffer, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::Delete
//
//  The entry comes out of the directory and the entries after it move up. The
//  blocks are free because no entry covers them. A block another entry also
//  covers isn't free, so it isn't counted as freed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    HRESULT                hr       = S_OK;
    bool                   whole    = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   found    = false;
    size_t                 index    = 0;
    int                    previous = 0;
    vector<Byte>           directory;
    vector<Byte>           result;
    vector<RawEntry>       entries;
    vector<std::string>    damage;
    VolumeIntegrityReport  report;



    outOutcome = DeleteOutcome();

    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    ReadEntries   (directory, entries, damage);

    found = TryFindEntry (entries, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = BuildIntegrityReport (report);
    CHR (hr);

    outOutcome.catalogFullyParsed = damage.empty();
    outOutcome.chainWasDamaged    = !entries[index].isValid;

    for (uint32_t unit : report.GetClaimsOf ((uint16_t) index))
    {
        if (report.IsUniquelyOwnedBy (unit, (uint16_t) index))
        {
            outOutcome.freedUnits.push_back (unit);
        }
    }

    previous = (int) entries.size();
    entries.erase (entries.begin() + (ptrdiff_t) index);

    result = m_sectors;

    WriteDirectory (result, directory, entries, previous);

    hr = HandBackResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::SetStartupProgram
//
//  Pascal runs SYSTEM.STARTUP by that name. It has no setting for this.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::EditEntry
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::EditEntry (const FilePath & path, const std::function<void (RawEntry &)> & edit, vector<Byte> & outBuffer) const
{
    HRESULT                hr       = S_OK;
    bool                   whole    = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   found    = false;
    size_t                 index    = 0;
    int                    previous = 0;
    vector<Byte>           directory;
    vector<Byte>           result;
    vector<RawEntry>       entries;
    vector<std::string>    damage;
    VolumeIntegrityReport  report;



    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    ReadEntries   (directory, entries, damage);

    found = TryFindEntry (entries, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = BuildIntegrityReport (report);
    CHR (hr);

    edit (entries[index]);

    previous = (int) entries.size();
    result   = m_sectors;

    WriteDirectory (result, directory, entries, previous);

    hr = HandBackResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    HRESULT              hr     = S_OK;
    bool                 nameOk = IsValidName (to);
    bool                 taken  = false;
    size_t               holder = 0;
    vector<Byte>         directory;
    vector<RawEntry>     entries;
    vector<std::string>  damage;



    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    ReadDirectory (directory);
    ReadEntries   (directory, entries, damage);

    taken = TryFindEntry (entries, to, holder) && _stricmp (entries[holder].name.c_str(), from.ToString().c_str()) != 0;
    CBREx (!taken, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    hr = EditEntry (from, [&to] (RawEntry & entry) { SetName (entry, to); }, outBuffer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::SetAttributes
//
//  Pascal stores a kind and one date, and no access or aux type.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    HRESULT  hr        = S_OK;
    bool     stored    = !attributes.hasAccess && !attributes.hasAuxType && !attributes.hasCreated;
    bool     validKind = !attributes.hasType || attributes.type <= kKindSecureDir;



    CBREx (stored,    HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));
    CBREx (validKind, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    hr = EditEntry (path, [&attributes] (RawEntry & entry)
    {
        if (attributes.hasType)
        {
            entry.bytes[s_kOffKind] = (Byte) ((entry.bytes[s_kOffKind] & ~s_kKindMask) | attributes.type);
        }

        if (attributes.hasModified)
        {
            SetWord (entry.bytes, s_kOffDate, FromProDosDate (attributes.modifiedDate));
        }
    }, outBuffer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::DecodeText
//
////////////////////////////////////////////////////////////////////////////////

void PascalVolume::DecodeText (std::span<const Byte> bytes, std::string & outText)
{
    size_t  at        = s_kTextPageBytes;
    bool    lineStart = true;



    outText.clear();

    while (at < bytes.size())
    {
        Byte  value = bytes[at++];

        if (value == s_kDataLinkEscape && at < bytes.size())
        {
            Byte  count = bytes[at++];

            outText.append ((count > s_kIndentBase) ? (size_t) (count - s_kIndentBase) : 0, ' ');
            lineStart = false;
        }
        else if (value == '\r')
        {
            outText  += "\r\n";
            lineStart = true;
        }
        else if (value != 0)
        {
            outText.push_back ((char) value);
            lineStart = false;
        }
    }

    if (!lineStart)
    {
        outText += "\r\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolume::EncodeText
//
//  A two-block header of zeros, then each line as an indent code, its text and
//  a return. A line that doesn't fit what is left of a page starts the next
//  page, which zeros pad to its end. A longer line than a page is split.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalVolume::EncodeText (const std::string & text, vector<Byte> & outBytes, size_t & outBadOffset)
{
    HRESULT       hr         = S_OK;
    size_t        at         = 0;
    size_t        pageStart  = s_kTextPageBytes;
    bool          isAscii    = true;
    vector<Byte>  line;



    outBytes.assign (s_kTextPageBytes, 0);
    outBadOffset = 0;

    auto  appendLine = [&] ()
    {
        if (outBytes.size() + line.size() > pageStart + s_kTextPageBytes)
        {
            pageStart += s_kTextPageBytes;
            outBytes.resize (pageStart, 0);
        }

        outBytes.insert (outBytes.end(), line.begin(), line.end());
        line.clear();
    };

    while (at < text.size())
    {
        size_t  end    = text.find_first_of ("\r\n", at);
        size_t  stop   = (end == std::string::npos) ? text.size() : end;
        size_t  indent = 0;

        while (at + indent < stop && text[at + indent] == ' ' && indent < s_kMaxIndent)
        {
            indent++;
        }

        if (indent > 0)
        {
            line.push_back (s_kDataLinkEscape);
            line.push_back ((Byte) (s_kIndentBase + indent));
        }

        for (size_t i = at + indent; i < stop; i++)
        {
            isAscii = (Byte) text[i] < 0x80;

            if (!isAscii)
            {
                outBadOffset = i;
            }

            CBREx (isAscii, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            line.push_back ((Byte) text[i]);

            if (line.size() >= s_kTextPageBytes - 1)
            {
                line.push_back ('\r');
                appendLine();
            }
        }

        line.push_back ('\r');
        appendLine();

        at = stop;

        if (at < text.size() && text[at] == '\r')
        {
            at++;
        }

        if (at < text.size() && text[at] == '\n')
        {
            at++;
        }
    }

    outBytes.resize (pageStart + s_kTextPageBytes, 0);

Error:
    return hr;
}
