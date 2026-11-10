#include "Pch.h"

#include "Machines/Apple2/Common/RdosVolume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





static constexpr size_t    s_kNameBytes        = 24;
static constexpr size_t    s_kOffType          = 0x18;
static constexpr size_t    s_kOffSectorCount   = 0x19;
static constexpr size_t    s_kOffLoadAddress   = 0x1A;
static constexpr size_t    s_kOffLength        = 0x1C;
static constexpr size_t    s_kOffFirstSector   = 0x1E;
static constexpr Byte      s_kDeleted          = 0x80;
static constexpr Byte      s_kListEnd          = 0x00;
static constexpr Byte      s_kTextEnd          = 0x00;
static constexpr Byte      s_kLowBits          = 0x7F;
static constexpr int       s_kSectorsRdos33    = 16;
static constexpr int       s_kSectorsRdos3     = 13;
static constexpr int       s_kCatalogRdos33    = 16;
static constexpr int       s_kCatalogRdos3     = 11;
static constexpr uint32_t  s_kSystemTracks     = 2;
static constexpr Byte      s_kProDosText       = 0x04;
static constexpr Byte      s_kProDosBinary     = 0x06;
static constexpr Byte      s_kProDosApplesoft  = 0xFC;

static constexpr const char *  s_kpszSystemPrefix  = "RDOS ";
static constexpr const char *  s_kpszOldVersion    = "RDOS 2";
static constexpr const char *  s_kpszSaveDisks[]   =
{
    "SSI SAVE GAME DISK RDOS",
    " >-SSI GAME SAVE DISK-<",
};





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::RdosVolume
//
////////////////////////////////////////////////////////////////////////////////

RdosVolume::RdosVolume (const vector<Byte> & sectors)
    : m_sectors (sectors),
      m_layout  (DetectLayout (sectors))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::GetSectorsPerTrack
//
////////////////////////////////////////////////////////////////////////////////

