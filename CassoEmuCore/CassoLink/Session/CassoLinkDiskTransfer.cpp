#include "Pch.h"

#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "CassoLink/CassoLinkSession.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DurableCommit.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::CassoLinkDiskTransfer
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkDiskTransfer::CassoLinkDiskTransfer (CassoLinkSession & session)
    : m_session (session)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::ReadWholeDisk
//
//  The image is written only once every track has been read, so a failed or
//  canceled read writes nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkDiskTransfer::ReadWholeDisk (LinkOperation & operation)
{
    HRESULT               hr       = S_OK;
    DiskFormat            format   = DiskFormat::Dsk;
    std::vector<Byte>     sectors (NibblizationLayer::kImageByteSize, 0);
    std::vector<Byte>     bytes;
    CommitPlan::Progress  progress;
    bool                  complete = false;



    hr = DiskImageStore::GetSourceFormatByExtension (operation.imagePath, format);
    CHR (hr);

    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        m_session.Report (operation, track * 100 / NibblizationLayer::kTrackCount);
        m_session.m_host->SetProgress ((Byte) (track * 100 / NibblizationLayer::kTrackCount), (Byte) ((track + 1) * 100 / NibblizationLayer::kTrackCount));

        hr = ReadTrack (operation.drive, track, sectors, operation.unreadable);
        CHR (hr);
    }

    complete = operation.unreadable.empty() || operation.blankUnreadable;

    if (!complete)
    {
        operation.message = std::format ("{} sectors could not be read, the first on track {}, sector {}.",
                                         operation.unreadable.size(), operation.unreadable[0].first, operation.unreadable[0].second);
    }

    CBREx (complete, HRESULT_FROM_WIN32 (ERROR_CRC));

    hr = BlankDiskBuilder::WrapInContainer (format, NibbleImageCodec::kNibTrackSize, false, 0, sectors, bytes);
    CHR (hr);

    hr = DurableCommit::Commit (m_session.m_fileIo, operation.imagePath, bytes, m_commitTag++, CommitMode::Replace, progress);
    CHR (hr);

    hr = SaveUnreadableList (operation);
    CHRF (hr, operation.message = std::format ("The image was written, but the list of its unreadable sectors could not be saved as {}.",
                                               GetUnreadableListPath (operation.imagePath)));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::SaveUnreadableList
