#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33PayloadCodec.h"
#include "Machines/Apple2/Common/Dos33FileChain.h"
#include "Machines/Apple2/Common/Dos33FreeMap.h"
#include "Machines/Apple2/Common/Dos33IntegrityScanner.h"
#include "Machines/Apple2/Common/Dos33VolumeRemover.h"
#include "Devices/Disk/ChainWalkGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Dos33Volume
//
////////////////////////////////////////////////////////////////////////////////

Dos33Volume::Dos33Volume (const vector<Byte> & sectors)
    : m_access (sectors)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Enumerate (VolumeListing & outListing) const
{
    HRESULT                    hr          = S_OK;
    size_t                     bufferBytes = m_access.GetBuffer().size();
    size_t                     imageBytes  = m_access.GetGeometry().GetImageByteSize();
    vector<Dos33CatalogEntry>  entries;
    bool                       fullyParsed = true;



    CBREx (bufferBytes == imageBytes, E_INVALIDARG);

    outListing = VolumeListing();

    outListing.volumeNumber    = m_access.ReadByte (Dos33Skeleton::kVtocTrack, 0,
                                                    Dos33Skeleton::kVtocOffVolumeNumber);
    outListing.hasVolumeNumber = true;
    outListing.totalUnits      = m_access.GetGeometry().GetUnitCount();

    m_catalog.CollectEntries (entries, outListing.damage, fullyParsed);

    for (const Dos33CatalogEntry & entry : entries)
    {
        FileEntry  listed;

        listed.catalogIndex = outListing.entries.size();
        listed.name         = entry.name;
        listed.type         = (Byte) (entry.typeByte & ~kLockedBit);
        listed.isLocked     = (entry.typeByte & kLockedBit) != 0;
        listed.sizeUnits    = entry.sectorCount;
        listed.isDirectory  = false;

        //  A binary's load address is in its first two bytes, not the catalog,
        //  so listing it costs a walk to the file's first data sector.
        if (listed.type == kTypeBinary)
        {
            ChainWalkGuard    guard (m_access.GetGeometry().GetUnitCount());
            vector<uint32_t>  units;
            vector<uint32_t>  listUnits;

            if (m_chain.CollectDataSectors (entry, units, listUnits, guard) && !units.empty())
            {
                listed.loadAddress    = (Word) (m_access.ReadByte (m_access.GetTrack (units[0]), m_access.GetSector (units[0]), 0)
                                              | (m_access.ReadByte (m_access.GetTrack (units[0]), m_access.GetSector (units[0]), 1) << 8));
                listed.hasLoadAddress = true;
            }
        }

        outListing.entries.push_back (listed);
    }

    outListing.freeUnits = m_freeMap.CountFreeUnits();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Read
//
//  DOS 3.3 has no subdirectories, so a path with more than one component names
//  something this filesystem cannot express. Refusing it is the honest answer;
//  quietly using the last component would open a different file than the caller
//  asked for.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT                    hr          = S_OK;
    size_t                     bufferBytes = m_access.GetBuffer().size();
    size_t                     imageBytes  = m_access.GetGeometry().GetImageByteSize();
    bool                       single      = path.IsSingleComponent();
    bool                       walked      = false;
    bool                       fullyParsed = true;
    uint16_t                   owner       = 0;
    vector<Dos33CatalogEntry>  entries;
    vector<uint32_t>           units;
    vector<std::string>        damage;



    CBREx (bufferBytes == imageBytes, E_INVALIDARG);
    CBREx (single, E_INVALIDARG);

    m_catalog.CollectEntries (entries, damage, fullyParsed);

    hr = Dos33Catalog::FindEntry (entries, path, owner);
    CHR (hr);

    {
        ChainWalkGuard    guard (m_access.GetGeometry().GetUnitCount());
        vector<uint32_t>  listUnits;

        walked = m_chain.CollectDataSectors (entries[owner], units, listUnits, guard);
    }

    CBREx (walked, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    outPayload           = FilePayload();
    outPayload.type      = (Byte) (entries[owner].typeByte & ~kLockedBit);
    outPayload.encoding  = PayloadEncoding::Verbatim;
    outPayload.access    = (entries[owner].typeByte & kLockedBit) != 0 ? FilePayload::kAccessLocked : FilePayload::kAccessUnlocked;
    outPayload.hasAccess = true;

    for (uint32_t unit : units)
    {
        int     track  = m_access.GetTrack (unit);
        int     sector = m_access.GetSector (unit);
        size_t  i      = 0;

        for (i = 0; i < (size_t) NibbleFormat::kSectorByteSize; i++)
        {
            outPayload.bytes.push_back (m_access.ReadByte (track, sector, i));
        }
    }

    Dos33PayloadCodec::ApplyTypeHeader (entries[owner].typeByte, outPayload.bytes, outPayload);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::BuildIntegrityReport
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    return m_scanner.BuildIntegrityReport (outReport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::AddFile
//
//  Allocate from the VTOC bitmap, build the track/sector lists, write the data
//  sectors, create the catalog entry, and leave the bitmap agreeing with what
//  was allocated.
//
//  Nothing is written into the volume this object holds. The complete new buffer
//  is produced instead, so a refusal at any point leaves the caller with exactly
//  what it had.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::AddFile (
    const vector<Byte>  & nameBytes,
    Byte                  typeByte,
    const vector<Byte>  & stored,
    vector<Byte>        & outBuffer) const
{
    HRESULT                hr         = S_OK;
    bool                   slotOk     = false;
    bool                   allocated  = false;
    int                    slotTrack  = 0;
    int                    slotSector = 0;
    size_t                 slotOffset = 0;
    size_t                 storedSize = stored.size();
    size_t                 dataCount  = 0;
    size_t                 listCount  = 0;
    vector<Byte>           result;
    vector<uint32_t>       units;
    VolumeIntegrityReport  report;



    slotOk = m_catalog.TryFindFreeSlot (slotTrack, slotSector, slotOffset);
    CBREx (slotOk, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    hr = m_scanner.BuildIntegrityReport (report);
    CHRA (hr);

    dataCount = (storedSize + (size_t) NibbleFormat::kSectorByteSize - 1)
              / (size_t) NibbleFormat::kSectorByteSize;
    listCount = (dataCount + (size_t) Dos33FileChain::kTsPairsPerList - 1) / (size_t) Dos33FileChain::kTsPairsPerList;

    // Even a file with no data occupies its track/sector list, which is what
    // makes it a file rather than a name.
    if (listCount == 0)
    {
        listCount = 1;
    }

    allocated = m_freeMap.TryAllocateUnits (report, dataCount + listCount, units);
    CBREx (allocated, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    result = m_access.GetBuffer();

    m_chain.ZeroUnits (result, units);
    m_chain.PlaceFile (result, units, dataCount, stored);

    for (uint32_t unit : units)
    {
        m_freeMap.SetFreeInBitmap (result, m_access.GetTrack (unit), m_access.GetSector (unit), false);
    }

    m_catalog.WriteEntry (result,
                          slotTrack,
                          slotSector,
                          slotOffset,
                          units[0],
                          typeByte,
                          (uint16_t) units.size(),
                          nameBytes);

    hr = Dos33IntegrityScanner::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Write
//
//  Adds a file, or replaces one of the same name.
//
//  REPLACEMENT IS COMPUTED AS ONE WHOLE. The removal is staged into a working
//  buffer that no caller can see, the new file is placed over that, and only a
//  finished buffer is ever handed back. What this must never be is a removal
//  APPLIED to the caller's image followed by a placement: a failure between the
//  two frees the old file without landing the new one, which loses the file
//  outright -- strictly worse than refusing.
//
//  The distinction is not merely internal ordering. Nothing here mutates the
//  volume's own buffer, so a refusal at any point, including one raised after
//  the staged removal, leaves the original image byte-for-byte as it was.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Write (
    const FilePath     & path,
    const FilePayload  & payload,
    vector<Byte>       & outBuffer) const
{
    HRESULT              hr          = S_OK;
    size_t               bufferBytes = m_access.GetBuffer().size();
    size_t               imageBytes  = m_access.GetGeometry().GetImageByteSize();
    bool                 single      = path.IsSingleComponent();
    bool                 nameOk      = false;
    bool                 exists      = false;
    bool                 ambiguous   = false;
    bool                 isLocked    = false;
    bool                 fullyParsed = true;
    Byte                 typeByte    = (Byte) ((payload.type & ~kLockedBit) | (payload.IsLocked() ? kLockedBit : 0));
    uint16_t             owner       = 0;
    vector<Byte>         nameBytes;
    vector<Byte>         stored;
    vector<Byte>         staged;
    //  stagedVolume binds to `staged` by reference, and is read only once that
    //  buffer has been filled.
    vector<Dos33CatalogEntry>  entries;
    vector<std::string>        damage;
    DeleteOutcome              removal;
    Dos33Volume          stagedVolume (staged);



    CBREx (bufferBytes == imageBytes, E_INVALIDARG);

    nameOk = single && Dos33Catalog::TryEncodeName (path.GetLeaf(), nameBytes);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    hr = Dos33PayloadCodec::ComposeStoredBytes (payload, stored);
    CHR (hr);

    m_catalog.CollectEntries (entries, damage, fullyParsed);

    //  Replacing one of several entries with a name would replace whichever
    //  comes first, so a shared name returns ERROR_DUP_NAME here too.
    hr        = Dos33Catalog::FindEntry (entries, path, owner);
    exists    = SUCCEEDED (hr);
    ambiguous = hr == HRESULT_FROM_WIN32 (ERROR_DUP_NAME);
    CBREx (!ambiguous, hr);

    hr = S_OK;

    if (exists)
    {
        isLocked = (entries[owner].typeByte & kLockedBit) != 0;
    }

    CBREx (!isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    if (!exists)
    {
        hr = AddFile (nameBytes, typeByte, stored, outBuffer);
        CHR (hr);
    }
    else
    {
        // The removal is answered for by the same rules a bare delete follows,
        // including its refusal to hand back a sector another entry claims. The
        // replacement simply does not get that space.
        hr = m_remover.Delete (path, staged, removal);
        CHRA (hr);

        hr = stagedVolume.AddFile (nameBytes, typeByte, stored, outBuffer);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    return m_remover.Delete (path, outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Delete (
    const FilePath  & path,
    vector<Byte>    & outBuffer,
    DeleteOutcome   & outOutcome) const
{
    return m_remover.Delete (path, outBuffer, outOutcome);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::HasDosImage
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::HasDosImage() const
{
    int     track  = 0;
    int     sector = 0;
    size_t  i      = 0;



    for (track = 0; track < Dos33Skeleton::kDosImageTracks; track++)
    {
        for (sector = 0; sector < m_access.GetGeometry().sectorsPerTrack; sector++)
        {
            for (i = 0; i < (size_t) NibbleFormat::kSectorByteSize; i++)
            {
                if (m_access.ReadByte (track, sector, i) != 0)
                {
                    return true;
                }
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::SetStartupProgram
//
//  DOS 3.3 keeps the name of the program it runs after booting inside its own
//  image on track 1, so setting it is a patch of thirty bytes and nothing else:
//  no catalog entry is added, moved or touched, and no file is written to chain
//  to another. A greeting that chained would cost a catalog slot and sectors,
//  change what CATALOG shows, and still not be how DOS does it.
//
//  THE NAME WRITTEN IS THE ONE THE CATALOG STORES, byte for byte, rather than
//  the one the caller spelled. DOS matches the greeting name against catalog
//  names on the disk, so a lower-case letter -- or one of the control characters
//  a real disk legitimately carries in a name -- would name a file DOS then
//  fails to find, with the disk booting to an error instead of to the program.
//
//  A VOLUME WITH NO DOS ON IT IS REFUSED. The tracks the image occupies are
//  reserved by the format whether or not anything was installed in them, so the
//  patch would land in space nothing reads and the command would report success
//  for a disk that still cannot boot at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    HRESULT                    hr          = S_OK;
    size_t                     bufferBytes = m_access.GetBuffer().size();
    size_t                     imageBytes  = m_access.GetGeometry().GetImageByteSize();
    bool                       single      = path.IsSingleComponent();
    bool                       sixteen     = m_access.GetGeometry().sectorsPerTrack == 16;
    bool                       hasDos      = false;
    bool                       fullyParsed = true;
    uint16_t                   owner       = 0;
    size_t                     i           = 0;
    vector<Dos33CatalogEntry>  entries;
    vector<std::string>        damage;
    vector<Byte>               storedName;
    vector<Byte>               result;
    VolumeIntegrityReport      report;



    CBREx (bufferBytes == imageBytes, E_INVALIDARG);
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    //  The greeting's place is DOS 3.3's. DOS 3.2's image is laid out
    //  differently.
    CBREx (sixteen, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    hasDos = HasDosImage();
    CBREx (hasDos, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    m_catalog.CollectEntries (entries, damage, fullyParsed);

    hr = Dos33Catalog::FindEntry (entries, path, owner);
    CHR (hr);

    hr = m_scanner.BuildIntegrityReport (report);
    CHRA (hr);

    result = m_access.GetBuffer();

    m_catalog.ReadStoredName (entries[owner], storedName);

    for (i = 0; i < storedName.size(); i++)
    {
        m_access.WriteByteAt (result, kGreetingTrack, kGreetingSector, kGreetingOffset + i, storedName[i]);
    }

    // The same self-check every other mutating call runs over its own output.
    // Nothing here moves a sector, so a difference would mean the patch landed
    // somewhere it had no business being.
    hr = Dos33IntegrityScanner::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::IsRunnableAsGreeting
//
//  Whether a booting DOS 3.3 would actually run this file.
//
//  Shared by the two routes that can set a startup program, so neither can come
//  to accept what the other refuses.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33Volume::IsRunnableAsGreeting (const VolumeListing & listing, const std::string & name)
{
    bool  runnable = true;



    for (const FileEntry & entry : listing.entries)
    {
        if (_stricmp (entry.name.c_str(), name.c_str()) == 0)
        {
            runnable = entry.type == Dos33Volume::kTypeApplesoft
                    || entry.type == Dos33Volume::kTypeInteger;
            break;
        }
    }

    return runnable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Rename
//
//  Thirty bytes of the catalog entry change and nothing else does: no sector
//  moves, the track/sector list stays where it was, and the free bitmap is
//  untouched. DOS's own RENAME refuses a locked file, and so does this.
//
//  A NAME ALREADY ON THE VOLUME IS REFUSED unless it is the file's own,
//  which lets a name be re-cased without a detour through a temporary.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Rename (
    const FilePath     & from,
    const std::string  & to,
    vector<Byte>       & outBuffer) const
{
    HRESULT                    hr          = S_OK;
    size_t                     bufferBytes = m_access.GetBuffer().size();
    size_t                     imageBytes  = m_access.GetGeometry().GetImageByteSize();
    bool                       single      = from.IsSingleComponent();
    bool                       nameOk      = false;
    bool                       taken       = false;
    bool                       isLocked    = false;
    bool                       fullyParsed = true;
    uint16_t                   owner       = 0;
    uint16_t                   holder      = 0;
    vector<Byte>               nameBytes;
    vector<Dos33CatalogEntry>  entries;
    vector<std::string>        damage;
    vector<Byte>               result;
    VolumeIntegrityReport      report;



    CBREx (bufferBytes == imageBytes, E_INVALIDARG);
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    nameOk = Dos33Catalog::TryEncodeName (to, nameBytes);
    CBREx (nameOk, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));

    m_catalog.CollectEntries (entries, damage, fullyParsed);

    hr = Dos33Catalog::FindEntry (entries, from, owner);
    CHR (hr);

    isLocked = (entries[owner].typeByte & kLockedBit) != 0;
    CBREx (!isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    taken = Dos33Catalog::TryFindEntry (entries, to, holder) && holder != owner;
    CBREx (!taken, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    hr = m_scanner.BuildIntegrityReport (report);
    CHRA (hr);

    result = m_access.GetBuffer();

    m_catalog.WriteName (result, entries[owner], nameBytes);

    // The same self-check every other mutating call runs over its own output.
    // A rename moves nothing, so a disagreement would mean the name landed
    // outside the entry it was meant for.
    hr = Dos33IntegrityScanner::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::SetAttributes
//
//  The type byte has both the type and the lock bit. A binary's load address
//  is the first two bytes of its first data sector, found through the first
//  pair of its track/sector list. DOS 3.3 doesn't store dates.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::SetAttributes (
    const FilePath        & path,
    const FileAttributes  & attributes,
    vector<Byte>          & outBuffer) const
{
    HRESULT                    hr          = S_OK;
    bool                       single      = path.IsSingleComponent();
    bool                       dated       = attributes.hasCreated || attributes.hasModified;
    bool                       whole       = m_access.GetBuffer().size() == m_access.GetGeometry().GetImageByteSize();
    bool                       listed      = false;
    bool                       inImage     = false;
    bool                       fullyParsed = true;
    bool                       locked      = false;
    uint16_t                   owner       = 0;
    Byte                       typeByte    = 0;
    size_t                     pair        = 0;
    size_t                     data        = 0;
    vector<Dos33CatalogEntry>  entries;
    vector<std::string>        damage;
    vector<Byte>               result;
    VolumeIntegrityReport      report;



    CBREx (whole, E_INVALIDARG);
    CBREx (single, HRESULT_FROM_WIN32 (ERROR_INVALID_NAME));
    CBREx (!dated, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));
    CBREx (!attributes.hasType || (attributes.type & kLockedBit) == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER));

    m_catalog.CollectEntries (entries, damage, fullyParsed);

    hr = Dos33Catalog::FindEntry (entries, path, owner);
    CHR (hr);

    typeByte = attributes.hasType ? (Byte) ((entries[owner].typeByte & kLockedBit) | attributes.type) : entries[owner].typeByte;
    locked   = attributes.hasAccess ? (attributes.access & FilePayload::kAccessWriteEnable) == 0 : (typeByte & kLockedBit) != 0;
    typeByte = locked ? (Byte) (typeByte | kLockedBit) : (Byte) (typeByte & ~kLockedBit);

    hr = m_scanner.BuildIntegrityReport (report);
    CHRA (hr);

    result = m_access.GetBuffer();

    m_catalog.WriteTypeByte (result, entries[owner], typeByte);

    //  Only a binary has a load address, and an empty one has no data sector for it.
    if (attributes.hasAuxType)
    {
        CBREx ((typeByte & ~kLockedBit) == kTypeBinary, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

        pair   = m_access.GetGeometry().GetSectorOffset (entries[owner].tsListTrack, entries[owner].tsListSector) + Dos33FileChain::kTsOffFirstPair;
        listed = pair + 1 < result.size() && result[pair] != 0;
        CBREx (listed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        data    = m_access.GetGeometry().GetSectorOffset (result[pair], result[pair + 1]);
        inImage = data + 1 < result.size();
        CBREx (inImage, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        result[data]     = (Byte) attributes.auxType;
        result[data + 1] = (Byte) (attributes.auxType >> 8);
    }

    hr = Dos33IntegrityScanner::HandBackVerifiedResult (report, result, outBuffer);
    CHRA (hr);

Error:
    return hr;
}
