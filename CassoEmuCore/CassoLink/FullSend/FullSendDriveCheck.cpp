#include "Pch.h"

#include "CassoLink/FullSend/FullSendDriveCheck.h"

#include "CassoLink/FullSend/FullSendContext.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendLink.h"
#include "CassoLink/FullSend/FullSendStatus.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck::FullSendDriveCheck
//
////////////////////////////////////////////////////////////////////////////////

FullSendDriveCheck::FullSendDriveCheck (FullSendContext & context) :
    m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck::Check
//
//  A disk is a repeat when the folder has an image of it, under the name it
//  was saved as or a new one. The folder is locked until the repeat
//  is reported, as another session may be writing to it. A disk that changed
//  during its read is checked again as a repeat, but it isn't one of the
//  manifest's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDriveCheck::Check (int drive, FullSendCheckedDisk & outDisk)
{
    HRESULT                       hr         = S_OK;
    DriveRecord                 & record     = GetRecord (drive);
    bool                          present    = m_context.link.HasDrive (drive);
    bool                          stable     = false;
    bool                          known      = false;
    std::unique_lock<std::mutex>  folderLock = std::unique_lock<std::mutex> (m_context.folder.GetLock(), std::defer_lock);



    outDisk        = {};
    record.repeat  = false;
    record.swapped = false;

    BAIL_OUT_IF (!present, S_OK);

    m_context.screen.SetDriveState (drive, FullSendDriveState::Checking);
    m_context.status.SetActivity (FullSendActivity::Checking, drive);
    m_context.status.ClearDisk (drive, FullSendDiskState::Checking);

    hr = m_context.ShowScreen();
    CHR (hr);

    hr = ReadFingerprint (drive, outDisk, stable);
    CHR (hr);

    if (stable)
    {
        folderLock.lock();

        hr = FindRepeat (outDisk, known);
        CHR (hr);
    }

    record.swapped = outDisk.check == FullSendDiskCheck::Repeat && !known;

    CountRepeat (drive, outDisk, record.swapped);

    if (outDisk.check == FullSendDiskCheck::Repeat)
    {
        record.repeat = true;
        ReportRepeat (drive, outDisk);
    }

    if (folderLock.owns_lock())
    {
        folderLock.unlock();
    }

    record.blank = false;

    if (outDisk.check == FullSendDiskCheck::New)
    {
        hr = CheckBlank (drive, outDisk);
        CHR (hr);
    }

    hr = ShowResult (drive, outDisk);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck::ReadFingerprint
//
//  Track 0, then the catalog track, then CP/M's directory track for a disk with
//  no file system Casso recognizes, and the whole-disk read skips each one read
//  whole. outStable is clear for a drive with no disk, or one whose reads
//  do not match. A disk with a track 0 and no catalog track has a fingerprint
//  too, with that track as zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDriveCheck::ReadFingerprint (int drive, FullSendCheckedDisk & outDisk, bool & outStable)
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
//  FullSendDriveCheck::ReadStableTrack
//
//  A track read whole passed every sector's checksum, so one read is enough. A
//  disk that is not seated, or that is being swapped, fails some of them and
//  reads differently from one read to the next, and is not sent until two reads
//  in a row match.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDriveCheck::ReadStableTrack (int drive, int track, FullSendCheckedDisk & ioDisk, bool & outHasDisk, bool & outStable)
{
    constexpr size_t   kTrackBytes = (size_t) NibblizationLayer::kSectorsPerTrack * NibblizationLayer::kSectorByteSize;
    HRESULT            hr          = S_OK;
    auto               start       = ioDisk.image.begin() + (ptrdiff_t) ((size_t) track * kTrackBytes);
    bool               whole       = false;
    std::vector<Byte>  previous;



    outStable = false;

    for (int read = 0; read < kFingerprintReads && !outStable; read++)
    {
        hr = m_context.link.ReadTrack (drive, track, ioDisk.image, outHasDisk, whole);
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





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck::FindRepeat
//
//  From the manifest as it is in the folder now, with any of its images that
//  were renamed found. The caller has the folder's lock.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDriveCheck::FindRepeat (FullSendCheckedDisk & ioDisk, bool & outKnown)
{
    HRESULT                   hr      = S_OK;
    FullSendFolder::RowMatch  matches = [&] (const FullSendRow & row) { return row.fingerprint == ioDisk.fingerprint; };



    outKnown = false;

    hr = m_context.folder.Reload();
    CHR (hr);

    hr = m_context.FindMovedImages (matches);
    CHR (hr);

    outKnown     = m_context.folder.FindWithImage (matches) != nullptr;
    ioDisk.check = (outKnown || m_changed.contains (ioDisk.fingerprint)) ? FullSendDiskCheck::Repeat : FullSendDiskCheck::New;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck::CountRepeat
//
//  A repeat is a duplicate once while it stays in its drive, however many
//  times the drive is checked.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDriveCheck::CountRepeat (int drive, const FullSendCheckedDisk & disk, bool swapped)
{
    DriveRecord  & record = GetRecord (drive);



    if (disk.check == FullSendDiskCheck::Repeat && !swapped && record.counted != disk.fingerprint)
    {
        record.counted = disk.fingerprint;
        m_context.screen.duplicates++;
    }
    else if (disk.check != FullSendDiskCheck::Repeat)
    {
        record.counted.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck::ReportRepeat
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDriveCheck::ReportRepeat (int drive, const FullSendCheckedDisk & disk)
{
    const FullSendRow  * match = m_context.folder.FindWithImage ([&] (const FullSendRow & row) { return row.fingerprint == disk.fingerprint; });



    if (match == nullptr)
    {
        m_context.Report (std::format ("The disk in drive {} changed while it was read. Press {} on the Apple to read it again.", drive, drive));
    }
    else
    {
        m_context.screen.GetName (drive) = match->file;

        m_context.Report (std::format ("The disk in drive {} matches {}. A match compares only a few tracks, so different disks can match. Press {} on the Apple to send it anyway.", drive, match->file, drive));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck::CheckBlank
//
//  A new disk that is formatted and has no files isn't sent: its drive's
//  number on the Apple sends it, as it sends a repeat. A DOS 3.3 disk whose
//  one file is a short BASIC program has that program's track read first, as
//  a program with no lines is what INIT writes after NEW. A blank is counted
//  once while it stays in its drive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDriveCheck::CheckBlank (int drive, FullSendCheckedDisk & ioDisk)
{
    HRESULT        hr      = S_OK;
    DriveRecord  & record  = GetRecord (drive);
    int            track   = FullSendDisk::FindLoneProgramTrack (ioDisk.image);
    bool           hasDisk = true;
    bool           whole   = false;
    std::string    system;



    if (track >= 0 && !ioDisk.IsReadWhole (track))
    {
        hr = m_context.link.ReadTrack (drive, track, ioDisk.image, hasDisk, whole);
        CHR (hr);

        if (hasDisk && whole)
        {
            ioDisk.wholeTracks.push_back (track);
        }
    }

    if (hasDisk && FullSendDisk::IsBlank (ioDisk.image, ioDisk.wholeTracks, system))
    {
        ioDisk.check  = FullSendDiskCheck::Blank;
        ioDisk.system = system;
        record.blank  = true;
        record.repeat = true;

        m_context.screen.GetName (drive) = system;

        if (record.blankDisk != ioDisk.fingerprint)
        {
            record.blankDisk = ioDisk.fingerprint;
            m_context.status.AddBlank();
        }

        m_context.Report (std::format ("The disk in drive {} is formatted for {} but is blank, so it wasn't sent. Press {} on the Apple to send it anyway.", drive, system, drive));
    }
    else
    {
        record.blankDisk.clear();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck::ShowResult
//
//  The check's result on the Apple and in the Full Send window.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDriveCheck::ShowResult (int drive, const FullSendCheckedDisk & disk)
{
    HRESULT  hr      = S_OK;
    bool     swapped = GetRecord (drive).swapped;



    m_context.screen.SetDriveState (drive, disk.check == FullSendDiskCheck::None   ? FullSendDriveState::Empty
                                         : swapped                                 ? FullSendDriveState::Changed
                                         : disk.check == FullSendDiskCheck::Repeat ? FullSendDriveState::Repeat
                                         : disk.check == FullSendDiskCheck::Blank  ? FullSendDriveState::Blank
                                                                                   : FullSendDriveState::Idle);

    m_context.status.ClearDisk (drive, disk.check == FullSendDiskCheck::None   ? FullSendDiskState::Empty
                                     : disk.check == FullSendDiskCheck::Repeat ? FullSendDiskState::Duplicate
                                     : disk.check == FullSendDiskCheck::Blank  ? FullSendDiskState::Blank
                                                                               : FullSendDiskState::None);

    if (disk.check == FullSendDiskCheck::Blank)
    {
        m_context.status.SetTitle (drive, "Blank disk", disk.system);
    }
    else if (disk.check == FullSendDiskCheck::Repeat && !swapped)
    {
        m_context.status.SetTitle (drive, m_context.screen.GetName (drive), disk.system);
    }

    hr = m_context.ShowScreen();
    CHR (hr);

Error:
    return hr;
}