//
//  A blank sector in an image is indistinguishable from one read as zeros, so
//  the sectors written as zeros are listed beside the image. A read with every
//  sector removes a list from an earlier read of the same image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkDiskTransfer::SaveUnreadableList (const LinkOperation & operation)
{
    HRESULT               hr    = S_OK;
    std::string           path  = GetUnreadableListPath (operation.imagePath);
    std::string           text;
    CommitPlan::Progress  progress;
    bool                  stale = false;



    if (operation.unreadable.empty())
    {
        stale = m_session.m_fileIo.Exists (path);

        if (stale)
        {
            hr = m_session.m_fileIo.Remove (path);
            CHR (hr);
        }
    }
    else
    {
        text = FormatUnreadableList (operation.unreadable);

        hr = DurableCommit::Commit (m_session.m_fileIo, path, std::vector<Byte> (text.begin(), text.end()), m_commitTag++, CommitMode::Replace, progress);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::GetUnreadableListPath
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkDiskTransfer::GetUnreadableListPath (const std::string & imagePath)
{
    return imagePath + ".unreadable.txt";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::FormatUnreadableList
//
//  DOS 3.3 sector numbers whatever the image's format, as a .dsk stores its
//  sectors in that order and the link reads them by it.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkDiskTransfer::FormatUnreadableList (const std::vector<std::pair<int, int>> & unreadable)
{
    std::string  text = "# Sectors that could not be read when this image was made, written as zeros.\r\n"
                        "# Each line is a sector's track and DOS 3.3 sector, in decimal.\r\n";



    for (const auto & [track, sector] : unreadable)
    {
        text += std::format ("{} {}\r\n", track, sector);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::ReadBootSector
//
//  Track 0's first sector against a link boot disk's: the universal disk's,
//  or the disk made for the link's chip. That sector is the loader, the same
//  at every speed and setting, and different only where the payload loads.
//  A drive whose track 0 cannot be read has no link disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkDiskTransfer::ReadBootSector (LinkOperation & operation)
{
    HRESULT                           hr       = S_OK;
    std::vector<Byte>                 sectors (NibblizationLayer::kImageByteSize, 0);
    std::vector<Byte>                 boot;
    std::vector<Byte>                 universal;
    std::vector<std::pair<int, int>>  unreadable;
    LinkShimOptions                   options;
    bool                              loader   = false;
    bool                              matches  = false;



    options.chip = CassoLinkSession::GetLinkChip (m_session.m_settings.transport);
    options.baud = m_session.m_settings.baud;

    hr = LinkProgram::BuildBootDisk (options, boot);
    CHR (hr);

    hr = LinkProgram::BuildUniversalBootDisk (options, universal);
    CHR (hr);

    hr = ReadTrack (operation.drive, 0, sectors, unreadable);
    CHR (hr);

    matches = std::equal (boot.begin(),      boot.begin()      + NibblizationLayer::kSectorByteSize, sectors.begin())
           || std::equal (universal.begin(), universal.begin() + NibblizationLayer::kSectorByteSize, sectors.begin());
    loader  = matches && std::none_of (unreadable.begin(), unreadable.end(), [] (const std::pair<int, int> & sector) { return sector.second == 0; });

    operation.linkDisk = loader;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::ReadTrack
//
//  A track's sixteen sectors in one request. When the drive cannot read one
//  of them, each is read on its own, and those that fail again are listed
//  and written as zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkDiskTransfer::ReadTrack (int drive, int track, std::vector<Byte> & sectors, std::vector<std::pair<int, int>> & unreadable)
{
    HRESULT            hr        = S_OK;
    HRESULT            disk      = S_OK;
    std::vector<Byte>  physicals;
    std::vector<Byte>  data;
    CassoLinkStatus    status;
    bool               damaged   = false;
    int                missing   = 0;



    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    hr = m_session.m_host->ReadSectors ((Byte) drive, (Byte) track, physicals, data, status);
    CHR (hr);

    disk    = CassoLinkSession::FromStatusError (status.error);
    damaged = status.error == CassoLinkHost::kErrorNotFound || status.error == CassoLinkHost::kErrorData;
    missing = status.done;

    BAIL_OUT_IF (FAILED (disk) && !damaged, disk);

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        int     logical = CassoLinkSession::GetLogicalSector (physical);
        size_t  to      = (size_t) (track * NibblizationLayer::kSectorsPerTrack + logical) * LinkedDisk::kSectorSize;
        size_t  from    = (size_t) physical * LinkedDisk::kSectorSize;

        if (damaged && physical >= missing)
        {
            const Byte  one[] = { (Byte) physical };

            hr = m_session.m_host->ReadSectors ((Byte) drive, (Byte) track, one, data, status);
            CHR (hr);

            from = 0;

            if (status.error != CassoLinkHost::kErrorNone)
            {
                unreadable.push_back ({ track, logical });
                continue;
            }
        }

        std::copy_n (data.begin() + (ptrdiff_t) from, LinkedDisk::kSectorSize, sectors.begin() + (ptrdiff_t) to);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::WriteWholeDisk
//
//  The image is decoded first, and nothing is written if any track is not
//  standard sectors. The volume number formatted into each address field
//  is the image's own for DOS 3.3, and 254 otherwise. The drive's sectors
//  and listing are then dropped, as the disk is another one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkDiskTransfer::WriteWholeDisk (LinkOperation & operation)
{
    HRESULT             hr       = S_OK;
    size_t              index    = (size_t) operation.drive - 1;
    std::vector<Byte>   fileBytes;
    std::vector<Byte>   sectors;
    std::vector<Byte>   data;
    std::vector<int>    badTracks;
    std::string         list;
    SectorDecodeReport  report;
    VolumeListing       listing;
    CassoLinkStatus     status;
    Byte                volume   = 254;
    Byte                gap      = 0;
    bool                standard = false;
    HRESULT             listed   = S_OK;



    hr = m_session.m_fileIo.ReadAllBytes (operation.imagePath, fileBytes);
    CHR (hr);

    hr = VolumeImage::Load (fileBytes, operation.imagePath, sectors, report);
    CHR (hr);

    for (int track = 0; track < report.GetTrackCount(); track++)
    {
        if (report.GetOutcome (track) != TrackDecodeOutcome::Complete)
        {
            badTracks.push_back (track);
            list += std::format ("{}{}", list.empty() ? "" : ", ", track);
        }
    }

    if (!badTracks.empty())
    {
        operation.message = std::format ("Tracks {} of the image are not standard sectors, so nothing was written.", list);
    }

    standard = badTracks.empty();
    CBREx (standard, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33)
    {
        listed = Dos33Volume (sectors).Enumerate (listing);

        if (SUCCEEDED (listed) && listing.hasVolumeNumber)
        {
            volume = listing.volumeNumber;
        }
    }

    m_session.m_disks[index]->Clear();
    m_session.m_listings[index].reset();

    hr = TimeDriveForFormat (operation.drive, gap);
    CHR (hr);

    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        m_session.Report (operation, track * 100 / NibblizationLayer::kTrackCount);
        m_session.m_host->SetProgress ((Byte) (track * 100 / NibblizationLayer::kTrackCount), (Byte) ((track + 1) * 100 / NibblizationLayer::kTrackCount));

        data.clear();

        for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
        {
            auto  from = sectors.begin() + (ptrdiff_t) ((size_t) (track * NibblizationLayer::kSectorsPerTrack + CassoLinkSession::GetLogicalSector (physical)) * LinkedDisk::kSectorSize);

            data.insert (data.end(), from, from + (ptrdiff_t) LinkedDisk::kSectorSize);
        }

        hr = m_session.m_host->WriteTrack ((Byte) operation.drive, (Byte) track, volume, data, status, gap);
        CHR (hr);

        hr = CassoLinkSession::FromStatusError (status.error);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::TimeDriveForFormat
//
//  The drive's revolution timed on track 0, and the gap after each sector
//  that fits 16 sectors in it. A track 0 with no sector 0, as on a blank
//  disk, is formatted first with the program's own gap, which fits a drive
//  up to about 4% fast. A drive that cannot be timed gets the program's gap.
//
//  Only a real Apple's drive is timed: the PC times the revolution by its
//  own clock, and an emulated Apple does not always run in real time, so its
//  milliseconds are not its cycles. An emulated drive turns at 300 rpm.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkDiskTransfer::TimeDriveForFormat (int drive, Byte & outGap)
{
    HRESULT          hr          = S_OK;
    CassoLinkStatus  status;
    double           revolution  = 0.0;



    outGap = 0;

    BAIL_OUT_IF (m_session.m_settings.transport != LinkTransport::Serial && m_session.m_settings.transport != LinkTransport::WifiModem, S_OK);

    //  A drive's speed is its own, not its disk's: timed once a connection.
    revolution = m_revolutionMs[(size_t) drive - 1];
    BAIL_OUT_IF (revolution > 0.0, S_OK);

    hr = m_session.m_host->TimeDrive ((Byte) drive, 0, kTimedRevolutions, revolution, status);
    CHR (hr);

    if (revolution == 0.0)
    {
        hr = m_session.m_host->FormatTrack ((Byte) drive, 0, 254, status);
        CHR (hr);

        hr = m_session.m_host->TimeDrive ((Byte) drive, 0, kTimedRevolutions, revolution, status);
        CHR (hr);
    }

    BAIL_OUT_IF (revolution == 0.0, S_OK);

    m_revolutionMs[(size_t) drive - 1] = revolution;
    m_session.m_sink.OnActivity (DescribeDriveSpeed (drive, revolution));

Error:
    outGap = (revolution > 0.0) ? CassoLinkHost::ComputeFormatGap (revolution) : 0;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDiskTransfer::DescribeDriveSpeed
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkDiskTransfer::DescribeDriveSpeed (int drive, double revolutionMs)
{
    double       rpm     = 60000.0 / revolutionMs;
    double       percent = (rpm - 300.0) / 3.0;
    std::string  text    = std::format ("D{} turns at {:.1f} rpm", drive, rpm);



    if (std::abs (percent) >= 0.05)
    {
        text += std::format (", {:.1f}% {}", std::abs (percent), percent < 0.0 ? "slow" : "fast");
    }

    if (std::abs (percent) > 1.5)
    {
        text += "; drives are usually set within 1.5% of 300 rpm";
    }

    return text;
}
