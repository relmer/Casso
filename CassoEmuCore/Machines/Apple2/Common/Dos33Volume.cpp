#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33PayloadCodec.h"
#include "Machines/Apple2/Common/Dos33FileChain.h"
#include "Machines/Apple2/Common/Dos33FreeMap.h"
#include "Machines/Apple2/Common/Dos33IntegrityScanner.h"
#include "Machines/Apple2/Common/Dos33StartupProgram.h"
#include "Machines/Apple2/Common/Dos33VolumeRemover.h"
#include "Machines/Apple2/Common/Dos33VolumeWriter.h"
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
//  Dos33Volume::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Write (
    const FilePath     & path,
    const FilePayload  & payload,
    vector<Byte>       & outBuffer) const
{
    return m_writer.Write (path, payload, outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::Rename (
    const FilePath     & from,
    const std::string  & to,
    vector<Byte>       & outBuffer) const
{
    return m_writer.Rename (from, to, outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::SetAttributes (
    const FilePath        & path,
    const FileAttributes  & attributes,
    vector<Byte>          & outBuffer) const
{
    return m_writer.SetAttributes (path, attributes, outBuffer);
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
//  Dos33Volume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33Volume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    return m_startup.SetStartupProgram (path, outBuffer);
}





