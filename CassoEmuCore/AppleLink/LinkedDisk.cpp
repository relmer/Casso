#include "Pch.h"

#include "AppleLink/LinkedDisk.h"
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
    HRESULT              hr = S_OK;
    std::vector<size_t>  sectors;



    for (int sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
    {
        sectors.push_back ((size_t) (Dos33Skeleton::kVtocTrack * NibblizationLayer::kSectorsPerTrack + sector));
    }

    for (int block = ProDosSkeleton::kDirKeyBlock; block <= ProDosSkeleton::kBitmapBlock; block++)
    {
        sectors.push_back (ProDosSkeleton::GetBlockByteOffset (block, 0) / kSectorSize);
        sectors.push_back (ProDosSkeleton::GetBlockByteOffset (block, kSectorSize) / kSectorSize);
    }

    hr = Fetch (sectors);
    CHR (hr);

    m_kind = VolumeImage::DetectFilesystem (m_sectors);

Error:
    return hr;
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
//  The write's buffer is compared with the disk's sector by sector; a
//  sector the write left as it was is not sent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkedDisk::Write (const WriteOperation & operation, std::vector<size_t> & outSent)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  result;
    std::vector<Byte>  data;
    bool               sized  = false;



    outSent.clear();

    hr = RunRounds ([&] (const IVolume & volume) { return operation (volume, result); });
    CHR (hr);

    sized = result.size() == m_sectors.size();
    CBREx (sized, E_UNEXPECTED);

    for (size_t sector = 0; sector < kSectorCount; sector++)
    {
        auto  from = result.begin() + (ptrdiff_t) (sector * kSectorSize);

        if (!std::equal (from, from + (ptrdiff_t) kSectorSize, m_sectors.begin() + (ptrdiff_t) (sector * kSectorSize)))
        {
            outSent.push_back (sector);
            data.insert (data.end(), from, from + (ptrdiff_t) kSectorSize);
        }
    }

    if (!outSent.empty())
    {
        hr = m_source.WriteSectors (outSent, data);
        CHR (hr);
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

        if (m_kind == VolumeKind::Dos33)
        {
            Dos33Volume  volume (m_sectors);

            volume.SetReadObserver (this);
            result = operation (volume);
        }
        else
        {
            ProDosVolume  volume (m_sectors);

            volume.SetReadObserver (this);
            result = operation (volume);
        }

        BAIL_OUT_IF (m_misses.empty(), result);

        hr = Fetch (m_misses);
        CHR (hr);
    }

    CBREx (false, HRESULT_FROM_WIN32 (ERROR_RETRY));

Error:
    return hr;
}
