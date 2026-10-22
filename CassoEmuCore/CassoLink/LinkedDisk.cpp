#include "Pch.h"

#include "CassoLink/LinkedDisk.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::LinkedDisk
//
////////////////////////////////////////////////////////////////////////////////

LinkedDisk::LinkedDisk (ILinkedSectorSource & source) :
    m_source  (source),
    m_sectors (kSectorCount * kSectorSize, 0),
    m_present (kSectorCount, false)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::Clear
//
////////////////////////////////////////////////////////////////////////////////

void LinkedDisk::Clear()
{
    std::fill (m_sectors.begin(), m_sectors.end(), (Byte) 0);
    std::fill (m_present.begin(), m_present.end(), false);

    m_misses.clear();
    m_kind   = VolumeKind::Unknown;
    m_rounds = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::Prefetch
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkedDisk::Prefetch()
{
    HRESULT              hr      = S_OK;
    std::vector<size_t>  sectors = GetCatalogSectors (VolumeKind::Dos33);
    std::vector<size_t>  prodos  = GetCatalogSectors (VolumeKind::ProDos);



    sectors.insert (sectors.end(), prodos.begin(), prodos.end());

    hr = Fetch (sectors);
    CHR (hr);

    m_kind = VolumeImage::DetectFilesystem (m_sectors);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::GetCatalogSectors
//
////////////////////////////////////////////////////////////////////////////////

std::vector<size_t> LinkedDisk::GetCatalogSectors (VolumeKind kind)
{
    std::vector<size_t>  sectors;



    if (kind == VolumeKind::Dos33)
    {
        for (int sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
        {
            sectors.push_back ((size_t) (Dos33Skeleton::kVtocTrack * NibblizationLayer::kSectorsPerTrack + sector));
        }
    }
    else if (kind == VolumeKind::ProDos)
    {
        for (int block = ProDosSkeleton::kDirKeyBlock; block <= ProDosSkeleton::kBitmapBlock; block++)
        {
            sectors.push_back (ProDosSkeleton::GetBlockByteOffset (block, 0) / kSectorSize);
            sectors.push_back (ProDosSkeleton::GetBlockByteOffset (block, kSectorSize) / kSectorSize);
        }
    }

    return sectors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::Run
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkedDisk::Run (const ReadOperation & operation)
{
    return RunRounds (operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::Write
//
//  A sector that was read is sent when the write changed it; one that was
//  not is sent when the write filled it (see FindWritten). They go in three
//  passes: the sectors not read, which are new data and new index or
//  directory blocks; then the other changed sectors; then the catalog's.
//  After a write that fails partway, no catalog entry, free map or directory
//  on the disk points at data never written.
//
//  A failed send clears the buffer: the sectors already written differ from
//  it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkedDisk::Write (const WriteOperation & operation, std::vector<size_t> & outSent)
{
    HRESULT              hr      = S_OK;
    std::vector<Byte>    result;
    std::vector<bool>    written;
    std::vector<size_t>  catalog;



    outSent.clear();

    hr = FindWritten (operation, result, written);
    CHR (hr);

    catalog = GetCatalogSectors (m_kind);

    for (int pass = 0; pass < 3; pass++)
    {
        std::vector<size_t>  part;
        std::vector<Byte>    data;

        for (size_t sector = 0; sector < kSectorCount; sector++)
        {
            auto  from      = result.begin() + (ptrdiff_t) (sector * kSectorSize);
            bool  isCatalog = std::find (catalog.begin(), catalog.end(), sector) != catalog.end();
            int   group     = isCatalog ? 2 : (m_present[sector] ? 1 : 0);
            bool  changed   = m_present[sector]
                            ? !std::equal (from, from + (ptrdiff_t) kSectorSize, m_sectors.begin() + (ptrdiff_t) (sector * kSectorSize))
                            : written[sector];

            if (changed && group == pass)
            {
                part.push_back (sector);
                data.insert (data.end(), from, from + (ptrdiff_t) kSectorSize);
            }
        }

        if (!part.empty())
        {
            hr = m_source.WriteSectors (part, data);
            CHRF (hr, Clear());
        }

        outSent.insert (outSent.end(), part.begin(), part.end());
    }

    m_sectors = std::move (result);

    for (size_t sector : outSent)
    {
        m_present[sector] = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::FindWritten
//
//  Runs the write until it reads nothing absent, then once more over a copy
//  whose absent sectors are kProbeFill rather than zeros. An absent sector
//  that comes out the same from both runs is written; one that comes out as
//  each run's own fill is untouched. One that is neither was read in part
//  after all, so it is fetched and the write runs again, as is anything the
//  second run read that the first did not.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkedDisk::FindWritten (const WriteOperation & operation, std::vector<Byte> & outResult, std::vector<bool> & outWritten)
{
    HRESULT              hr     = S_OK;
    std::vector<Byte>    probe;
    std::vector<Byte>    probed;
    std::vector<size_t>  partly;
    bool                 sized  = false;
    int                  round  = 0;



    for (round = 0; round < kMaxRounds; round++)
    {
        hr = RunRounds ([&] (const IVolume & volume) { return operation (volume, outResult); });
        CHR (hr);

        sized = outResult.size() == m_sectors.size();
        CBREx (sized, E_UNEXPECTED);

        probe = m_sectors;

        for (size_t sector = 0; sector < kSectorCount; sector++)
        {
            if (!m_present[sector])
            {
                std::fill_n (probe.begin() + (ptrdiff_t) (sector * kSectorSize), kSectorSize, kProbeFill);
            }
        }

        m_misses.clear();
        hr = RunOn (probe, [&] (const IVolume & volume) { return operation (volume, probed); });
        CHR (hr);

        sized = probed.size() == m_sectors.size();
        CBREx (sized, E_UNEXPECTED);

        partly = m_misses;
        outWritten.assign (kSectorCount, false);

        for (size_t sector = 0; sector < kSectorCount; sector++)
        {
            auto  first   = outResult.begin() + (ptrdiff_t) (sector * kSectorSize);
            auto  second  = probed.begin()    + (ptrdiff_t) (sector * kSectorSize);
            bool  same    = std::equal (first, first + (ptrdiff_t) kSectorSize, second);
            bool  zeros   = std::all_of (first,  first  + (ptrdiff_t) kSectorSize, [] (Byte b) { return b == 0; });
            bool  filled  = std::all_of (second, second + (ptrdiff_t) kSectorSize, [] (Byte b) { return b == kProbeFill; });

            if (!m_present[sector] && same)
            {
                outWritten[sector] = true;
            }
            else if (!m_present[sector] && !(zeros && filled))
            {
                partly.push_back (sector);
            }
        }

        BAIL_OUT_IF (partly.empty(), S_OK);

        hr = Fetch (partly);
        CHR (hr);
    }

    CBREx (false, HRESULT_FROM_WIN32 (ERROR_RETRY));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::Fetch
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkedDisk::Fetch (std::span<const size_t> sectors)
{
    HRESULT              hr     = S_OK;
    std::vector<size_t>  absent;
    std::vector<Byte>    data;
    bool                 whole  = false;



    for (size_t sector : sectors)
    {
        if (sector < kSectorCount && !m_present[sector] && std::find (absent.begin(), absent.end(), sector) == absent.end())
        {
            absent.push_back (sector);
        }
    }

    BAIL_OUT_IF (absent.empty(), S_OK);

    hr = m_source.ReadSectors (absent, data);
    CHR (hr);

    whole = data.size() == absent.size() * kSectorSize;
    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (size_t i = 0; i < absent.size(); i++)
    {
        std::copy_n (data.begin() + (ptrdiff_t) (i * kSectorSize), kSectorSize, m_sectors.begin() + (ptrdiff_t) (absent[i] * kSectorSize));
        m_present[absent[i]] = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::OnSectorRead
//
////////////////////////////////////////////////////////////////////////////////

void LinkedDisk::OnSectorRead (size_t sector)
{
    if (sector < kSectorCount && !m_present[sector] && std::find (m_misses.begin(), m_misses.end(), sector) == m_misses.end())
    {
        m_misses.push_back (sector);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::RunRounds
//
//  A run with misses read zeros where the missing sectors were, so its
//  result is discarded along with anything it produced; only the last run,
//  which read nothing absent, counts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkedDisk::RunRounds (const std::function<HRESULT (const IVolume & volume)> & operation)
{
    HRESULT  hr     = S_OK;
    HRESULT  result = S_OK;
    bool     known  = false;



    m_rounds = 0;

    if (m_kind == VolumeKind::Unknown)
    {
        hr = Prefetch();
        CHR (hr);
    }

    known = m_kind != VolumeKind::Unknown;
    CBREx (known, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

    for (m_rounds = 1; m_rounds <= kMaxRounds; m_rounds++)
    {
        m_misses.clear();

        result = RunOn (m_sectors, operation);

        BAIL_OUT_IF (m_misses.empty(), result);

        hr = Fetch (m_misses);
        CHR (hr);
    }

    CBREx (false, HRESULT_FROM_WIN32 (ERROR_RETRY));

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  LinkedDisk::RunOn
//
//  The operation's result, over a volume of the disk's kind on `sectors`.
//  A read of a sector not here is added to the miss list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkedDisk::RunOn (const std::vector<Byte> & sectors, const std::function<HRESULT (const IVolume & volume)> & operation)
{
    HRESULT  hr = S_OK;



    if (m_kind == VolumeKind::Dos33)
    {
        Dos33Volume  volume (sectors);

        volume.SetReadObserver (this);
        hr = operation (volume);
    }
    else
    {
        ProDosVolume  volume (sectors);

        volume.SetReadObserver (this);
        hr = operation (volume);
    }

    return hr;
}