int RdosVolume::GetSectorsPerTrack (Layout layout)
{
    return (layout == Layout::Rdos3) ? s_kSectorsRdos3 : s_kSectorsRdos33;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::GetCatalogSectors
//
////////////////////////////////////////////////////////////////////////////////

int RdosVolume::GetCatalogSectors (Layout layout)
{
    return (layout == Layout::Rdos3) ? s_kCatalogRdos3 : s_kCatalogRdos33;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::GetSectorTotal
//
////////////////////////////////////////////////////////////////////////////////

uint32_t RdosVolume::GetSectorTotal (Layout layout)
{
    return (layout == Layout::None) ? 0u : (uint32_t) (kTracks * GetSectorsPerTrack (layout));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::GetUnit
//
//  Rdos33 counts a track's sectors in the order of a ProDOS-ordered file,
//  and Rdos3 in the order of the address fields.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t RdosVolume::GetUnit (Layout layout, uint32_t index)
{
    uint32_t  perTrack = (uint32_t) GetSectorsPerTrack (layout);
    uint32_t  track    = index / perTrack;
    int       position = (int) (index % perTrack);
    int       logical  = 0;



    if (layout == Layout::Rdos3)
    {
        logical = NibblizationLayer::GetDosFileIndexForPhysicalSector (position);
    }
    else
    {
        for (int candidate = 0; candidate < kBufferSectorsPerTrack; candidate++)
        {
            if (NibblizationLayer::GetPoFileIndexForDosLogicalSector (candidate) == position)
            {
                logical = candidate;
            }
        }
    }

    return track * (uint32_t) kBufferSectorsPerTrack + (uint32_t) logical;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::GetProDosType
//
////////////////////////////////////////////////////////////////////////////////

Byte RdosVolume::GetProDosType (Byte type)
{
    return (type == kTypeApplesoft) ? s_kProDosApplesoft
         : (type == kTypeBinary)    ? s_kProDosBinary
         : (type == kTypeText)      ? s_kProDosText
                                    : (Byte) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word RdosVolume::ReadWord (const vector<Byte> & sectors, size_t at)
{
    return (Word) (sectors[at] | (sectors[at + 1] << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::ReadText
//
//  High ASCII, with the padding at the end removed.
//
////////////////////////////////////////////////////////////////////////////////

std::string RdosVolume::ReadText (const vector<Byte> & sectors, size_t at, size_t length)
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
//  RdosVolume::IsSystemName
//
//  The first entry of a game disk begins with RDOS and a version. A disk
//  made for saved games has one of two other first entries.
//
////////////////////////////////////////////////////////////////////////////////

bool RdosVolume::IsSystemName (const std::string & name)
{
    bool  isSystem = name.rfind (s_kpszSystemPrefix, 0) == 0;



    for (const char * saveDisk : s_kpszSaveDisks)
    {
        isSystem = isSystem || name == saveDisk;
    }

    return isSystem;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::DetectLayout
//
//  Sector 0 of track 1 is the first catalog sector in both layouts. The
//  first entry's length is two tracks, which is 26 sectors in Rdos3 and 32
//  in Rdos33. For any other length, version 2 in the name is Rdos3.
//
////////////////////////////////////////////////////////////////////////////////

RdosVolume::Layout RdosVolume::DetectLayout (const vector<Byte> & sectors)
{
    Layout       layout  = Layout::None;
    size_t       at      = (size_t) (kCatalogTrack * kBufferSectorsPerTrack) * kSectorBytes;
    bool         isFound = sectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    uint32_t     count   = 0;
    std::string  name;



    if (isFound)
    {
        name    = ReadText (sectors, at, s_kNameBytes);
        count   = sectors[at + s_kOffSectorCount];
        isFound = IsSystemName (name) && ReadWord (sectors, at + s_kOffFirstSector) == 0;
    }

    if (isFound)
    {
        layout = (count == s_kSystemTracks * s_kSectorsRdos3)  ? Layout::Rdos3
               : (count == s_kSystemTracks * s_kSectorsRdos33) ? Layout::Rdos33
               : (name.rfind (s_kpszOldVersion, 0) == 0)       ? Layout::Rdos3
                                                               : Layout::Rdos33;
    }

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::CollectFiles
//
//  Deleted entries are left out. An entry whose sectors or length don't fit
//  is listed with its damage.
//
////////////////////////////////////////////////////////////////////////////////

void RdosVolume::CollectFiles (vector<RawFile> & outFiles) const
{
    uint32_t  perTrack = (uint32_t) GetSectorsPerTrack (m_layout);
    uint32_t  total    = GetSectorTotal (m_layout);
    bool      done     = false;



    outFiles.clear();

    for (int sector = 0; !done && sector < GetCatalogSectors (m_layout); sector++)
    {
        uint32_t  unit = GetUnit (m_layout, (uint32_t) kCatalogTrack * perTrack + (uint32_t) sector);

        if (m_readObserver != nullptr)
        {
            m_readObserver->OnSectorRead (unit);
        }

        for (int slot = 0; !done && slot < kEntriesPerSector; slot++)
        {
            size_t   at = (size_t) unit * kSectorBytes + (size_t) slot * kEntryBytes;
            RawFile  file;

            if (m_sectors[at] == s_kListEnd)
            {
                done = true;
                continue;
            }

            if (m_sectors[at] == s_kDeleted)
            {
                continue;
            }

            file.name        = ReadText (m_sectors, at, s_kNameBytes);
            file.slot        = (size_t) sector * kEntriesPerSector + (size_t) slot;
            file.type        = (Byte) (m_sectors[at + s_kOffType] & s_kLowBits);
            file.sectorCount = m_sectors[at + s_kOffSectorCount];
            file.loadAddress = ReadWord (m_sectors, at + s_kOffLoadAddress);
            file.length      = ReadWord (m_sectors, at + s_kOffLength);
            file.firstSector = ReadWord (m_sectors, at + s_kOffFirstSector);

            if (file.firstSector + file.sectorCount > total)
            {
                file.damage = "the file's sectors end past the end of the disk";
            }
            else if ((size_t) file.length > (size_t) file.sectorCount * kSectorBytes)
            {
                file.damage = "the length is greater than the file's sectors";
            }

            outFiles.push_back (file);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::ReadBytes
//
//  The bytes of a file without damage: its sectors cut to the length in the
//  catalog, and a T file cut at its first $00.
//
////////////////////////////////////////////////////////////////////////////////

void RdosVolume::ReadBytes (const RawFile & file, bool notify, vector<Byte> & outBytes) const
{
    outBytes.clear();

    for (uint32_t i = 0; i < file.sectorCount; i++)
    {
        uint32_t  unit = GetUnit (m_layout, file.firstSector + i);
        size_t    at   = (size_t) unit * kSectorBytes;

        if (notify && m_readObserver != nullptr)
        {
            m_readObserver->OnSectorRead (unit);
        }

        outBytes.insert (outBytes.end(), m_sectors.begin() + (ptrdiff_t) at, m_sectors.begin() + (ptrdiff_t) (at + kSectorBytes));
    }

    outBytes.resize (file.length);

    if (file.type == kTypeText)
    {
        outBytes.erase (std::find (outBytes.begin(), outBytes.end(), s_kTextEnd), outBytes.end());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::TryFindFile
//
//  By catalog position when the path has one, because two entries can have one name.
//
////////////////////////////////////////////////////////////////////////////////

bool RdosVolume::TryFindFile (const vector<RawFile> & files, const FilePath & path, size_t & outIndex)
{
    std::string  name  = path.ToString();
    bool         found = false;



    for (size_t i = 0; !found && i < files.size(); i++)
    {
        if (files[i].name == name && (!path.HasLeafIndex() || path.GetLeafIndex() == files[i].slot))
        {
            outIndex = i;
            found    = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::Enumerate
//
//  The catalog has no free count, so it is the sectors outside every entry.
//  A deleted entry's sectors are in that count.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::Enumerate (VolumeListing & outListing) const
{
    HRESULT          hr      = S_OK;
    bool             isFound = m_layout != Layout::None;
    uint32_t         total   = GetSectorTotal (m_layout);
    uint32_t         used    = 0;
    vector<bool>     inFile (total, false);
    vector<RawFile>  files;
    vector<Byte>     bytes;



    outListing = VolumeListing();

    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

    CollectFiles (files);

    for (const RawFile & file : files)
    {
        FileEntry  entry;

        entry.name           = file.name;
        entry.type           = file.type;
        entry.sizeUnits      = file.sectorCount;
        entry.eofBytes       = file.length;
        entry.hasEofBytes    = true;
        entry.loadAddress    = file.loadAddress;
        entry.hasLoadAddress = file.type == kTypeBinary;
        entry.catalogIndex   = file.slot;
        entry.firstBlock     = (int) file.firstSector;

        if (!file.damage.empty())
        {
            outListing.damage.push_back (std::format ("{} is damaged: {}", file.name, file.damage));
            outListing.entries.push_back (entry);
            continue;
        }

        if (file.type == kTypeText)
        {
            ReadBytes (file, false, bytes);

            entry.eofBytes = (uint32_t) bytes.size();
        }

        for (uint32_t i = 0; i < file.sectorCount; i++)
        {
            used += inFile[file.firstSector + i] ? 0u : 1u;

            inFile[file.firstSector + i] = true;
        }

        outListing.entries.push_back (entry);
    }

    outListing.totalUnits = total;
    outListing.freeUnits  = total - used;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::Read
//
//  A file with damage fails with ERROR_FILE_CORRUPT.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT          hr      = S_OK;
    bool             isFound = m_layout != Layout::None;
    bool             found   = false;
    bool             intact  = false;
    size_t           index   = 0;
    vector<RawFile>  files;



    outPayload = FilePayload();

    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

    CollectFiles (files);

    found = TryFindFile (files, path, index);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    intact = files[index].damage.empty();
    CBREx (intact, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    ReadBytes (files[index], true, outPayload.bytes);

    outPayload.type           = files[index].type;
    outPayload.loadAddress    = files[index].loadAddress;
    outPayload.hasLoadAddress = files[index].type == kTypeBinary;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::BuildIntegrityReport
//
//  There is no allocation map, so an allocated sector is one in a file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT          hr      = S_OK;
    bool             isFound = m_layout != Layout::None;
    vector<RawFile>  files;



    CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

    CollectFiles (files);

    outReport.Reset (kUnitCount);
    outReport.SetCatalogFullyParsed (true);

    for (size_t i = 0; i < files.size(); i++)
    {
        if (!files[i].damage.empty())
        {
            outReport.MarkChainUnfollowable ((uint16_t) i);
            continue;
        }

        for (uint32_t at = 0; at < files[i].sectorCount; at++)
        {
            outReport.AddClaim (GetUnit (m_layout, files[i].firstSector + at), (uint16_t) i);
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
//  RdosVolume::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (payload);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);
    UNREFERENCED_PARAMETER (outOutcome);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (from);
    UNREFERENCED_PARAMETER (to);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RdosVolume::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RdosVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (attributes);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}
