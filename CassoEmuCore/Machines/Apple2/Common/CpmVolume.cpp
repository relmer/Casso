#include "Pch.h"

#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





static constexpr size_t  s_kOffName         = 1;
static constexpr size_t  s_kOffType         = 9;
static constexpr size_t  s_kOffSystem       = 10;
static constexpr size_t  s_kOffExtent       = 12;
static constexpr size_t  s_kOffS2           = 14;
static constexpr size_t  s_kOffRecords      = 15;
static constexpr size_t  s_kOffBlocks       = 16;
static constexpr size_t  s_kNameBytes       = 11;
static constexpr size_t  s_kBaseBytes       = 8;
static constexpr size_t  s_kTypeBytes       = 3;
static constexpr int     s_kUserMax         = 31;
static constexpr int     s_kUserSystem      = 31;
static constexpr int     s_kExtentMax       = 31;
static constexpr int     s_kRecordsMax      = 128;
static constexpr int     s_kBlocksPerEntry  = 16;
static constexpr Byte    s_kHighBit         = 0x80;
static constexpr size_t  s_kDirectoryBytes  = (size_t) CpmVolume::kEntries * CpmVolume::kEntryBytes;
static constexpr int     s_kMinWithInvalid  = 3;
static constexpr int     s_kDirectorySectors = CpmVolume::kDirectoryBlocks * CpmVolume::kSectorsPerBlock;
static constexpr int     s_kUserVisibleMax  = 15;





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::CpmVolume
//
////////////////////////////////////////////////////////////////////////////////

CpmVolume::CpmVolume (const vector<Byte> & sectors)
    : m_sectors (sectors)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::GetSectorOffset
//
//  A CP/M sector is physical sector 3n mod 16, and the buffer's order is DOS
//  logical order.
//
////////////////////////////////////////////////////////////////////////////////

