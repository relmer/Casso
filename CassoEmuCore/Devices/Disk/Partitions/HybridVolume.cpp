#include "Pch.h"

#include "Devices/Disk/Partitions/HybridVolume.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





static constexpr size_t  s_kSectorBytes = Dos33Geometry::kSectorByteSize;





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::HybridVolume
//
////////////////////////////////////////////////////////////////////////////////

HybridVolume::HybridVolume (const vector<Byte> & sectors, std::unique_ptr<IVolume> volume, VolumeKind other) :
    m_sectors (sectors),
    m_volume  (std::move (volume)),
    m_other   (other)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::Enumerate (VolumeListing & outListing) const
{
    return m_volume->Enumerate (outListing);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::EnumerateDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::EnumerateDirectory (const FilePath & directory, VolumeListing & outListing) const
{
    return m_volume->EnumerateDirectory (directory, outListing);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    return m_volume->Read (path, outPayload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::CreateDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::CreateDirectory (const FilePath & path, vector<Byte> & outBuffer) const
{
    return CheckResult (m_volume->CreateDirectory (path, outBuffer), outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::BuildRemovalPlan
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::BuildRemovalPlan (const FilePath & path, DirectoryRemovalPlan & outPlan) const
{
    return m_volume->BuildRemovalPlan (path, outPlan);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::RemoveDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::RemoveDirectory (const FilePath & path, bool force, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    return CheckResult (m_volume->RemoveDirectory (path, force, outBuffer, outOutcome), outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    return CheckResult (m_volume->Write (path, payload, outBuffer), outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    return CheckResult (m_volume->Delete (path, outBuffer), outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    return CheckResult (m_volume->Delete (path, outBuffer, outOutcome), outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::BuildIntegrityReport
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    return m_volume->BuildIntegrityReport (outReport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    return CheckResult (m_volume->SetStartupProgram (path, outBuffer), outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    return CheckResult (m_volume->Rename (from, to, outBuffer), outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    return CheckResult (m_volume->SetAttributes (path, attributes, outBuffer), outBuffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::CheckResult
//
//  The result of a write that succeeded, checked. The buffer of a write that
//  fails the check is emptied.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::CheckResult (HRESULT hrWrite, vector<Byte> & ioBuffer) const
{
    HRESULT  hr = hrWrite;



    if (SUCCEEDED (hr))
    {
        hr = CheckWrite (m_sectors, ioBuffer, m_other);

        if (FAILED (hr))
        {
            ioBuffer.clear();
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::CheckWrite
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::CheckWrite (const vector<Byte> & before, const vector<Byte> & edited, VolumeKind other)
{
    HRESULT            hr         = S_OK;
    std::vector<bool>  marked (before.size() / s_kSectorBytes, false);
    size_t             sector     = 0;
    size_t             offset     = 0;
    bool               isSameSize = edited.size() == before.size();
    bool               isSame     = true;



    CBREx (isSameSize, E_UNEXPECTED);

    hr = MarkSectors (before, other, marked);
    CHR (hr);

    for (sector = 0; sector < marked.size(); sector++)
    {
        offset = sector * s_kSectorBytes;
        isSame = !marked[sector] || std::equal (before.begin() + (ptrdiff_t) offset, before.begin() + (ptrdiff_t) (offset + s_kSectorBytes), edited.begin() + (ptrdiff_t) offset);

        CBREx (isSame, E_UNEXPECTED);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::MarkSectors
//
//  Marks the sectors of the buffer that the `other` file system's files and
//  structures are in. For ProDOS and Pascal those are the blocks its report
//  lists as in use by a file or structure. For DOS 3.3 they are the sectors
//  of its files, its VTOC and its catalog; the rest of tracks 0 to 2 and 17
//  is not marked, because the block volume's boot blocks and directory are on
//  track 0 and the spare sectors of track 17 may be the block volume's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HybridVolume::MarkSectors (const vector<Byte> & sectors, VolumeKind other, std::vector<bool> & ioMarked)
{
    HRESULT                   hr      = S_OK;
    std::unique_ptr<IVolume>  volume;
    VolumeIntegrityReport     report;
    uint32_t                  unit    = 0;
    bool                      isDos   = other == VolumeKind::Dos33;
    bool                      isFound = false;
    bool                      isFile  = false;



    switch (other)
    {
        case VolumeKind::Dos33:  volume = std::make_unique<Dos33Volume>  (sectors); break;
        case VolumeKind::ProDos: volume = std::make_unique<ProDosVolume> (sectors); break;
        case VolumeKind::Pascal: volume = std::make_unique<PascalVolume> (sectors); break;
        default:                 break;
    }

    isFound = volume != nullptr;
    CBREx (isFound, E_UNEXPECTED);

    hr = volume->BuildIntegrityReport (report);
    CHR (hr);

    for (unit = 0; unit < report.GetUnitCount(); unit++)
    {
        if (!isDos)
        {
            if (report.IsClaimed (unit))
            {
                MarkBlock (sectors, unit, ioMarked);
            }

            continue;
        }

        isFile = false;

        for (uint16_t owner : report.GetClaimantsOf (unit))
        {
            isFile = isFile || owner != VolumeIntegrityReport::kVolumeOwner;
        }

        if (isFile && unit < ioMarked.size())
        {
            ioMarked[unit] = true;
        }
    }

    if (isDos)
    {
        MarkDosCatalog (sectors, ioMarked);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::MarkDosCatalog
//
//  The VTOC and each catalog sector linked from it.
//
////////////////////////////////////////////////////////////////////////////////

void HybridVolume::MarkDosCatalog (const vector<Byte> & sectors, std::vector<bool> & ioMarked)
{
    Dos33Geometry  geometry;
    size_t         offset   = 0;
    uint32_t       walked   = 0;
    int            track    = Dos33Geometry::kVtocTrack;
    int            sector   = 0;
    bool           isLinked = true;



    while (isLinked && walked < geometry.GetUnitCount())
    {
        offset = geometry.GetSectorOffset (track, sector);

        if (offset + s_kSectorBytes > sectors.size())
        {
            break;
        }

        ioMarked[offset / s_kSectorBytes] = true;

        track    = sectors[offset + Dos33Geometry::kVtocOffCatalogTrack];
        sector   = sectors[offset + Dos33Geometry::kVtocOffCatalogSector];
        isLinked = track != 0 && track < geometry.tracks && sector < geometry.sectorsPerTrack;
        walked++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume::MarkBlock
//
//  Both sectors of a block.
//
////////////////////////////////////////////////////////////////////////////////

void HybridVolume::MarkBlock (const vector<Byte> & sectors, uint32_t block, std::vector<bool> & ioMarked)
{
    size_t  half   = 0;
    size_t  offset = 0;



    for (half = 0; half < (size_t) ProDosSkeleton::kBlockByteSize; half += s_kSectorBytes)
    {
        offset = ProDosSkeleton::GetBlockByteOffset (sectors.size(), (int) block, half);

        if (offset / s_kSectorBytes < ioMarked.size())
        {
            ioMarked[offset / s_kSectorBytes] = true;
        }
    }
}
