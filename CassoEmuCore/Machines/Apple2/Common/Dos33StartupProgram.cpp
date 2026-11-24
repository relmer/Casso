#include "Pch.h"

#include "Machines/Apple2/Common/Dos33StartupProgram.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Devices/Disk/FilePath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33StartupProgram::Dos33StartupProgram
//
////////////////////////////////////////////////////////////////////////////////

Dos33StartupProgram::Dos33StartupProgram (const Dos33SectorAccess & access)
    : m_access (access)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33StartupProgram::HasDosImage
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33StartupProgram::HasDosImage() const
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
//  Dos33StartupProgram::SetStartupProgram
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

HRESULT Dos33StartupProgram::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
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
//  Dos33StartupProgram::IsRunnableAsGreeting
//
//  Whether a booting DOS 3.3 would actually run this file.
//
//  Shared by the two routes that can set a startup program, so neither can come
//  to accept what the other refuses.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33StartupProgram::IsRunnableAsGreeting (const VolumeListing & listing, const std::string & name)
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





