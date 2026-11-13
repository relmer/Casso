#include "Pch.h"

#include "Machines/Apple2/Common/GutenbergVolume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





static constexpr size_t  s_kOffPreviousTrack  = 0;
static constexpr size_t  s_kOffPreviousSector = 1;
static constexpr size_t  s_kOffCurrentTrack  = 2;
static constexpr size_t  s_kOffCurrentSector = 3;
static constexpr size_t  s_kOffNextTrack     = 4;
static constexpr size_t  s_kOffNextSector    = 5;
static constexpr size_t  s_kOffVolumeName    = 6;
static constexpr size_t  s_kVolumeNameBytes  = 9;
static constexpr size_t  s_kOffEntries       = 16;
static constexpr size_t  s_kNameBytes        = 12;
static constexpr size_t  s_kOffStartTrack    = 12;
static constexpr size_t  s_kOffStartSector   = 13;
static constexpr size_t  s_kOffType          = 14;
static constexpr size_t  s_kOffMarker        = 15;
static constexpr Byte    s_kMarker           = 0x8D;
static constexpr Byte    s_kEmptySlot        = 0xA0;
static constexpr Byte    s_kHighBit          = 0x80;
static constexpr Byte    s_kLowBits          = 0x7F;
static constexpr Byte    s_kStoredZero       = 0x40;
static constexpr char    s_kDelete           = 0x7F;
static constexpr uint32_t  s_kSectorsPerMapByte = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::GutenbergVolume
//
////////////////////////////////////////////////////////////////////////////////

GutenbergVolume::GutenbergVolume (const vector<Byte> & sectors)
    : m_sectors (sectors)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::DecodeLink
//
//  Bit 7 is a flag in three of the six header bytes, and $40 is 0.
//
////////////////////////////////////////////////////////////////////////////////

