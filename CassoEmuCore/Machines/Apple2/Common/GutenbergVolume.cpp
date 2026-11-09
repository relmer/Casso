#include "Pch.h"

#include "Machines/Apple2/Common/GutenbergVolume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





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
            file.type   = (Byte) (m_sectors[entry + s_kOffType] & s_kLowBits);

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
//  The format of the allocation map is unknown, so the free count is the
//  sectors from track 3 on that aren't in the directory or a file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Enumerate (VolumeListing & outListing) const
{
    HRESULT                hr    = S_OK;
    bool                   whole = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    uint32_t               spare = 0;
    vector<RawFile>        files;
    Chain                  directory;
    std::string            volumeName;
    VolumeIntegrityReport  report;



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

    hr = BuildIntegrityReport (report);
    CHR (hr);

    for (uint32_t unit = (uint32_t) (kFirstDataTrack * kSectorsPerTrack); unit < kUnitCount; unit++)
    {
        spare += report.IsClaimed (unit) ? 0u : 1u;
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
//  GutenbergVolume::BuildIntegrityReport
//
//  The allocation map isn't read, so an allocated sector is one in the
//  directory or a file, and the map sector.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT          hr    = S_OK;
    bool             whole = m_sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    vector<RawFile>  files;
    Chain            directory;
    std::string      volumeName;



    CBREx (whole, E_INVALIDARG);

    CollectFiles (files, directory, volumeName);

    outReport.Reset (kUnitCount);
    outReport.SetCatalogFullyParsed (directory.damage.empty());
    outReport.AddClaim ((uint32_t) (kDirectoryTrack * kSectorsPerTrack + kMapSector), VolumeIntegrityReport::kVolumeOwner);

    for (uint32_t unit : directory.units)
    {
        outReport.AddClaim (unit, VolumeIntegrityReport::kVolumeOwner);
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
        }

        if (!chain.damage.empty())
        {
            outReport.MarkChainUnfollowable ((uint16_t) i);
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
//  GutenbergVolume::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (payload);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);
    UNREFERENCED_PARAMETER (outOutcome);

    return kReadOnly;
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

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (from);
    UNREFERENCED_PARAMETER (to);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GutenbergVolume::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GutenbergVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (attributes);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}
