#include "Pch.h"

#include "Machines/Apple2/Common/Dos33VolumeWriter.h"
#include "Machines/Apple2/Common/Dos33FileChain.h"
#include "Machines/Apple2/Common/Dos33FreeMap.h"
#include "Machines/Apple2/Common/Dos33PayloadCodec.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/Dos33VolumeInterfaces.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Devices/Disk/FilePath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeWriter::Dos33VolumeWriter
//
////////////////////////////////////////////////////////////////////////////////

Dos33VolumeWriter::Dos33VolumeWriter (const Dos33SectorAccess & access, const IDos33FileRemoval & removal)
    : m_access  (access),
      m_removal (removal)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::AddFile
//
//  Allocate from the VTOC bitmap, build the track/sector lists, write the data
//  sectors, create the catalog entry, and leave the bitmap agreeing with what
//  was allocated.
//
//  Nothing is written into `source`. The complete new buffer is produced
//  instead, so a refusal at any point leaves the caller with exactly
//  what it had.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33VolumeWriter::AddFile (
    const Dos33SectorAccess  & source,
    const vector<Byte>       & nameBytes,
    Byte                       typeByte,
    const vector<Byte>       & stored,
    vector<Byte>             & outBuffer) const
{
    HRESULT                hr         = S_OK;
    Dos33Catalog           catalog (source);
    Dos33FileChain         chain (source);
    Dos33FreeMap           freeMap (source);
    Dos33IntegrityScanner  scanner (source);
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



    slotOk = catalog.TryFindFreeSlot (slotTrack, slotSector, slotOffset);
    CBREx (slotOk, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    hr = scanner.BuildIntegrityReport (report);
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

    allocated = freeMap.TryAllocateUnits (report, dataCount + listCount, units);
    CBREx (allocated, HRESULT_FROM_WIN32 (ERROR_DISK_FULL));

    result = source.GetBuffer();

    chain.ZeroUnits (result, units);
    chain.PlaceFile (result, units, dataCount, stored);

    for (uint32_t unit : units)
    {
        freeMap.SetFreeInBitmap (result, source.GetTrack (unit), source.GetSector (unit), false);
    }

    catalog.WriteEntry (result,
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
//  Dos33VolumeWriter::Write
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

HRESULT Dos33VolumeWriter::Write (
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
    Byte                 typeByte    = (Byte) ((payload.type & ~Dos33Volume::kLockedBit) | (payload.IsLocked() ? Dos33Volume::kLockedBit : 0));
    uint16_t             owner       = 0;
    vector<Byte>         nameBytes;
    vector<Byte>         stored;
    vector<Byte>         staged;
    //  stagedAccess binds to `staged` by reference, and is read only once that
    //  buffer has been filled.
    vector<Dos33CatalogEntry>  entries;
    vector<std::string>        damage;
    DeleteOutcome              removal;
    Dos33SectorAccess          stagedAccess (staged);



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
        isLocked = (entries[owner].typeByte & Dos33Volume::kLockedBit) != 0;
    }

    CBREx (!isLocked, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    if (!exists)
    {
        hr = AddFile (m_access, nameBytes, typeByte, stored, outBuffer);
        CHR (hr);
    }
    else
    {
        // The removal is answered for by the same rules a bare delete follows,
        // including its refusal to hand back a sector another entry claims. The
        // replacement simply does not get that space.
        hr = m_removal.Delete (path, staged, removal);
        CHRA (hr);

        hr = AddFile (stagedAccess, nameBytes, typeByte, stored, outBuffer);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeWriter::Rename
//
//  Thirty bytes of the catalog entry change and nothing else does: no sector
//  moves, the track/sector list stays where it was, and the free bitmap is
//  untouched. DOS's own RENAME refuses a locked file, and so does this.
//
//  A NAME ALREADY ON THE VOLUME IS REFUSED unless it is the file's own,
//  which lets a name be re-cased without a detour through a temporary.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33VolumeWriter::Rename (
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

    isLocked = (entries[owner].typeByte & Dos33Volume::kLockedBit) != 0;
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
//  Dos33VolumeWriter::SetAttributes
//
//  The type byte has both the type and the lock bit. A binary's load address
//  is the first two bytes of its first data sector, found through the first
//  pair of its track/sector list. DOS 3.3 doesn't store dates.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33VolumeWriter::SetAttributes (
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
    CBREx (!attributes.hasType || (attributes.type & Dos33Volume::kLockedBit) == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER));

    m_catalog.CollectEntries (entries, damage, fullyParsed);

    hr = Dos33Catalog::FindEntry (entries, path, owner);
    CHR (hr);

    typeByte = attributes.hasType ? (Byte) ((entries[owner].typeByte & Dos33Volume::kLockedBit) | attributes.type) : entries[owner].typeByte;
    locked   = attributes.hasAccess ? (attributes.access & FilePayload::kAccessWriteEnable) == 0 : (typeByte & Dos33Volume::kLockedBit) != 0;
    typeByte = locked ? (Byte) (typeByte | Dos33Volume::kLockedBit) : (Byte) (typeByte & ~Dos33Volume::kLockedBit);

    hr = m_scanner.BuildIntegrityReport (report);
    CHRA (hr);

    result = m_access.GetBuffer();

    m_catalog.WriteTypeByte (result, entries[owner], typeByte);

    //  Only a binary has a load address, and an empty one has no data sector for it.
    if (attributes.hasAuxType)
    {
        CBREx ((typeByte & ~Dos33Volume::kLockedBit) == Dos33Volume::kTypeBinary, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

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





