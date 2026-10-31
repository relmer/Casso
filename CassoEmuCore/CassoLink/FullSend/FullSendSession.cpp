#include "Pch.h"

#include "CassoLink/FullSend/FullSendSession.h"

#include "CassoLink/LinkProgram.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DurableCommit.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::FullSendSession
//
////////////////////////////////////////////////////////////////////////////////

FullSendSession::FullSendSession (IFullSendLink & link, IDiskFileIo & fileIo, ClockFn clock, std::string folder, std::string extension) :
    m_link      (link),
    m_fileIo    (fileIo),
    m_clock     (std::move (clock)),
    m_folder    (std::move (folder)),
    m_extension (std::move (extension))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::Run
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::Run()
{
    HRESULT  hr = S_OK;



    hr = LoadManifest();
    CHR (hr);

    for (int drive : { 1, 2 })
    {
        if (!m_link.HasDrive (drive))
        {
            SetDriveState (drive, FullSendDriveState::Off);
        }
    }

    for (;;)
    {
        hr = RunRound();
        CHR (hr);
    }

Error:
    End (hr);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::LoadManifest
//
//  A folder's manifest is read once. One that cannot be parsed ends the session
//  before any disk is read, and it is never overwritten.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::LoadManifest()
{
    HRESULT            hr     = S_OK;
    std::string        path   = GetPath (FullSendManifest::kFileName);
    std::vector<Byte>  bytes;
    bool               exists = m_fileIo.Exists (path);



    BAIL_OUT_IF (!exists, S_OK);

    hr = m_fileIo.ReadAllBytes (path, bytes);
    CHRF (hr, m_endReason = EndReason::ManifestDamaged);

    hr = m_manifest.Parse (bytes);
    CHRF (hr, m_endReason = EndReason::ManifestDamaged);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::RunRound
//
//  The other drive and then the drive just read, a new disk in either sent at
//  once. At the start drive 1 is first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::RunRound()
{
    HRESULT             hr    = S_OK;
    std::array<int, 2>  order = { m_lastRead == 1 ? 2 : 1, m_lastRead == 1 ? 1 : 2 };
    CheckedDisk         disk;



    for (int drive : order)
    {
        hr = CheckDrive (drive, disk);
        CHR (hr);

        if (disk.check == DiskCheck::New)
        {
            hr = SendDisk (drive, disk);
            CHR (hr);
            BAIL_OUT_IF (true, S_OK);
        }
    }

    hr = Pause();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::Pause
//
//  Space checks both drives again. A new disk is sent, and otherwise the first
//  repeated disk, so with one in both the drive not read last is sent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::Pause()
{
    HRESULT             hr     = S_OK;
    FullSendKey         key    = FullSendKey::Space;
    std::array<int, 2>  order  = { m_lastRead == 1 ? 2 : 1, m_lastRead == 1 ? 1 : 2 };
    int                 repeat = 0;
    CheckedDisk         disk;
    CheckedDisk         repeated;



    m_state       = State::Paused;
    m_screen.quip = PickQuip();

    hr = m_link.ShowScreen (m_screen);
    CHR (hr);

    hr = m_link.WaitForKey (key);
    CHR (hr);

    m_screen.quip = 0;

    CBREx (key != FullSendKey::Quit, IFullSendLink::kQuit);

    for (int drive : order)
    {
        hr = CheckDrive (drive, disk);
        CHR (hr);

        if (disk.check == DiskCheck::New)
        {
            hr = SendDisk (drive, disk);
            CHR (hr);
            BAIL_OUT_IF (true, S_OK);
        }

        if (disk.check == DiskCheck::Repeat && repeat == 0)
        {
            repeat   = drive;
            repeated = disk;
        }
    }

    if (repeat != 0)
    {
        hr = SendDisk (repeat, repeated);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::PickQuip
//
//  Every line once in a random order, then again in a new order that does not
//  begin with the line just shown.
//
////////////////////////////////////////////////////////////////////////////////

Byte FullSendSession::PickQuip()
{
    if (m_quips.empty())
    {
        for (int line = 1; line <= LinkProgram::kQuipCount; line++)
        {
            m_quips.push_back ((Byte) line);
        }

        std::shuffle (m_quips.begin(), m_quips.end(), m_random);

        if (m_quips.back() == m_lastQuip)
        {
            std::swap (m_quips.front(), m_quips.back());
        }
    }

    m_lastQuip = m_quips.back();
    m_quips.pop_back();

    return m_lastQuip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::CheckDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::CheckDrive (int drive, CheckedDisk & outDisk)
{
    HRESULT  hr      = S_OK;
    bool     present = m_link.HasDrive (drive);
    bool     stable  = false;



    outDisk = {};

    BAIL_OUT_IF (!present, S_OK);

    m_state = State::Checking;
    SetDriveState (drive, FullSendDriveState::Checking);

    hr = m_link.ShowScreen (m_screen);
    CHR (hr);

    hr = ReadFingerprint (drive, outDisk, stable);
    CHR (hr);

    if (stable)
    {
        outDisk.check = (m_manifest.HasFingerprint (outDisk.fingerprint) || m_changed.contains (outDisk.fingerprint)) ? DiskCheck::Repeat : DiskCheck::New;
    }

    SetDriveState (drive, outDisk.check == DiskCheck::None   ? FullSendDriveState::Empty
                        : outDisk.check == DiskCheck::Repeat ? FullSendDriveState::Repeat
                                                             : FullSendDriveState::Idle);

    hr = m_link.ShowScreen (m_screen);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::ReadFingerprint
//
//  Each track twice before the arm moves to the next, as a move between track
//  0 and the catalog track costs more than a read. outStable is clear for a
//  drive with no disk, or one whose reads do not match. A disk with a track 0
//  and no catalog track has a fingerprint too, with that track as zeros.
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

        outStable = true;
    }

    hr = FullSendDisk::ComputeFingerprint (outDisk.image, outDisk.fingerprint);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::ReadStableTrack
//
//  A disk that is not seated, or that is being swapped, reads differently from
//  one read to the next, and is not sent until two reads in a row match.
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

        outStable = read > 0 && std::equal (previous.begin(), previous.end(), start);
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
//  FullSendSession::SendDisk
//
//  A disk that changed during its read, by its tracks against the check before
//  the read and by its catalog track read again after it, is failed with no
//  file written, and the session goes on. Another read of it then takes Space, so
//  that a disk that reads differently every time cannot loop forever.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::SendDisk (int drive, const CheckedDisk & checked)
{
    HRESULT                             hr               = S_OK;
    int                                 sequence         = m_manifest.GetNextSequence();
    std::string                         title            = FullSendDisk::MakeImageTitle (sequence, FullSendDisk::GetCatalogTitle (checked.image));
    std::string                         fingerprint      = checked.fingerprint;
    std::vector<Byte>                   image            = checked.image;
    std::vector<Byte>                   after;
    std::vector<std::pair<int, int>>    unreadable;
    std::string                         imageFingerprint;
    std::string                         afterFingerprint;
    std::string                         hash;
    const FullSendRow                 * earlier          = nullptr;
    FullSendRow                         row;
    bool                                hasDisk          = false;
    bool                                whole            = false;
    bool                                same             = false;



    m_state       = State::Sending;
    m_lastRead    = drive;
    m_screen.name = title;
    SetDriveState (drive, FullSendDriveState::Sending);

    if (m_link.HasDrive (3 - drive))
    {
        SetDriveState (3 - drive, FullSendDriveState::OnDeck);
        m_screen.quip = PickQuip();
    }

    hr = m_link.ShowScreen (m_screen);
    CHR (hr);

    hr = m_link.ReadDisk (drive, image, checked.wholeTracks, unreadable);
    CHR (hr);

    hr = FullSendDisk::ComputeFingerprint (image, imageFingerprint);
    CHR (hr);

    after = image;

    hr = m_link.ReadTrack (drive, FullSendDisk::IsCatalogOnTrackZero (image) ? 0 : FullSendDisk::kDosCatalogTrack, after, hasDisk, whole);
    CHR (hr);

    hr = FullSendDisk::ComputeFingerprint (after, afterFingerprint);
    CHR (hr);

    same          = imageFingerprint == fingerprint && hasDisk && afterFingerprint == fingerprint;
    m_screen.quip = 0;

    if (!same)
    {
        m_changed.insert (fingerprint);
        m_link.Report (std::format ("The disk in drive {} changed while it was read, so no image was written", drive));

        SetDriveState (drive, FullSendDriveState::Idle);
        m_screen.name.clear();

        hr = m_link.ShowScreen (m_screen);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = FullSendDisk::ComputeHash (image, hash);
    CHR (hr);

    earlier = m_manifest.FindImageByHash (hash);

    row.sequence    = sequence;
    row.drive       = drive;
    row.fingerprint = fingerprint;
    row.hash        = hash;
    row.time        = m_clock();
    row.unreadable  = (int) unreadable.size();

    if (earlier != nullptr)
    {
        row.status      = FullSendManifest::kStatusDuplicate;
        row.duplicateOf = earlier->file;
        m_screen.tone   = FullSendTone::Duplicate;
        m_screen.name   = std::format ("{:04}", sequence);
        m_screen.duplicates++;
    }
    else
    {
        title = FullSendDisk::MakeImageTitle (sequence, FullSendDisk::GetCatalogTitle (image));

        hr = WriteImage (title, image, unreadable, row.file);
        CHRF (hr, m_fileFailed = true);

        row.status    = unreadable.empty() ? FullSendManifest::kStatusSent : FullSendManifest::kStatusProtected;
        m_screen.tone = unreadable.empty() ? FullSendTone::Done : FullSendTone::BadSectors;
        m_screen.name = title;
        m_screen.sent++;
        m_screen.bad += unreadable.empty() ? 0 : 1;
    }

    m_link.Report (row.file.empty() ? std::format ("Disk {:04} is a duplicate of {}", sequence, row.duplicateOf)
                                    : std::format ("Disk {:04} is {}, with {} unreadable sectors", sequence, row.file, row.unreadable));

    m_manifest.Add (row);

    hr = WriteManifest();
    CHRF (hr, m_fileFailed = true);

    SetDriveState (drive, FullSendDriveState::Sent);

    hr = m_link.ShowScreen (m_screen);
    m_screen.tone = FullSendTone::None;
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::WriteImage
//
//  CreateNew, so an existing file is never overwritten. When a file from another
//  session or the user is already at the title, the next number in parentheses
//  is added. The list of unreadable sectors goes beside the image, as for
//  Read disk to image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::WriteImage (const std::string & title, const std::vector<Byte> & image, const std::vector<std::pair<int, int>> & unreadable, std::string & outFile)
{
    HRESULT               hr        = S_OK;
    std::string           extension = FullSendDisk::GetImageExtension (image, m_extension);
    std::string           text;
    std::vector<Byte>     bytes;
    DiskFormat            format    = DiskFormat::Dsk;
    CommitPlan::Progress  progress;



    hr = DiskImageStore::GetSourceFormatByExtension (extension, format);
    CHR (hr);

    hr = BlankDiskBuilder::WrapInContainer (format, NibbleImageCodec::kNibTrackSize, false, 0, image, bytes);
    CHR (hr);

    outFile = title + extension;

    for (int copy = 2; m_fileIo.Exists (GetPath (outFile)); copy++)
    {
        outFile = std::format ("{} ({}){}", title, copy, extension);
    }

    hr = DurableCommit::Commit (m_fileIo, GetPath (outFile), bytes, m_commitTag++, CommitMode::CreateNew, progress);
    CHR (hr);

    if (!unreadable.empty())
    {
        text = CassoLinkDiskTransfer::FormatUnreadableList (unreadable);

        hr = DurableCommit::Commit (m_fileIo, CassoLinkDiskTransfer::GetUnreadableListPath (GetPath (outFile)), std::vector<Byte> (text.begin(), text.end()), m_commitTag++, CommitMode::Replace, progress);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::WriteManifest
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::WriteManifest()
{
    HRESULT               hr = S_OK;
    CommitPlan::Progress  progress;



    hr = DurableCommit::Commit (m_fileIo, GetPath (FullSendManifest::kFileName), m_manifest.Format(), m_commitTag++, CommitMode::Replace, progress);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::End
//
////////////////////////////////////////////////////////////////////////////////

void FullSendSession::End (HRESULT hr)
{
    std::string  why;



    if (m_endReason == EndReason::None)
    {
        m_endReason = hr == IFullSendLink::kQuit              ? EndReason::Quit
                    : hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED) ? EndReason::Canceled
                    : m_fileFailed                               ? EndReason::FileFailed
                                                                 : EndReason::LinkFailed;
    }

    m_state = State::Ended;

    switch (m_endReason)
    {
        case EndReason::Quit:          why = "Q was pressed on the Apple.";                                                                                            break;
        case EndReason::Canceled:        why = "It was canceled.";                                                                                                            break;
        case EndReason::ManifestDamaged: why = std::format ("{} could not be read, so no disk was read. Repair or move it, then start again.", FullSendManifest::kFileName); break;
        case EndReason::FileFailed:      why = "A file could not be written to the folder.";                                                                                  break;
        default:                         why = "The link failed.";                                                                                                            break;
    }

    m_message = std::format ("Full send ended with {} sent, {} duplicates and {} with bad sectors. {}", m_screen.sent, m_screen.duplicates, m_screen.bad, why);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::GetPath
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendSession::GetPath (const std::string & file) const
{
    return (std::filesystem::path (m_folder) / file).string();
}
