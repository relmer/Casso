#include "Pch.h"

#include "CassoLink/FullSend/FullSendSession.h"

#include "CassoLink/FullSend/FullSendDisk.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::CheckDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::CheckDrive (int drive, CheckedDisk & outDisk)
{
    HRESULT                       hr         = S_OK;
    bool                          present    = m_link.HasDrive (drive);
    bool                          stable     = false;
    bool                          swapped    = false;
    bool                          known      = false;
    std::unique_lock<std::mutex>  folderLock = std::unique_lock<std::mutex> (GetFolderLock (m_folder), std::defer_lock);



    outDisk = {};
    m_repeats[(size_t) drive - 1] = false;
    m_swapped[(size_t) drive - 1] = false;

    BAIL_OUT_IF (!present, S_OK);

    m_state = State::Checking;
    SetDriveState (drive, FullSendDriveState::Checking);
    m_status.SetActivity (FullSendActivity::Checking, drive);
    m_status.ClearDisk (drive, FullSendDiskState::Checking);

    hr = m_link.ShowScreen (m_screen);
    CHR (hr);

    hr = ReadFingerprint (drive, outDisk, stable);
    CHR (hr);

    //  A disk is a repeat when an image of it is still in the folder, under
    //  the name it was saved as or a new one. The folder is locked until the
    //  repeat is reported, as another session may be writing to it.
    if (stable)
    {
        folderLock.lock();

        hr = ReloadManifest();
        CHR (hr);

        hr = FindMovedImages ([&] (const FullSendRow & row) { return row.fingerprint == outDisk.fingerprint; });
        CHR (hr);

        known         = FindWithImage ([&] (const FullSendRow & row) { return row.fingerprint == outDisk.fingerprint; }) != nullptr;
        outDisk.check = (known || m_changed.contains (outDisk.fingerprint)) ? DiskCheck::Repeat : DiskCheck::New;
    }

    //  A disk that changed during its read is checked again as a repeat, but
    //  it isn't one of the manifest's.
    swapped = outDisk.check == DiskCheck::Repeat && !known;

    m_swapped[(size_t) drive - 1] = swapped;

    //  A repeat is a duplicate once while it stays in its drive, however many
    //  times the drive is checked.
    if (outDisk.check == DiskCheck::Repeat && !swapped && m_counted[(size_t) drive - 1] != outDisk.fingerprint)
    {
        m_counted[(size_t) drive - 1] = outDisk.fingerprint;
        m_screen.duplicates++;
    }
    else if (outDisk.check != DiskCheck::Repeat)
    {
        m_counted[(size_t) drive - 1].clear();
    }

    if (outDisk.check == DiskCheck::Repeat)
    {
        m_repeats[(size_t) drive - 1] = true;
        ReportRepeat (drive, outDisk);
    }

    if (folderLock.owns_lock())
    {
        folderLock.unlock();
    }

    m_blank[(size_t) drive - 1] = false;

    if (outDisk.check == DiskCheck::New)
    {
        hr = CheckBlank (drive, outDisk);
        CHR (hr);
    }

    SetDriveState (drive, outDisk.check == DiskCheck::None   ? FullSendDriveState::Empty
                        : swapped                            ? FullSendDriveState::Changed
                        : outDisk.check == DiskCheck::Repeat ? FullSendDriveState::Repeat
                        : outDisk.check == DiskCheck::Blank  ? FullSendDriveState::Blank
                                                             : FullSendDriveState::Idle);

    m_status.ClearDisk (drive, outDisk.check == DiskCheck::None   ? FullSendDiskState::Empty
                             : outDisk.check == DiskCheck::Repeat ? FullSendDiskState::Duplicate
                             : outDisk.check == DiskCheck::Blank  ? FullSendDiskState::Blank
                                                                  : FullSendDiskState::None);

    if (outDisk.check == DiskCheck::Blank)
    {
        m_status.SetTitle (drive, "Blank disk", outDisk.system);
    }
    else if (outDisk.check == DiskCheck::Repeat && !swapped)
    {
        m_status.SetTitle (drive, m_screen.names[(size_t) drive - 1], outDisk.system);
    }

    hr = m_link.ShowScreen (m_screen);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::ReadFingerprint
//
//  Track 0, then the catalog track, then CP/M's directory track for a disk with
//  no file system Casso recognizes, and the whole-disk read skips each one read
//  whole. outStable is clear for a drive with no disk, or one whose reads
//  do not match. A disk with a track 0 and no catalog track has a fingerprint
//  too, with that track as zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::ReadFingerprint (int drive, CheckedDisk & outDisk, bool & outStable)
{
    HRESULT  hr      = S_OK;
    bool     hasDisk = false;



    outStable = false;
    outDisk.image.assign (NibblizationLayer::kImageByteSize, 0);

    hr = ReadStableTrack (drive, 0, outDisk, hasDisk, outStable);
    CHR (hr);
    BAIL_OUT_IF (!hasDisk || !outStable, S_OK);

    if (!FullSendDisk::IsCatalogOnTrackZero (outDisk.image))
    {
        hr = ReadStableTrack (drive, FullSendDisk::kDosCatalogTrack, outDisk, hasDisk, outStable);
        CHR (hr);
        BAIL_OUT_IF (hasDisk && !outStable, S_OK);

        if (FullSendDisk::NeedsDirectoryTrack (outDisk.image))
        {
            hr = ReadStableTrack (drive, FullSendDisk::kCpmDirectoryTrack, outDisk, hasDisk, outStable);
            CHR (hr);
            BAIL_OUT_IF (hasDisk && !outStable, S_OK);
        }

        outStable = true;
    }

    hr = FullSendDisk::ComputeFingerprint (outDisk.image, outDisk.fingerprint);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::CheckBlank
//
//  A new disk that is formatted and has no files isn't sent: its drive's
//  number on the Apple sends it, as it sends a repeat. A DOS 3.3 disk whose
//  one file is a short BASIC program has that program's track read first, as
//  a program with no lines is what INIT writes after NEW. A blank is counted
//  once while it stays in its drive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::CheckBlank (int drive, CheckedDisk & ioDisk)
{
    HRESULT      hr      = S_OK;
    int          track   = FullSendDisk::FindLoneProgramTrack (ioDisk.image);
    bool         hasDisk = true;
    bool         whole   = false;
    std::string  system;



    if (track >= 0 && !ioDisk.IsReadWhole (track))
    {
        hr = m_link.ReadTrack (drive, track, ioDisk.image, hasDisk, whole);
        CHR (hr);

        if (hasDisk && whole)
        {
            ioDisk.wholeTracks.push_back (track);
        }
    }

    if (hasDisk && FullSendDisk::IsBlank (ioDisk.image, ioDisk.wholeTracks, system))
    {
        ioDisk.check  = DiskCheck::Blank;
        ioDisk.system = system;

        m_blank[(size_t) drive - 1]        = true;
        m_repeats[(size_t) drive - 1]      = true;
        m_screen.names[(size_t) drive - 1] = system;

        if (m_blankDisk[(size_t) drive - 1] != ioDisk.fingerprint)
        {
            m_blankDisk[(size_t) drive - 1] = ioDisk.fingerprint;
            m_status.AddBlank();
        }

        Report (std::format ("The disk in drive {} is formatted for {} but is blank, so it wasn't sent. Press {} on the Apple to send it anyway.", drive, system, drive));
    }
    else
    {
        m_blankDisk[(size_t) drive - 1].clear();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::ReadStableTrack
//
//  A track read whole passed every sector's checksum, so one read is enough. A
//  disk that is not seated, or that is being swapped, fails some of them and
//  reads differently from one read to the next, and is not sent until two reads
//  in a row match.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::ReadStableTrack (int drive, int track, CheckedDisk & ioDisk, bool & outHasDisk, bool & outStable)
{
    constexpr size_t   kTrackBytes = (size_t) NibblizationLayer::kSectorsPerTrack * NibblizationLayer::kSectorByteSize;
    HRESULT            hr          = S_OK;
    auto               start       = ioDisk.image.begin() + (ptrdiff_t) ((size_t) track * kTrackBytes);
    bool               whole       = false;
    std::vector<Byte>  previous;



    outStable = false;

    for (int read = 0; read < kFingerprintReads && !outStable; read++)
    {
        hr = m_link.ReadTrack (drive, track, ioDisk.image, outHasDisk, whole);
        CHR (hr);
        BAIL_OUT_IF (!outHasDisk, S_OK);

        outStable = whole || (read > 0 && std::equal (previous.begin(), previous.end(), start));
        previous.assign (start, start + (ptrdiff_t) kTrackBytes);
    }

    if (outStable && whole)
    {
        ioDisk.wholeTracks.push_back (track);
    }

Error:
    return hr;
}