int GutenbergVolume::DecodeLink (Byte stored)
{
    Byte  value = (Byte) (stored & s_kLowBits);



    return (value == s_kStoredZero) ? 0 : value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::WalkChain
//
//  A link is followed only into a sector whose current track and sector are
//  where the sector is. That stops at a bad link into the boot code, and a
//  file with sectors in tracks 0 to 2 is followed as any other.
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::WalkChain (const vector<Byte> & sectors, int track, int sector, Chain & outChain)
{
    vector<bool>  visited (kUnitCount, false);
    bool          done = false;



    outChain = Chain();

    while (!done)
    {
        uint32_t      unit   = (uint32_t) (track * kSectorsPerTrack + sector);
        const Byte  * header = nullptr;

        if (track >= kTracks || sector >= kSectorsPerTrack)
        {
            outChain.damage = std::format ("a link to track {}, sector {} is outside the disk", track, sector);
            break;
        }

        if (visited[unit])
        {
            outChain.damage = std::format ("the chain returns to track {}, sector {} before its end", track, sector);
            break;
        }

        header = &sectors[(size_t) unit * kSectorBytes];

        if (DecodeLink (header[s_kOffCurrentTrack]) != track || DecodeLink (header[s_kOffCurrentSector]) != sector)
        {
            outChain.damage = std::format ("a link to track {}, sector {} isn't to a sector of a file", track, sector);
            break;
        }

        visited[unit] = true;
        outChain.units.push_back (unit);

        done   = (header[s_kOffNextTrack] & s_kHighBit) != 0;
        track  = DecodeLink (header[s_kOffNextTrack]);
        sector = DecodeLink (header[s_kOffNextSector]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::IsDirectorySector
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::IsDirectorySector (const vector<Byte> & sectors, uint32_t unit)
{
    size_t  at      = (size_t) unit * kSectorBytes;
    bool    isValid = true;



    for (size_t marker = s_kOffMarker; isValid && marker < kSectorBytes; marker += kEntryBytes)
    {
        isValid = sectors[at + marker] == s_kMarker;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::IsFound (const vector<Byte> & sectors)
{
    Chain  directory;
    bool   isFound = sectors.size() == (size_t) NibblizationLayer::kImageByteSize;



    if (isFound)
    {
        WalkChain (sectors, kDirectoryTrack, kDirectorySector, directory);

        isFound = directory.damage.empty() && directory.units.size() <= (size_t) kDirectorySectorsMax;
    }

    for (size_t i = 0; isFound && i < directory.units.size(); i++)
    {
        isFound = IsDirectorySector (sectors, directory.units[i]);
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::ReadText
//
//  High ASCII, with the padding at the end removed.
//
////////////////////////////////////////////////////////////////////////////////

std::string GutenbergVolume::ReadText (const vector<Byte> & sectors, size_t at, size_t length)
{
    std::string  text;



    for (size_t i = 0; i < length; i++)
    {
        char  ch = (char) (sectors[at + i] & s_kLowBits);

        text.push_back ((ch < ' ' || ch == 0x7F) ? '?' : ch);
    }

    while (!text.empty() && text.back() == ' ')
    {
        text.pop_back();
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::CollectFiles
//
//  A directory sector without the $8D markers ends the list.
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::CollectFiles (vector<RawFile> & outFiles, Chain & outDirectory, std::string & outVolumeName) const
{
    outFiles.clear();
    outVolumeName.clear();

    WalkChain (m_sectors, kDirectoryTrack, kDirectorySector, outDirectory);

    for (size_t i = 0; i < outDirectory.units.size() && i < (size_t) kDirectorySectorsMax; i++)
    {
        size_t  at = (size_t) outDirectory.units[i] * kSectorBytes;

        if (!IsDirectorySector (m_sectors, outDirectory.units[i]))
        {
            outDirectory.damage = "a sector of the directory isn't a directory sector";
            outDirectory.units.resize (i);
            break;
        }

        if (m_readObserver != nullptr)
        {
            m_readObserver->OnSectorRead (outDirectory.units[i]);
        }

        if (i == 0)
        {
            outVolumeName = ReadText (m_sectors, at + s_kOffVolumeName, s_kVolumeNameBytes);
        }

        for (int slot = 0; slot < kEntriesPerSector; slot++)
        {
            size_t   entry = at + s_kOffEntries + (size_t) slot * kEntryBytes;
            RawFile  file;

            if (m_sectors[entry] == s_kEmptySlot)
            {
                continue;
            }

            file.name   = ReadText (m_sectors, entry, s_kNameBytes);
            file.track  = DecodeLink (m_sectors[entry + s_kOffStartTrack]);
            file.sector = DecodeLink (m_sectors[entry + s_kOffStartSector]);
            file.type    = (Byte) (m_sectors[entry + s_kOffType] & s_kLowBits);
            file.entryAt = entry;

            outFiles.push_back (file);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::TryFindFile
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::TryFindFile (const vector<RawFile> & files, const std::string & name, size_t & outIndex)
{
    bool  found = false;



    for (size_t i = 0; !found && i < files.size(); i++)
    {
        if (files[i].name == name)
        {
            outIndex = i;
            found    = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::NoteRead
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::NoteRead (const Chain & chain) const
{
    if (m_readObserver != nullptr)
    {
        for (uint32_t unit : chain.units)
        {
            m_readObserver->OnSectorRead (unit);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::Enumerate
//
//  The free count is the sectors the map marks free, the count Gutenberg's
//  AVAILABLE SPACE reports.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Enumerate (VolumeListing & outListing) const
{
    HRESULT          hr    = S_OK;
    bool             whole = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    uint32_t         spare = 0;
    vector<RawFile>  files;
    Chain            directory;
    std::string      volumeName;



    outListing = VolumeListing();

    CBREx (whole, E_INVALIDARG);

    CollectFiles (files, directory, volumeName);

    outListing.volumeName    = volumeName;
    outListing.hasVolumeName = true;

    if (!directory.damage.empty())
    {
        outListing.damage.push_back (std::format ("The directory is damaged: {}", directory.damage));
    }

    for (size_t i = 0; i < files.size(); i++)
    {
        FileEntry  entry;
        Chain      chain;

        WalkChain (m_sectors, files[i].track, files[i].sector, chain);

        entry.name         = files[i].name;
        entry.type         = files[i].type;
        entry.isLocked     = files[i].type == kTypeLocked;
        entry.sizeUnits    = (uint32_t) chain.units.size();
        entry.eofBytes     = (uint32_t) (chain.units.size() * kDataBytes);
        entry.hasEofBytes  = true;
        entry.catalogIndex = i;
        entry.firstBlock   = (uint32_t) (files[i].track * kSectorsPerTrack + files[i].sector);

        outListing.entries.push_back (entry);

        if (!chain.damage.empty())
        {
            outListing.damage.push_back (std::format ("{} is damaged: {}", files[i].name, chain.damage));
        }
    }

    for (uint32_t unit = 0; unit < kUnitCount; unit++)
    {
        bool  allocated = IsAllocatedInMap (m_sectors, unit);

        spare += allocated ? 0u : 1u;
    }

    outListing.totalUnits = kUnitCount;
    outListing.freeUnits  = spare;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::Read
//
//  The 250 data bytes of each sector in the chain. A file with a damaged
//  chain fails with ERROR_FILE_CORRUPT.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT          hr     = S_OK;
    bool             whole  = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool             found  = false;
    bool             intact = false;
    size_t           index  = 0;
    vector<RawFile>  files;
    Chain            directory;
    Chain            chain;
    std::string      volumeName;



    outPayload = FilePayload();

    CBREx (whole, E_INVALIDARG);

    CollectFiles (files, directory, volumeName);

    found = TryFindFile (files, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    WalkChain (m_sectors, files[index].track, files[index].sector, chain);
    intact = chain.damage.empty();
    CBREx (intact, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    NoteRead (chain);

    outPayload.type      = files[index].type;
    outPayload.access    = (files[index].type == kTypeLocked) ? FilePayload::kAccessLocked : FilePayload::kAccessUnlocked;
    outPayload.hasAccess = true;

    for (uint32_t unit : chain.units)
    {
        size_t  at = (size_t) unit * kSectorBytes + kHeaderBytes;

        outPayload.bytes.insert (outPayload.bytes.end(), m_sectors.begin() + (ptrdiff_t) at, m_sectors.begin() + (ptrdiff_t) (at + kDataBytes));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::IsNameValid
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::IsNameValid (const std::string & name)
{
    bool  isValid = !name.empty() && name.size() <= kNameBytesMax;



    for (size_t i = 0; isValid && i < name.size(); i++)
    {
        isValid = name[i] > ' ' && name[i] < s_kDelete && name[i] != '/';
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::IsInMap
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::IsInMap (uint32_t unit)
{
    return unit >= (uint32_t) (kFirstDataTrack * kSectorsPerTrack) && unit < kUnitCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::LocateInMap
//
//  Eight bytes a track from track 3, one byte for each pair of sectors.
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::LocateInMap (uint32_t unit, size_t & outAt, Byte & outBit)
{
    uint32_t  track  = unit / (uint32_t) kSectorsPerTrack;
    uint32_t  sector = unit % (uint32_t) kSectorsPerTrack;



    outAt  = (size_t) (kDirectoryTrack * kSectorsPerTrack + kMapSector) * kSectorBytes;
    outAt += (size_t) (track - (uint32_t) kFirstDataTrack) * kMapBytesPerTrack + sector / s_kSectorsPerMapByte;
    outBit = (sector % s_kSectorsPerMapByte == 0) ? kMapEvenSector : kMapOddSector;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::IsAllocatedInMap
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::IsAllocatedInMap (const vector<Byte> & sectors, uint32_t unit)
{
    bool    allocated = true;
    size_t  at        = 0;
    Byte    bit       = 0;



    if (IsInMap (unit))
    {
        LocateInMap (unit, at, bit);

        allocated = (sectors[at] & bit) != 0;
    }

    return allocated;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::SetAllocatedInMap
//
//  The byte's other bits are left as they are.
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::SetAllocatedInMap (vector<Byte> & sectors, uint32_t unit, bool allocated)
{
    size_t  at  = 0;
    Byte    bit = 0;



    if (IsInMap (unit))
    {
        LocateInMap (unit, at, bit);

        sectors[at] = allocated ? (Byte) (sectors[at] | bit) : (Byte) (sectors[at] & ~bit);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::IsReserved
//
//  A sector reserved for the volume rather than a file: tracks 0 to 2, a
//  sector whose map byte is $FF, and the sectors of track 17 FORMAT marks in
//  use.
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::IsReserved (const vector<Byte> & sectors, uint32_t unit)
{
    bool      reserved  = !IsInMap (unit);
    bool      allocated = IsAllocatedInMap (sectors, unit);
    uint32_t  first     = (uint32_t) (kDirectoryTrack * kSectorsPerTrack);
    size_t    at        = 0;
    Byte      bit       = 0;



    if (!reserved)
    {
        LocateInMap (unit, at, bit);

        reserved = sectors[at] == kMapReserved;
        reserved = reserved || (allocated && unit >= first && unit < first + (uint32_t) kDirectorySectorsFormatted);
    }

    return reserved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::BuildIntegrityReport
//
//  The directory, the map sector and the reserved sectors are the volume's,
//  and the map indicates what is allocated. A file's sector in track 17 or in a
//  $FF byte is the file's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT          hr    = S_OK;
    bool             whole = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    uint32_t         map   = (uint32_t) (kDirectoryTrack * kSectorsPerTrack + kMapSector);
    vector<bool>     claimed (kUnitCount, false);
    vector<RawFile>  files;
    Chain            directory;
    std::string      volumeName;



    CBREx (whole, E_INVALIDARG);

    CollectFiles (files, directory, volumeName);

    outReport.Reset (kUnitCount);
    outReport.SetCatalogFullyParsed (directory.damage.empty());
    outReport.AddClaim (map, VolumeIntegrityReport::kVolumeOwner);
    claimed[map] = true;

    for (uint32_t unit : directory.units)
    {
        outReport.AddClaim (unit, VolumeIntegrityReport::kVolumeOwner);
        claimed[unit] = true;
    }

    //  The first entry is DIR, whose sectors are the directory's.
    for (size_t i = 0; i < files.size(); i++)
    {
        Chain  chain;
        bool   isDirectory = files[i].track == kDirectoryTrack && files[i].sector == kDirectorySector;

        WalkChain (m_sectors, files[i].track, files[i].sector, chain);

        for (size_t at = 0; !isDirectory && at < chain.units.size(); at++)
        {
            outReport.AddClaim (chain.units[at], (uint16_t) i);
            claimed[chain.units[at]] = true;
        }

        if (!chain.damage.empty())
        {
            outReport.MarkChainUnfollowable ((uint16_t) i);
        }
    }

    for (uint32_t unit = 0; unit < kUnitCount; unit++)
    {
        bool  reserved  = !claimed[unit] && IsReserved (m_sectors, unit);
        bool  allocated = IsAllocatedInMap (m_sectors, unit);

        if (reserved)
        {
            outReport.AddClaim (unit, VolumeIntegrityReport::kVolumeOwner);
        }

        outReport.SetAllocatedInFreeMap (unit, allocated);
    }

    outReport.Finish();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::WriteHeader
//
//  A link to the first sector has bit 7 set in its sector, except the last
//  sector's next link, which has bit 7 set in its track.
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::WriteHeader (vector<Byte> & sectors, uint32_t unit, uint32_t previous, uint32_t next, uint32_t first, bool isLast)
{
    size_t    at          = (size_t) unit * kSectorBytes;
    uint32_t  perTrack    = (uint32_t) kSectorsPerTrack;
    Byte      previousTag = (previous == first) ? s_kHighBit : (Byte) 0;
    Byte      currentTag  = (unit == first) ? s_kHighBit : (Byte) 0;



    sectors[at + s_kOffPreviousTrack]  = (Byte) (previous / perTrack);
    sectors[at + s_kOffPreviousSector] = (Byte) ((previous % perTrack) | previousTag);
    sectors[at + s_kOffCurrentTrack]   = (Byte) (unit / perTrack);
    sectors[at + s_kOffCurrentSector]  = (Byte) ((unit % perTrack) | currentTag);

    if (isLast)
    {
        sectors[at + s_kOffNextTrack]  = (Byte) ((first / perTrack) | s_kHighBit);
        sectors[at + s_kOffNextSector] = (Byte) (first % perTrack);
    }
    else
    {
        sectors[at + s_kOffNextTrack]  = (Byte) (next / perTrack);
        sectors[at + s_kOffNextSector] = (Byte) (next % perTrack);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::WriteName
//
//  High ASCII, padded with spaces.
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::WriteName (vector<Byte> & sectors, size_t at, const std::string & name)
{
    for (size_t i = 0; i < s_kNameBytes; i++)
    {
        sectors[at + i] = (i < name.size()) ? (Byte) (name[i] | s_kHighBit) : s_kEmptySlot;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::WriteEntry
//
//  The directory stores a track or sector of 0 as $40.
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::WriteEntry (vector<Byte> & sectors, size_t at, const std::string & name, uint32_t first, Byte type)
{
    Byte  track  = (Byte) (first / (uint32_t) kSectorsPerTrack);
    Byte  sector = (Byte) (first % (uint32_t) kSectorsPerTrack);



    WriteName (sectors, at, name);

    sectors[at + s_kOffStartTrack]  = (track  == 0) ? s_kStoredZero : track;
    sectors[at + s_kOffStartSector] = (sector == 0) ? s_kStoredZero : sector;
    sectors[at + s_kOffType]        = (Byte) (type | s_kHighBit);
    sectors[at + s_kOffMarker]      = s_kMarker;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::ClearEntry
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::ClearEntry (vector<Byte> & sectors, size_t at)
{
    std::fill_n (sectors.begin() + (ptrdiff_t) at, s_kOffMarker, s_kEmptySlot);

    sectors[at + s_kOffMarker] = s_kMarker;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::AddDirectorySector
//
//  The new sector has the volume name and empty slots, and is linked after
//  the directory's last sector. The first sector's previous link isn't
//  changed, and Gutenberg doesn't change it either.
//
////////////////////////////////////////////////////////////////////////////////

void GutenbergVolume::AddDirectorySector (vector<Byte> & sectors, const Chain & directory, uint32_t unit)
{
    uint32_t  first   = directory.units.front();
    uint32_t  last    = directory.units.back();
    size_t    at      = (size_t) unit  * kSectorBytes;
    size_t    firstAt = (size_t) first * kSectorBytes;
    size_t    lastAt  = (size_t) last  * kSectorBytes;



    for (size_t i = s_kOffVolumeName; i < s_kOffEntries; i++)
    {
        sectors[at + i] = sectors[firstAt + i];
    }

    for (size_t i = s_kOffEntries; i < kSectorBytes; i += kEntryBytes)
    {
        ClearEntry (sectors, at + i);
    }

    WriteHeader (sectors, unit, last, first, first, true);

    sectors[lastAt + s_kOffNextTrack]  = (Byte) (unit / (uint32_t) kSectorsPerTrack);
    sectors[lastAt + s_kOffNextSector] = (Byte) (unit % (uint32_t) kSectorsPerTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::TryAllocate
//
//  The lowest sector the map marks free that no file or directory has.
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::TryAllocate (const vector<Byte> & sectors, vector<bool> & ioTaken, uint32_t & outUnit)
{
    bool  found = false;



    for (uint32_t unit = (uint32_t) (kFirstDataTrack * kSectorsPerTrack); !found && unit < kUnitCount; unit++)
    {
        bool  allocated = IsAllocatedInMap (sectors, unit);

        if (!allocated && !ioTaken[unit])
        {
            ioTaken[unit] = true;
            outUnit       = unit;
            found         = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::TryFindEmptySlot
//
////////////////////////////////////////////////////////////////////////////////

bool GutenbergVolume::TryFindEmptySlot (const vector<Byte> & sectors, const Chain & directory, size_t & outAt)
{
    bool  found = false;



    for (size_t i = 0; !found && i < directory.units.size(); i++)
    {
        for (int slot = 0; !found && slot < kEntriesPerSector; slot++)
        {
            size_t  at = (size_t) directory.units[i] * kSectorBytes + s_kOffEntries + (size_t) slot * kEntryBytes;

            if (sectors[at] == s_kEmptySlot)
            {
                outAt = at;
                found = true;
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::GetStoredType
//
//  A type Gutenberg doesn't store becomes a document, and a locked document
//  is L.
//
////////////////////////////////////////////////////////////////////////////////

Byte GutenbergVolume::GetStoredType (const FilePayload & payload)
{
    Byte  type = IsKnownType (payload.type) ? payload.type : kTypeDocument;



    if (type == kTypeDocument && payload.IsLocked())
    {
        type = kTypeLocked;
    }

    return type;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::HandBackResult
//
//  A computed buffer is returned only after it passes the same integrity pass
//  as the volume it came from.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::HandBackResult (const VolumeIntegrityReport & before, const vector<Byte> & result, vector<Byte> & outBuffer)
{
    HRESULT                hr     = S_OK;
    GutenbergVolume        volume (result);
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
//  GutenbergVolume::Write
//
//  A new file takes an empty slot, or a directory sector added for it. A
//  replaced file's sectors are freed first, so the new contents can reuse
//  them. The data is 250 bytes a sector, padded with $00, and an empty file
//  has one sector. A document without a $00 has one added, since Gutenberg
//  ends a document there. A locked file fails with ERROR_ACCESS_DENIED.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    HRESULT                hr         = S_OK;
    bool                   whole      = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    std::string            name       = path.ToString();
    bool                   nameOk     = IsNameValid (name);
    bool                   intact     = false;
    bool                   exists     = false;
    bool                   locked     = false;
    bool                   hasSlot    = false;
    bool                   hasRoom    = true;
    bool                   isDocument = false;
    bool                   hasEnd     = false;
    size_t                 length     = 0;
    size_t                 index      = 0;
    size_t                 entryAt    = 0;
    size_t                 count      = 0;
    uint32_t               unit       = 0;
    vector<RawFile>        files;
    Chain                  directory;
    std::string            volumeName;
    VolumeIntegrityReport  before;
    vector<Byte>           result;
    vector<bool>           taken (kUnitCount, false);
    vector<uint32_t>       units;



    CBREx (whole,  E_INVALIDARG);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    CollectFiles (files, directory, volumeName);

    intact = directory.damage.empty();
    CBREx (intact, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    exists = TryFindFile (files, name, index);
    locked = exists && files[index].type == kTypeLocked;
    CBREx (!locked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = BuildIntegrityReport (before);
    CHR (hr);

    result = m_sectors;

    for (uint32_t i = 0; i < kUnitCount; i++)
    {
        taken[i] = before.IsClaimed (i);
    }

    if (exists)
    {
        entryAt = files[index].entryAt;

        for (uint32_t owned : before.GetClaimsOf ((uint16_t) index))
        {
            bool  isOnly = before.IsUniquelyOwnedBy (owned, (uint16_t) index);

            if (isOnly)
            {
                taken[owned] = false;
                SetAllocatedInMap (result, owned, false);
            }
        }
    }
    else
    {
        hasSlot = TryFindEmptySlot (result, directory, entryAt);

        if (!hasSlot)
        {
            hasRoom = directory.units.size() < (size_t) kDirectorySectorsMax;
            hasRoom = hasRoom && TryAllocate (result, taken, unit);
            CBREx (hasRoom, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

            AddDirectorySector (result, directory, unit);
            SetAllocatedInMap  (result, unit, true);

            entryAt = (size_t) unit * kSectorBytes + s_kOffEntries;
        }
    }

    //  A document ends at its first $00, so one without it gains a byte.
    isDocument = IsDocumentType (GetStoredType (payload));
    hasEnd     = std::find (payload.bytes.begin(), payload.bytes.end(), kDocumentEnd) != payload.bytes.end();
    length     = payload.bytes.size() + ((isDocument && !hasEnd) ? 1 : 0);
    count      = std::max ((size_t) 1, (length + kDataBytes - 1) / kDataBytes);

    for (size_t i = 0; hasRoom && i < count; i++)
    {
        hasRoom = TryAllocate (result, taken, unit);

        if (hasRoom)
        {
            units.push_back (unit);
        }
    }

    CBREx (hasRoom, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    for (size_t i = 0; i < count; i++)
    {
        size_t  at       = (size_t) units[i] * kSectorBytes;
        size_t  from     = i * kDataBytes;
        size_t  length   = std::min (kDataBytes, payload.bytes.size() - std::min (from, payload.bytes.size()));
        bool    isLast   = i + 1 == count;
        size_t  previous = (i == 0) ? count - 1 : i - 1;
        size_t  next     = isLast ? 0 : i + 1;

        WriteHeader (result, units[i], units[previous], units[next], units[0], isLast);

        std::fill_n (result.begin() + (ptrdiff_t) (at + kHeaderBytes), kDataBytes, kDocumentEnd);
        std::copy_n (payload.bytes.begin() + (ptrdiff_t) std::min (from, payload.bytes.size()), length, result.begin() + (ptrdiff_t) (at + kHeaderBytes));

        SetAllocatedInMap (result, units[i], true);
    }

    WriteEntry (result, entryAt, name, units[0], GetStoredType (payload));

    hr = HandBackResult (before, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    DeleteOutcome  outcome;



    return Delete (path, outBuffer, outcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::Delete
//
//  Clears the map bits of the sectors only the file has and fills its entry
//  with spaces, as Gutenberg's DELETE does. The sectors' contents are left.
//  A locked file, DIR among them, fails with ERROR_ACCESS_DENIED.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    HRESULT                hr     = S_OK;
    bool                   whole  = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   found  = false;
    bool                   locked = false;
    size_t                 index  = 0;
    vector<RawFile>        files;
    Chain                  directory;
    Chain                  chain;
    std::string            volumeName;
    VolumeIntegrityReport  before;
    vector<Byte>           result;



    outOutcome = DeleteOutcome();

    CBREx (whole, E_INVALIDARG);

    CollectFiles (files, directory, volumeName);

    found = TryFindFile (files, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    locked = files[index].type == kTypeLocked;
    CBREx (!locked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    hr = BuildIntegrityReport (before);
    CHR (hr);

    WalkChain (m_sectors, files[index].track, files[index].sector, chain);

    outOutcome.catalogFullyParsed = directory.damage.empty();
    outOutcome.chainWasDamaged    = !chain.damage.empty();

    result = m_sectors;

    for (uint32_t unit : before.GetClaimsOf ((uint16_t) index))
    {
        bool  isOnly = before.IsUniquelyOwnedBy (unit, (uint16_t) index);

        if (isOnly)
        {
            outOutcome.freedUnits.push_back (unit);
            SetAllocatedInMap (result, unit, false);
        }
        else
        {
            outOutcome.leakedUnits.push_back (unit);
        }
    }

    if (!outOutcome.leakedUnits.empty())
    {
        outOutcome.warnings.push_back (std::format ("{} sectors of {} are also in another file and stay allocated", outOutcome.leakedUnits.size(), files[index].name));
    }

    ClearEntry (result, files[index].entryAt);

    hr = HandBackResult (before, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::Rename
//
//  Rewrites the entry's name. A locked file fails with ERROR_ACCESS_DENIED,
//  and a name another entry has with ERROR_FILE_EXISTS.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    HRESULT                hr     = S_OK;
    bool                   whole  = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   nameOk = IsNameValid (to);
    bool                   found  = false;
    bool                   locked = false;
    bool                   taken  = false;
    size_t                 index  = 0;
    size_t                 other  = 0;
    vector<RawFile>        files;
    Chain                  directory;
    std::string            volumeName;
    VolumeIntegrityReport  before;
    vector<Byte>           result;



    CBREx (whole,  E_INVALIDARG);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    CollectFiles (files, directory, volumeName);

    found = TryFindFile (files, from.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    locked = files[index].type == kTypeLocked;
    CBREx (!locked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    taken = TryFindFile (files, to, other) && other != index;
    CBREx (!taken, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    hr = BuildIntegrityReport (before);
    CHR (hr);

    result = m_sectors;

    WriteName (result, files[index].entryAt, to);

    hr = HandBackResult (before, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::SetAttributes
//
//  The type and the lock, which is the L type of a document, as Gutenberg's
//  LOCK and UNLOCK set it. A program can't be locked, and the volume stores
//  no aux type or dates. DIR can't be changed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    HRESULT                hr          = S_OK;
    bool                   whole       = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool                   supported   = !attributes.hasAuxType && !attributes.hasCreated && !attributes.hasModified;
    bool                   found       = false;
    bool                   isDirectory = false;
    bool                   typeOk      = true;
    bool                   lockOk      = true;
    size_t                 index       = 0;
    Byte                   type        = 0;
    vector<RawFile>        files;
    Chain                  directory;
    std::string            volumeName;
    VolumeIntegrityReport  before;
    vector<Byte>           result;



    CBREx (whole,     E_INVALIDARG);
    CBREx (supported, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    CollectFiles (files, directory, volumeName);

    found = TryFindFile (files, path.ToString(), index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    isDirectory = files[index].track == kDirectoryTrack && files[index].sector == kDirectorySector;
    CBREx (!isDirectory, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    type = files[index].type;

    if (attributes.hasType)
    {
        typeOk = IsKnownType (attributes.type);
        type   = attributes.type;
    }

    CBREx (typeOk, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER));

    if (attributes.hasAccess)
    {
        bool  wantLocked = (attributes.access & FilePayload::kAccessWriteEnable) == 0;
        bool  isDocument = IsDocumentType (type);

        lockOk = isDocument || !wantLocked;
        type   = isDocument ? (wantLocked ? kTypeLocked : kTypeDocument) : type;
    }

    CBREx (lockOk, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    hr = BuildIntegrityReport (before);
    CHR (hr);

    result = m_sectors;

    result[files[index].entryAt + s_kOffType] = (Byte) (type | s_kHighBit);

    hr = HandBackResult (before, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}