size_t CpmVolume::GetSectorOffset (int block, int sector)
{
    constexpr int  kSkew     = 3;
    int            track     = (kDirectoryTrack + block / kSectorsPerBlock) % NibblizationLayer::kTrackCount;
    int            cpmSector = kSectorsPerBlock * (block % kSectorsPerBlock) + sector;
    int            physical  = (kSkew * cpmSector) % NibblizationLayer::kSectorsPerTrack;



    return ((size_t) track * NibblizationLayer::kSectorsPerTrack + (size_t) NibblizationLayer::GetDosFileIndexForPhysicalSector (physical)) * NibblizationLayer::kSectorByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::GetDirectorySectors
//
////////////////////////////////////////////////////////////////////////////////

vector<size_t> CpmVolume::GetDirectorySectors()
{
    vector<size_t>  sectors;



    for (int block = 0; block < kDirectoryBlocks; block++)
    {
        for (int sector = 0; sector < kSectorsPerBlock; sector++)
        {
            sectors.push_back (GetSectorOffset (block, sector) / NibblizationLayer::kSectorByteSize);
        }
    }

    return sectors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::IsPlausibleEntry
//
//  A user of 0 to 31, an extent and record count CP/M 2.2 allows, a printable
//  name and block numbers inside the volume. The system entry in user 31 can
//  have blocks on tracks 0 to 2. Block 1 is the directory, and a block number
//  of 0 in an entry is unused.
//
////////////////////////////////////////////////////////////////////////////////

bool CpmVolume::IsPlausibleEntry (std::span<const Byte> entry)
{
    int   user    = entry[0];
    bool  isValid = user <= s_kUserMax && entry[s_kOffExtent] <= s_kExtentMax && entry[s_kOffS2] == 0 && entry[s_kOffRecords] <= s_kRecordsMax;



    for (size_t i = 0; isValid && i < s_kNameBytes; i++)
    {
        Byte  value = (Byte) (entry[s_kOffName + i] & ~s_kHighBit);

        isValid = value >= ' ' && value < 0x7F;
    }

    isValid = isValid && entry[s_kOffName] != ' ';

    for (size_t i = 0; isValid && i < (size_t) s_kBlocksPerEntry; i++)
    {
        Byte  block = entry[s_kOffBlocks + i];

        isValid = block != 1 && block < ((user == s_kUserSystem) ? kUnitCount : kVolumeBlocks);
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool CpmVolume::IsFound (const vector<Byte> & sectors)
{
    vector<Byte>  directory (s_kDirectoryBytes, 0);
    int           valid     = 0;
    int           invalid   = 0;
    int           sector    = 0;
    bool          allFree   = true;
    int           limit     = 0;



    if (sectors.size() != (size_t) NibblizationLayer::kImageByteSize)
    {
        return false;
    }

    for (size_t at : GetDirectorySectors())
    {
        std::copy_n (sectors.begin() + (ptrdiff_t) (at * NibblizationLayer::kSectorByteSize), NibblizationLayer::kSectorByteSize,
                     directory.begin() + (ptrdiff_t) ((size_t) sector * NibblizationLayer::kSectorByteSize));
        sector++;
    }

    limit = GetSlotLimit (directory);

    for (int slot = 0; slot < kEntries; slot++)
    {
        std::span<const Byte>  entry = std::span<const Byte> (directory).subspan ((size_t) slot * kEntryBytes, kEntryBytes);

        allFree = allFree && IsFreeSlot (entry);

        if (entry[0] != kFree)
        {
            valid   += IsPlausibleEntry (entry) ? 1 : 0;
            invalid += (!IsPlausibleEntry (entry) && slot < limit) ? 1 : 0;
        }
    }

    return allFree || (valid >= 1 && invalid == 0) || valid >= s_kMinWithInvalid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::IsFreeSlot
//
//  All $E5, as FORMAT writes it, or an entry ERA erased, which has $E5 for its
//  user and the rest of the entry as it was.
//
////////////////////////////////////////////////////////////////////////////////

bool CpmVolume::IsFreeSlot (std::span<const Byte> entry)
{
    std::array<Byte, kEntryBytes>  erased = {};



    if (entry[0] != kFree)
    {
        return false;
    }

    if (std::all_of (entry.begin(), entry.end(), [] (Byte value) { return value == kFree; }))
    {
        return true;
    }

    std::copy (entry.begin() + 1, entry.end(), erased.begin() + 1);

    return IsPlausibleEntry (erased);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::WriteBlank
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::WriteBlank (vector<Byte> & outSectors)
{
    outSectors.assign ((size_t) NibblizationLayer::kImageByteSize, kFree);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::HasSystemTracks
//
////////////////////////////////////////////////////////////////////////////////

bool CpmVolume::HasSystemTracks (const vector<Byte> & sectors)
{
    size_t  bytes = (size_t) kDirectoryTrack * NibblizationLayer::kSectorsPerTrack * NibblizationLayer::kSectorByteSize;



    return sectors.size() >= bytes && std::any_of (sectors.begin(), sectors.begin() + (ptrdiff_t) bytes, [&sectors] (Byte value) { return value != sectors[0]; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::InstallSystem
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::InstallSystem (const vector<Byte> & source, vector<Byte> & ioSectors)
{
    HRESULT       hr     = S_OK;
    size_t        bytes  = (size_t) kDirectoryTrack * NibblizationLayer::kSectorsPerTrack * NibblizationLayer::kSectorByteSize;
    bool          sized  = source.size() == (size_t) NibblizationLayer::kImageByteSize && ioSectors.size() == source.size();
    int           target = 0;
    vector<Byte>  fromDirectory;
    vector<Byte>  directory;



    CBRAEx (sized, E_INVALIDARG);

    std::copy_n (source.begin(), bytes, ioSectors.begin());

    CpmVolume (source).ReadDirectory (fromDirectory);
    CpmVolume (ioSectors).ReadDirectory (directory);

    for (int slot = 0; slot < kEntries; slot++)
    {
        std::span<const Byte>  entry    = std::span<const Byte> (fromDirectory).subspan ((size_t) slot * kEntryBytes, kEntryBytes);
        bool                   isSystem = entry[0] == s_kUserSystem && IsPlausibleEntry (entry) && entry[s_kOffBlocks] >= kVolumeBlocks;

        if (isSystem)
        {
            std::copy (entry.begin(), entry.end(), directory.begin() + (ptrdiff_t) ((size_t) target * kEntryBytes));
            target++;
        }
    }

    WriteDirectory (ioSectors, directory);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::TryParseName
//
////////////////////////////////////////////////////////////////////////////////

bool CpmVolume::TryParseName (const std::string & name, ParsedName & outParsed)
{
    size_t       colon   = name.find (':');
    std::string  rest    = (colon == std::string::npos) ? name : name.substr (colon + 1);
    size_t       dot     = rest.find ('.');
    std::string  base    = (dot == std::string::npos) ? rest : rest.substr (0, dot);
    std::string  type    = (dot == std::string::npos) ? std::string() : rest.substr (dot + 1);
    bool         isValid = !base.empty() && base.size() <= s_kBaseBytes && type.size() <= s_kTypeBytes;



    outParsed      = ParsedName();
    outParsed.name.fill (' ');

    if (colon != std::string::npos)
    {
        std::string  user = name.substr (0, colon);

        isValid = isValid && !user.empty() && user.size() <= 2 && std::all_of (user.begin(), user.end(), [] (char ch) { return ch >= '0' && ch <= '9'; });

        if (isValid)
        {
            outParsed.user = atoi (user.c_str());
            isValid        = outParsed.user <= s_kUserVisibleMax;
        }
    }

    for (const std::string * part : { &base, &type })
    {
        for (size_t i = 0; isValid && i < part->size(); i++)
        {
            char  ch = (*part)[i];

            isValid = ch > ' ' && ch < 0x7F && strchr ("<>.,;:=?*[]", ch) == nullptr;

            if (isValid)
            {
                outParsed.name[((part == &base) ? 0 : s_kBaseBytes) + i] = (char) toupper ((unsigned char) ch);
            }
        }
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::MakeName
//
//  The name as a listing has it.
//
////////////////////////////////////////////////////////////////////////////////

std::string CpmVolume::MakeName (const ParsedName & parsed)
{
    std::string  base (parsed.name.data(), s_kBaseBytes);
    std::string  type (parsed.name.data() + s_kBaseBytes, s_kTypeBytes);
    std::string  name = (parsed.user != 0) ? std::format ("{}:", parsed.user) : std::string();



    base.erase (base.find_last_not_of (' ') + 1);
    type.erase (type.find_last_not_of (' ') + 1);

    return name + (type.empty() ? base : base + "." + type);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::IsValidName
//
////////////////////////////////////////////////////////////////////////////////

bool CpmVolume::IsValidName (const std::string & name)
{
    ParsedName  parsed;



    return TryParseName (name, parsed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::ReadBlock (int block, std::span<Byte> outBytes) const
{
    constexpr size_t  kSectorBytes = (size_t) NibblizationLayer::kSectorByteSize;



    for (int sector = 0; sector < kSectorsPerBlock; sector++)
    {
        size_t  at = GetSectorOffset (block, sector);

        if (m_readObserver != nullptr)
        {
            m_readObserver->OnSectorRead (at / kSectorBytes);
        }

        std::copy_n (m_sectors.begin() + (ptrdiff_t) at, kSectorBytes, outBytes.begin() + (ptrdiff_t) ((size_t) sector * kSectorBytes));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::ReadDirectory
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::ReadDirectory (vector<Byte> & outDirectory) const
{
    outDirectory.assign (s_kDirectoryBytes, 0);

    for (int block = 0; block < kDirectoryBlocks; block++)
    {
        ReadBlock (block, std::span<Byte> (outDirectory).subspan ((size_t) block * kBlockBytes, kBlockBytes));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::CollectFiles
//
//  An entry that isn't plausible is counted, and otherwise left alone: its slot
//  isn't free, and its block numbers aren't a file's.
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::CollectFiles (const vector<Byte> & directory, vector<RawFile> & outFiles, int & outInvalid)
{
    int  limit = GetSlotLimit (directory);



    outFiles.clear();
    outInvalid = 0;

    for (int slot = 0; slot < kEntries; slot++)
    {
        std::span<const Byte>  bytes = std::span<const Byte> (directory).subspan ((size_t) slot * kEntryBytes, kEntryBytes);
        RawEntry               entry;
        std::string            base;
        std::string            type;
        size_t                 file  = 0;
        std::string            name;

        if (bytes[0] == kFree)
        {
            continue;
        }

        if (!IsPlausibleEntry (bytes))
        {
            outInvalid += (slot < limit) ? 1 : 0;
            continue;
        }

        for (size_t i = 0; i < s_kBaseBytes; i++)
        {
            base.push_back ((char) (bytes[s_kOffName + i] & ~s_kHighBit));
        }

        for (size_t i = 0; i < s_kTypeBytes; i++)
        {
            type.push_back ((char) (bytes[s_kOffType + i] & ~s_kHighBit));
        }

        base.erase (base.find_last_not_of (' ') + 1);

        type.erase (type.find_last_not_of (' ') + 1);

        name = (bytes[0] != 0) ? std::format ("{}:", (int) bytes[0]) : std::string();
        name += type.empty() ? base : base + "." + type;

        entry.slot       = slot;
        entry.user       = bytes[0];
        entry.extent     = bytes[s_kOffExtent];
        entry.records    = bytes[s_kOffRecords];
        entry.isReadOnly = (bytes[s_kOffType] & s_kHighBit) != 0;
        entry.isSystem   = (bytes[s_kOffSystem] & s_kHighBit) != 0;
        entry.name       = name;

        std::copy_n (bytes.begin() + (ptrdiff_t) s_kOffBlocks, entry.blocks.size(), entry.blocks.begin());

        if (!TryFindFile (outFiles, name, file))
        {
            outFiles.emplace_back();
            file                = outFiles.size() - 1;
            outFiles[file].name = name;
        }

        outFiles[file].isReadOnly = outFiles[file].isReadOnly || entry.isReadOnly;
        outFiles[file].isSystem   = outFiles[file].isSystem || entry.isSystem;
        outFiles[file].isFixed    = outFiles[file].isFixed || entry.user == s_kUserSystem;
        outFiles[file].extents.push_back (entry);
    }

    for (RawFile & file : outFiles)
    {
        std::stable_sort (file.extents.begin(), file.extents.end(), [] (const RawEntry & a, const RawEntry & b) { return a.extent < b.extent; });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::GetSlotLimit
//
////////////////////////////////////////////////////////////////////////////////

int CpmVolume::GetSlotLimit (const vector<Byte> & directory)
{
    for (int slot = kCompatibleEntries; slot < kEntries; slot++)
    {
        std::span<const Byte>  bytes = std::span<const Byte> (directory).subspan ((size_t) slot * kEntryBytes, kEntryBytes);

        if (bytes[0] != kFree && IsPlausibleEntry (bytes))
        {
            return kEntries;
        }
    }

    return kCompatibleEntries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::CollectStrayBlocks
//
////////////////////////////////////////////////////////////////////////////////

std::set<Byte> CpmVolume::CollectStrayBlocks (const vector<Byte> & directory)
{
    int             limit = GetSlotLimit (directory);
    std::set<Byte>  blocks;



    for (int slot = 0; slot < limit; slot++)
    {
        std::span<const Byte>  bytes = std::span<const Byte> (directory).subspan ((size_t) slot * kEntryBytes, kEntryBytes);

        if (bytes[0] > s_kUserMax || IsPlausibleEntry (bytes))
        {
            continue;
        }

        for (size_t i = 0; i < (size_t) s_kBlocksPerEntry; i++)
        {
            Byte  block = bytes[s_kOffBlocks + i];

            if (block >= kDirectoryBlocks && block < kVolumeBlocks)
            {
                blocks.insert (block);
            }
        }
    }

    return blocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::TryFindFile
//
////////////////////////////////////////////////////////////////////////////////

bool CpmVolume::TryFindFile (const vector<RawFile> & files, const std::string & name, size_t & outIndex)
{
    for (size_t i = 0; i < files.size(); i++)
    {
        if (_stricmp (files[i].name.c_str(), name.c_str()) == 0)
        {
            outIndex = i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::CountRecords
//
//  With 1 KB blocks on a disk this size, each extent is 16 KB of 128 records,
//  so the length comes from the last entry, whatever the extents before it list.
//
////////////////////////////////////////////////////////////////////////////////

int CpmVolume::CountRecords (const RawFile & file)
{
    const RawEntry &  last = file.extents.back();



    return last.extent * s_kRecordsMax + last.records;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::FillEntry
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::FillEntry (const RawFile & file, size_t index, FileEntry & outEntry)
{
    std::set<Byte>  blocks;



    for (const RawEntry & extent : file.extents)
    {
        for (Byte block : extent.blocks)
        {
            if (block != 0)
            {
                blocks.insert (block);
            }
        }
    }

    outEntry              = FileEntry();
    outEntry.name         = file.name;
    outEntry.isLocked     = file.isReadOnly || file.isFixed;
    outEntry.sizeUnits    = (uint32_t) blocks.size();
    outEntry.eofBytes     = (uint32_t) CountRecords (file) * (uint32_t) kRecordBytes;
    outEntry.hasEofBytes  = true;
    outEntry.catalogIndex = index;
    outEntry.records      = (uint32_t) CountRecords (file);
    outEntry.extents      = (uint32_t) file.extents.size();
    outEntry.user         = file.extents.front().user;
    outEntry.isReadOnly   = file.isReadOnly;
    outEntry.isSystem     = file.isSystem;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::Enumerate (VolumeListing & outListing) const
{
    HRESULT                hr      = S_OK;
    bool                   whole   = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    int                    invalid = 0;
    uint32_t               used    = 0;
    uint32_t               wrapped = 0;
    vector<Byte>           directory;
    vector<RawFile>        files;
    VolumeIntegrityReport  report;



    outListing = VolumeListing();

    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    CollectFiles  (directory, files, invalid);

    for (size_t i = 0; i < files.size(); i++)
    {
        FileEntry  entry;

        FillEntry (files[i], i, entry);
        outListing.entries.push_back (entry);
    }

    if (invalid != 0)
    {
        outListing.damage.push_back ((invalid == 1) ? std::string ("1 directory entry isn't a valid CP/M entry")
                                                    : std::format ("{} directory entries aren't valid CP/M entries", invalid));
    }

    hr = BuildIntegrityReport (report);
    CHR (hr);

    for (uint32_t unit = 0; unit < kVolumeBlocks; unit++)
    {
        used += report.IsClaimed (unit) ? 1u : 0u;
    }

    for (uint32_t unit = kVolumeBlocks; unit < kUnitCount; unit++)
    {
        wrapped += report.IsClaimed (unit) ? 1u : 0u;
    }

    outListing.totalUnits = kVolumeBlocks + wrapped;
    outListing.freeUnits  = kVolumeBlocks - used;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::Read
//
//  Block number 0 in an entry is a hole, which reads as zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT          hr      = S_OK;
    bool             whole   = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool             found   = false;
    size_t           index   = 0;
    size_t           size    = 0;
    int              invalid = 0;
    vector<Byte>     directory;
    vector<RawFile>  files;



    outPayload = FilePayload();

    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    CollectFiles  (directory, files, invalid);

    found = TryFindFile (files, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    size = (size_t) CountRecords (files[index]) * kRecordBytes;

    outPayload.bytes.assign (size, 0);
    outPayload.textForm = PayloadTextForm::CpmRecords;
    outPayload.access   = (files[index].isReadOnly || files[index].isFixed) ? FilePayload::kAccessLocked : FilePayload::kAccessUnlocked;
    outPayload.hasAccess = true;
    outPayload.isSystem  = files[index].isSystem;

    for (const RawEntry & extent : files[index].extents)
    {
        for (int i = 0; i < s_kBlocksPerEntry; i++)
        {
            std::array<Byte, kBlockBytes>  block = {};
            size_t                         at    = ((size_t) extent.extent * s_kBlocksPerEntry + (size_t) i) * kBlockBytes;

            if (extent.blocks[(size_t) i] == 0 || at >= size)
            {
                continue;
            }

            ReadBlock (extent.blocks[(size_t) i], block);
            std::copy_n (block.begin(), (std::min) (kBlockBytes, size - at), outPayload.bytes.begin() + (ptrdiff_t) at);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::BuildIntegrityReport
//
//  With no free map, an allocated block is one the directory lists, including
//  in an invalid entry real CP/M would read as a file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT          hr      = S_OK;
    bool             whole   = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    int              invalid = 0;
    vector<Byte>     directory;
    vector<RawFile>  files;



    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    CollectFiles  (directory, files, invalid);

    outReport.Reset (kUnitCount);
    outReport.SetCatalogFullyParsed (invalid == 0);

    for (uint32_t unit = 0; unit < (uint32_t) kDirectoryBlocks; unit++)
    {
        outReport.AddClaim (unit, VolumeIntegrityReport::kVolumeOwner);
    }

    for (Byte block : CollectStrayBlocks (directory))
    {
        outReport.AddClaim (block, VolumeIntegrityReport::kVolumeOwner);
    }

    for (size_t i = 0; i < files.size(); i++)
    {
        for (const RawEntry & extent : files[i].extents)
        {
            for (Byte block : extent.blocks)
            {
                if (block != 0)
                {
                    outReport.AddClaim (block, (uint16_t) i);
                }
            }
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
//  CpmVolume::HandBackResult
//
//  A computed buffer is returned only after it passes the same integrity pass
//  as the volume it came from.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::HandBackResult (const VolumeIntegrityReport & before, const vector<Byte> & result, vector<Byte> & outBuffer)
{
    HRESULT                hr     = S_OK;
    CpmVolume              volume (result);
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
//  CpmVolume::WriteBlockBytes
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::WriteBlockBytes (vector<Byte> & buffer, int block, std::span<const Byte> bytes)
{
    constexpr size_t  kSectorBytes = (size_t) NibblizationLayer::kSectorByteSize;



    for (int sector = 0; sector < kSectorsPerBlock; sector++)
    {
        std::copy_n (bytes.begin() + (ptrdiff_t) ((size_t) sector * kSectorBytes), kSectorBytes, buffer.begin() + (ptrdiff_t) GetSectorOffset (block, sector));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::WriteDirectory
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::WriteDirectory (vector<Byte> & buffer, const vector<Byte> & directory)
{
    for (int block = 0; block < kDirectoryBlocks; block++)
    {
        WriteBlockBytes (buffer, block, std::span<const Byte> (directory).subspan ((size_t) block * kBlockBytes, kBlockBytes));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::Write
//
//  The first free blocks and directory slots, whichever they are, since a CP/M
//  file needn't be contiguous. A file of the same name is removed from a copy
//  first. A file's last record and block are padded with $1A, CP/M's end of
//  text, which a program that isn't reading text doesn't look at.
//
//  A slot with invalid bytes isn't free, and blocks past kVolumeBlocks, tracks 0
//  to 2, are never allocated. Slots stop at the disk's limit: CP/AM's parameter
//  block has 48 directory entries, so an entry past that would be invisible to
//  it, and a disk with a valid entry past that is one that reads them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    HRESULT                hr         = S_OK;
    std::string            name       = path.ToString();
    bool                   whole      = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   nameOk     = false;
    bool                   locked     = false;
    bool                   hasRoom    = false;
    size_t                 index      = 0;
    size_t                 size       = payload.bytes.size();
    size_t                 blocks     = (size + kBlockBytes - 1) / kBlockBytes;
    size_t                 records    = (size + kRecordBytes - 1) / kRecordBytes;
    size_t                 entries    = (std::max) ((size_t) 1, (blocks + s_kBlocksPerEntry - 1) / s_kBlocksPerEntry);
    size_t                 nextBlock  = 0;
    int                    invalid    = 0;
    ParsedName             parsed;
    vector<Byte>           directory;
    vector<Byte>           result;
    vector<RawFile>        files;
    vector<int>            freeSlots;
    vector<int>            freeBlocks;
    VolumeIntegrityReport  report;
    vector<bool>           used (kVolumeBlocks, false);
    int                    slotLimit  = 0;



    CBREx (whole, E_INVALIDARG);

    nameOk = TryParseName (name, parsed);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    ReadDirectory (directory);
    CollectFiles  (directory, files, invalid);

    hr = BuildIntegrityReport (report);
    CHR (hr);

    slotLimit = GetSlotLimit (directory);

    for (Byte block : CollectStrayBlocks (directory))
    {
        used[block] = true;
    }

    if (TryFindFile (files, MakeName (parsed), index))
    {
        locked = files[index].isReadOnly || files[index].isFixed;
        CBREx (!locked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

        for (const RawEntry & extent : files[index].extents)
        {
            directory[(size_t) extent.slot * kEntryBytes] = kFree;
        }

        files.erase (files.begin() + (ptrdiff_t) index);
    }

    for (const RawFile & file : files)
    {
        for (const RawEntry & extent : file.extents)
        {
            for (Byte block : extent.blocks)
            {
                if (block < kVolumeBlocks)
                {
                    used[block] = true;
                }
            }
        }
    }

    for (int block = kDirectoryBlocks; block < (int) kVolumeBlocks; block++)
    {
        if (!used[(size_t) block])
        {
            freeBlocks.push_back (block);
        }
    }

    for (int slot = 0; slot < slotLimit; slot++)
    {
        if (directory[(size_t) slot * kEntryBytes] == kFree)
        {
            freeSlots.push_back (slot);
        }
    }

    hasRoom = freeBlocks.size() >= blocks && freeSlots.size() >= entries;
    CBREx (hasRoom, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    result = m_sectors;

    for (size_t i = 0; i < blocks; i++)
    {
        std::array<Byte, kBlockBytes>  block = {};
        size_t                         from  = i * kBlockBytes;
        size_t                         count = (std::min) (kBlockBytes, size - from);

        block.fill (kTextEnd);
        std::copy_n (payload.bytes.begin() + (ptrdiff_t) from, count, block.begin());

        WriteBlockBytes (result, freeBlocks[i], block);
    }

    for (size_t e = 0; e < entries; e++)
    {
        size_t  at   = (size_t) freeSlots[e] * kEntryBytes;
        size_t  left = records - (std::min) (records, e * s_kRecordsMax);

        std::fill_n (directory.begin() + (ptrdiff_t) at, kEntryBytes, (Byte) 0);

        directory[at] = (Byte) parsed.user;

        std::copy_n ((const Byte *) parsed.name.data(), s_kNameBytes, directory.begin() + (ptrdiff_t) (at + s_kOffName));

        directory[at + s_kOffType]    = (Byte) (directory[at + s_kOffType] | (payload.IsLocked() ? s_kHighBit : 0));
        directory[at + s_kOffSystem]  = (Byte) (directory[at + s_kOffSystem] | (payload.isSystem ? s_kHighBit : 0));
        directory[at + s_kOffExtent]  = (Byte) e;
        directory[at + s_kOffRecords] = (Byte) (std::min) (left, (size_t) s_kRecordsMax);

        for (size_t i = 0; i < (size_t) s_kBlocksPerEntry && nextBlock < blocks; i++)
        {
            directory[at + s_kOffBlocks + i] = (Byte) freeBlocks[nextBlock++];
        }
    }

    WriteDirectory (result, directory);

    hr = HandBackResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    DeleteOutcome  ignored;



    return Delete (path, outBuffer, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::Delete
//
//  $E5 in each of the file's directory entries, as CP/M's ERA command does. The
//  blocks are free because no entry lists them. A block another file also lists
//  isn't free, so it isn't counted as freed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    HRESULT                hr      = S_OK;
    bool                   whole   = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   found   = false;
    bool                   locked  = false;
    size_t                 index   = 0;
    int                    invalid = 0;
    vector<Byte>           directory;
    vector<Byte>           result;
    vector<RawFile>        files;
    VolumeIntegrityReport  report;



    outOutcome = DeleteOutcome();

    CBREx (whole, E_INVALIDARG);

    ReadDirectory (directory);
    CollectFiles  (directory, files, invalid);

    found = TryFindFile (files, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    locked = files[index].isReadOnly || files[index].isFixed;
    CBREx (!locked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = BuildIntegrityReport (report);
    CHR (hr);

    outOutcome.catalogFullyParsed = invalid == 0;

    for (uint32_t unit : report.GetClaimsOf ((uint16_t) index))
    {
        if (report.IsUniquelyOwnedBy (unit, (uint16_t) index))
        {
            outOutcome.freedUnits.push_back (unit);
        }
    }

    for (const RawEntry & extent : files[index].extents)
    {
        directory[(size_t) extent.slot * kEntryBytes] = kFree;
    }

    result = m_sectors;

    WriteDirectory (result, directory);

    hr = HandBackResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::SetStartupProgram
//
//  CP/M runs what its CCP is given, and has no setting on the disk for this.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::Rename
//
//  The name and user in each of the file's entries. A read-only file can't be
//  renamed, and the attribute bits of the name's characters are preserved.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    HRESULT                hr      = S_OK;
    bool                   whole   = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   nameOk  = false;
    bool                   found   = false;
    bool                   locked  = false;
    bool                   taken   = false;
    size_t                 index   = 0;
    size_t                 holder  = 0;
    int                    invalid = 0;
    ParsedName             parsed;
    vector<Byte>           directory;
    vector<Byte>           result;
    vector<RawFile>        files;
    VolumeIntegrityReport  report;



    CBREx (whole, E_INVALIDARG);

    nameOk = TryParseName (to, parsed);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    ReadDirectory (directory);
    CollectFiles  (directory, files, invalid);

    found = TryFindFile (files, from.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    locked = files[index].isReadOnly || files[index].isFixed;
    CBREx (!locked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    taken = TryFindFile (files, MakeName (parsed), holder) && holder != index;
    CBREx (!taken, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    hr = BuildIntegrityReport (report);
    CHR (hr);

    for (const RawEntry & extent : files[index].extents)
    {
        size_t  at = (size_t) extent.slot * kEntryBytes;

        directory[at] = (Byte) parsed.user;

        for (size_t i = 0; i < s_kNameBytes; i++)
        {
            directory[at + s_kOffName + i] = (Byte) ((directory[at + s_kOffName + i] & s_kHighBit) | (Byte) parsed.name[i]);
        }
    }

    result = m_sectors;

    WriteDirectory (result, directory);

    hr = HandBackResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::SetAttributes
//
//  CP/M stores the read-only bit, and no type or dates.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpmVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    HRESULT                hr      = S_OK;
    bool                   whole   = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   stored  = !attributes.hasType && !attributes.hasAuxType && !attributes.hasCreated && !attributes.hasModified;
    bool                   found   = false;
    bool                   locks   = attributes.hasAccess && (attributes.access & FilePayload::kAccessWriteEnable) == 0;
    size_t                 index   = 0;
    int                    invalid = 0;
    vector<Byte>           directory;
    vector<Byte>           result;
    vector<RawFile>        files;
    VolumeIntegrityReport  report;



    CBREx (whole,  E_INVALIDARG);
    CBREx (stored, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    ReadDirectory (directory);
    CollectFiles  (directory, files, invalid);

    found = TryFindFile (files, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = BuildIntegrityReport (report);
    CHR (hr);

    if (attributes.hasAccess)
    {
        for (const RawEntry & extent : files[index].extents)
        {
            size_t  at = (size_t) extent.slot * kEntryBytes + s_kOffType;

            directory[at] = (Byte) (locks ? (directory[at] | s_kHighBit) : (directory[at] & ~s_kHighBit));
        }
    }

    result = m_sectors;

    WriteDirectory (result, directory);

    hr = HandBackResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::DecodeText
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::DecodeText (std::span<const Byte> bytes, std::string & outText)
{
    constexpr Byte  kCharacterBits = 0x7F;
    auto            end            = std::find (bytes.begin(), bytes.end(), kTextEnd);



    outText.clear();

    //  A word processor's high bits on a CP/M text file aren't characters.
    for (auto at = bytes.begin(); at != end; ++at)
    {
        outText.push_back ((char) (*at & kCharacterBits));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolume::EncodeText
//
////////////////////////////////////////////////////////////////////////////////

void CpmVolume::EncodeText (const std::string & text, vector<Byte> & outBytes)
{
    char  previous = 0;



    outBytes.clear();

    for (char ch : text)
    {
        if (ch == '\n' && previous != '\r')
        {
            outBytes.push_back ('\r');
        }

        if (ch != '\r' && previous == '\r' && ch != '\n')
        {
            outBytes.push_back ('\n');
        }

        outBytes.push_back ((Byte) ch);

        previous = ch;
    }

    if (previous == '\r')
    {
        outBytes.push_back ('\n');
    }

    outBytes.push_back (kTextEnd);
}
