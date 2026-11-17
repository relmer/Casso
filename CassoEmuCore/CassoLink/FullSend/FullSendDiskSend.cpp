#include "Pch.h"

#include "CassoLink/FullSend/FullSendDiskSend.h"

#include "CassoLink/FullSend/FullSendContext.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendDriveCheck.h"
#include "CassoLink/FullSend/FullSendLink.h"
#include "CassoLink/FullSend/FullSendStatus.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::FullSendDiskSend
//
////////////////////////////////////////////////////////////////////////////////

FullSendDiskSend::FullSendDiskSend (FullSendContext & context, FullSendDriveCheck & check, ClockFn clock) :
    m_context (context),
    m_check   (check),
    m_clock   (std::move (clock))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::Send
//
//  A disk that changed during its read, by its tracks against the check before
//  the read and by its catalog track read again after it, is failed with no
//  file written, and the session goes on. Another read of it then takes Space, so
//  that a disk that reads differently every time cannot loop forever. outSent
//  is set once the disk's image is written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::Send (int drive, const FullSendCheckedDisk & checked, FullSendSendMode mode, const std::string & prefix, bool & outSent)
{
    HRESULT             hr    = S_OK;
    Sending             disk;
    FullSendDriveState  state = FullSendDriveState::Sent;



    outSent = false;

    disk.drive         = drive;
    disk.mode          = mode;
    disk.checked       = &checked;
    disk.prefix        = prefix;
    disk.image.sectors = checked.image;

    hr = Begin (disk);
    CHR (hr);

    hr = Read (disk);
    CHR (hr);

    m_context.screen.quip = 0;

    if (!disk.same)
    {
        hr = DropChanged (disk);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    //  The capture comes before the folder's lock, as it takes minutes.
    hr = CaptureProtected (disk);
    CHR (hr);

    hr = CheckVolume (disk);
    CHR (hr);

    hr = Record (disk);
    CHR (hr);

    hr = AwaitBootCheck (disk);
    CHR (hr);

    //  A raw capture has every track, so its row has no map of bad sectors.
    if (disk.image.raw.empty() && (!disk.image.unreadable.empty() || !disk.image.report.marks.empty()))
    {
        FullSendScreen::MakeTrackMap (m_context.screen.GetMap (drive), disk.image.report.marks, disk.image.unreadable);
    }

    outSent = true;
    state   = !disk.image.raw.empty() ? FullSendDriveState::Protected
            : disk.isProtected        ? FullSendDriveState::ProtectedSent
                                      : FullSendDriveState::Sent;

    m_context.screen.SetDriveState (drive, state);

    hr = m_context.ShowScreen();
    m_context.screen.tone = FullSendTone::None;
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::Begin
//
//  The disk on the Apple's screen and in the Full Send window. The other
//  drive is on deck: the first time with the instruction, and later with a
//  prompt line.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::Begin (Sending & disk)
{
    HRESULT                    hr        = S_OK;
    const std::vector<Byte>  & sectors   = disk.checked->image;
    int                        sequence  = m_context.folder.GetManifest().GetNextSequence();
    std::string                catalog   = FullSendDisk::GetCatalogTitle (sectors);
    int                        other     = 3 - disk.drive;
    bool                       verifying = m_context.link.IsVerifying();



    disk.title = FullSendDisk::MakeImageTitle (sequence, catalog, disk.prefix);

    m_context.screen.GetName (disk.drive) = GetScreenName (sequence, catalog);
    m_context.screen.GetMap (disk.drive).clear();
    m_context.screen.SetDriveState (disk.drive, FullSendDriveState::Sending);

    m_context.status.SetActivity (FullSendActivity::Sending, disk.drive);
    m_context.status.BeginDisk (disk.drive, verifying, verifying ? std::span<const int>() : std::span<const int> (disk.checked->wholeTracks));
    m_context.status.SetTitle (disk.drive, disk.title, FullSendDisk::GetSystemName (sectors));
    m_context.status.AddLine (std::format ("Sending the disk in drive {}", disk.drive));

    if (m_context.link.HasDrive (other))
    {
        m_context.screen.SetDriveState (other, FullSendDriveState::OnDeck);
        m_context.screen.quip = m_deckShown[(size_t) other - 1] ? m_context.quips.Pick() : (Byte) 0;
        m_deckShown[(size_t) other - 1] = true;
    }

    hr = m_context.ShowScreen();
    CHR (hr);

    m_context.screen.verify = m_context.link.IsVerifying();
    m_context.screen.prefix = disk.prefix;

    hr = m_context.ShowScreen();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::Read
//
//  A raw retry follows a sector read of the same disk, so its capture begins
//  at once, with that read's hash and counts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::Read (Sending & disk)
{
    HRESULT              hr    = S_OK;
    const FullSendRow  * first = nullptr;



    if (disk.mode == FullSendSendMode::RawAgain)
    {
        first = m_context.folder.FindWithImage ([&] (const FullSendRow & row) { return row.fingerprint == disk.checked->fingerprint; });
    }

    disk.isRawRetry = first != nullptr;

    if (disk.isRawRetry)
    {
        disk.first = *first;
        BAIL_OUT_IF (true, S_OK);
    }

    hr = ReadWholeDisk (disk.drive, *disk.checked, disk.image, disk.same);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::ReadWholeDisk
//
//  Reads the disk's sectors into ioImage, then its catalog track again. A disk
//  whose tracks differ from the check before the read, or whose catalog track
//  differs after it, changed during the read, and outSame is false.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::ReadWholeDisk (int drive, const FullSendCheckedDisk & checked, FullSendImage & ioImage, bool & outSame)
{
    HRESULT            hr               = S_OK;
    std::vector<Byte>  after;
    std::string        imageFingerprint;
    std::string        afterFingerprint;
    bool               comparable       = false;
    bool               hasDisk          = false;
    bool               whole            = false;
    int                afterTrack       = 0;



    outSame = false;

    hr = m_context.link.ReadDisk (drive, ioImage.sectors, checked.wholeTracks, ioImage.unreadable, ioImage.report);
    CHR (hr);

    MarkTracks (drive, ioImage);

    hr = FullSendDisk::ComputeFingerprint (ioImage.sectors, imageFingerprint);
    CHR (hr);

    after      = ioImage.sectors;
    afterTrack = (FullSendDisk::IsCatalogOnTrackZero (ioImage.sectors) || !checked.IsReadWhole (FullSendDisk::kDosCatalogTrack)) ? 0 : FullSendDisk::kDosCatalogTrack;

    //  The catalog track is read again, or track 0 when the check could not
    //  read the catalog track whole: a copy-protected disk's can have no sector
    //  that reads at all, which would look like an empty drive.
    hr = m_context.link.ReadTrack (drive, afterTrack, after, hasDisk, whole);
    CHR (hr);

    hr = FullSendDisk::ComputeFingerprint (after, afterFingerprint);
    CHR (hr);

    //  A sector that reads differently from read to read isn't a swap, which a
    //  verified read has already voted on. Nor is a track that wasn't read whole
    //  during the check, as a copy-protected disk's track 0 or catalog track can
    //  be: each read's retries recover a different set of its sectors.
    //  Every track of the fingerprint counts, CP/M's directory track too on a
    //  disk with no file system Casso recognizes.
    comparable = checked.IsReadWhole (0)
                 && (FullSendDisk::IsCatalogOnTrackZero (checked.image)
                     || (checked.IsReadWhole (FullSendDisk::kDosCatalogTrack)
                         && (!FullSendDisk::NeedsDirectoryTrack (checked.image) || checked.IsReadWhole (FullSendDisk::kCpmDirectoryTrack))));
    outSame    = hasDisk && (!comparable || (imageFingerprint == checked.fingerprint && afterFingerprint == checked.fingerprint) || !ioImage.report.marks.empty());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::MarkTracks
//
//  Each track of a disk read: unreadable with a sector that wasn't read,
//  unreliable with one read once or recovered, and otherwise read, or
//  verified when the session's reads are.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDiskSend::MarkTracks (int drive, const FullSendImage & image)
{
    std::vector<FullSendTrackMark>  marks (FullSendDriveStatus::kTracks, image.report.tracksVerified != 0 ? FullSendTrackMark::Verified : FullSendTrackMark::Read);



    for (const SectorMark & mark : image.report.marks)
    {
        if (mark.track >= 0 && mark.track < FullSendDriveStatus::kTracks)
        {
            marks[(size_t) mark.track] = mark.mark == 'X' ? FullSendTrackMark::Unreadable : FullSendTrackMark::Unreliable;
        }
    }

    for (const auto & [track, sector] : image.unreadable)
    {
        if (track >= 0 && track < FullSendDriveStatus::kTracks)
        {
            marks[(size_t) track] = FullSendTrackMark::Unreadable;
        }
    }

    for (int track = 0; track < FullSendDriveStatus::kTracks; track++)
    {
        m_context.status.MarkTrack (drive, track, marks[(size_t) track]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::DropChanged
//
//  No image is written of a disk that changed during its read, and it is sent
//  only after Space.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::DropChanged (Sending & disk)
{
    HRESULT  hr = S_OK;



    m_check.NoteChanged (disk.checked->fingerprint);
    m_context.Report (std::format ("The disk in drive {} changed while it was read, so no image was written", disk.drive));

    m_context.screen.SetDriveState (disk.drive, FullSendDriveState::Idle);
    m_context.status.ClearDisk (disk.drive, FullSendDiskState::None);
    m_context.screen.GetName (disk.drive).clear();

    hr = m_context.ShowScreen();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::CaptureProtected
//
//  A disk with a condition of the Copy-protected disks section is likely
//  copy-protected, and captured raw when the section's choice is to capture
//  it, as its sector image is unlikely to run. Unreadable sectors are a sign
//  when the disk is assessed as protected: a damaged disk with no file
//  system Casso recognizes is assessed alike, and a raw capture of it has
//  more than its sector image. A disk read again in raw mode is captured
//  whatever its signs. The capture draws on the Apple's screen, so the
//  session's screen is drawn again after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::CaptureProtected (Sending & disk)
{
    HRESULT          hr       = S_OK;
    FullSendImage  & image    = disk.image;
    ProtectionSigns  signs    = FullSendProtection::Find (image.sectors);
    bool             rawAgain = disk.mode == FullSendSendMode::RawAgain;
    bool             readSign = m_protection.readErrors && !image.unreadable.empty()
                             && std::string (FullSendDisk::AssessDamage (image.sectors, image.report.emptyTracks, (int) image.unreadable.size())) == FullSendManifest::kStatusProtected;
    std::string      reason;



    disk.isLikely = readSign || m_protection.IsMet (signs);
    BAIL_OUT_IF (!rawAgain && (!disk.isLikely || !m_protection.captureRaw), S_OK);

    //  A disk read again in raw mode whose damage is unclear has the reason
    //  in the line that began the read.
    if (disk.isLikely)
    {
        reason = readSign                                         ? std::format ("Unreadable sectors were found on the disk in drive {} during the normal transfer", disk.drive)
               : m_protection.unrecognized && signs.isUnrecognized ? std::format ("The disk in drive {} has no recognized file system", disk.drive)
               : m_protection.diskCode && signs.hasDiskCode        ? std::format ("{} on the disk in drive {} accesses the disk drive directly", signs.diskCodeFile, disk.drive)
                                                                    : std::format ("The disk in drive {} doesn't have a recognized boot loader", disk.drive);

        m_context.Report (reason + ", so it appears to be copy-protected. Restarting the capture using raw mode.");
    }

    m_context.screen.SetDriveState (disk.drive, FullSendDriveState::Capturing);
    m_context.status.SetActivity (FullSendActivity::Capturing, disk.drive);
    m_context.status.BeginCapture (disk.drive);

    hr = m_context.ShowScreen();
    CHR (hr);

    hr = m_context.link.CaptureRaw (disk.drive, IsAgain (disk.mode), image.raw, image.reads, image.marks);
    CHR (hr);

    m_context.screen.SetDriveState (disk.drive, FullSendDriveState::Sending);
    m_context.status.SetActivity (FullSendActivity::Sending, disk.drive);

    hr = m_context.ShowScreen();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::CheckVolume
//
//  A sector image has no place for the address fields' volume, so a disk
//  whose track 0 has a volume other than the one its sector image is
//  nibblized with is measured on every track and saved as a WOZ built to
//  the layouts, whatever image type is chosen. A raw capture, a disk
//  measured during its read, or one whose track 0 can't be measured, is
//  left as it is.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::CheckVolume (Sending & disk)
{
    HRESULT                   hr          = S_OK;
    FullSendImage           & image       = disk.image;
    std::vector<TrackLayout>  layouts;
    Byte                      imageVolume = NibblizationLayer::GetImageVolume (image.sectors);
    bool                      measured    = !image.raw.empty() || !image.report.layouts.empty();
    bool                      differs     = false;



    BAIL_OUT_IF (!m_protection.volumeWoz || measured, S_OK);

    hr = m_context.link.MeasureLayouts (disk.drive, 1, layouts);
    CHR (hr);

    differs = !layouts.empty() && layouts[0].nibbles != 0 && layouts[0].volume != imageVolume;
    BAIL_OUT_IF (!differs, S_OK);

    m_context.Report (std::format ("The disk in drive {} doesn't use volume {}, so it can't be saved as a {} image. Preserving its volume {} by saving it as a WOZ image instead.",
                                   disk.drive, imageVolume, FullSendDisk::GetImageExtension (image.sectors, m_context.folder.GetExtension()), layouts[0].volume));

    hr = m_context.link.MeasureLayouts (disk.drive, NibblizationLayer::kTrackCount, image.report.layouts);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::Record
//
//  The disk is numbered, written and listed under the folder's lock, from
//  the manifest as it is now: another Apple may have sent to this folder
//  during the read. Every disk read is written; one that matches an earlier
//  image is noted as its duplicate. One sent anyway is a sent disk whose
//  row has the earlier image too, so the repeat is in the manifest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::Record (Sending & disk)
{
    HRESULT                       hr         = S_OK;
    std::unique_lock<std::mutex>  folderLock { m_context.folder.GetLock() };
    std::string                   system     = FullSendDisk::GetSystemName (disk.image.sectors);
    size_t                        bytes      = 0;



    hr = Identify (disk);
    CHR (hr);

    hr = m_context.folder.WriteImage (disk.title, disk.image, !disk.hasEarlier, disk.row.file);
    CHRF (hr, m_fileFailed = true);

    //  The disk just sent isn't a duplicate while it stays in its drive.
    m_check.NoteSent (disk.drive, disk.checked->fingerprint);

    Tally (disk);

    bytes = disk.hasEarlier ? 0 : disk.image.raw.empty() ? disk.image.sectors.size() : disk.image.raw.size();

    m_context.status.SetTitle (disk.drive, disk.title, system);
    m_context.status.EndDisk (disk.drive, disk.row, bytes);

    ReportSent (disk);

    m_context.folder.GetManifest().Add (disk.row);

    hr = m_context.WriteManifest();
    CHRF (hr, m_fileFailed = true);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::Identify
//
//  The disk's number, its title with that number, and the earlier image it
//  matches, if any. The caller has the folder's lock.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::Identify (Sending & disk)
{
    HRESULT                   hr       = S_OK;
    int                       sequence = 0;
    std::string               hash;
    FullSendFolder::RowMatch  sameHash = [&] (const FullSendRow & row) { return row.hash == hash; };
    const FullSendRow       * match    = nullptr;



    hr = m_context.folder.Reload();
    CHR (hr);

    sequence = m_context.folder.GetManifest().GetNextSequence();

    if (disk.isRawRetry)
    {
        hash = disk.first.hash;
    }
    else
    {
        hr = FullSendDisk::ComputeHash (disk.image.sectors, hash);
        CHR (hr);
    }

    hr = m_context.FindMovedImages (sameHash);
    CHR (hr);

    match           = m_context.folder.FindWithImage (sameHash);
    disk.match      = match != nullptr ? match->file : std::string();
    disk.hasEarlier = match != nullptr && disk.mode == FullSendSendMode::New;
    disk.earlier    = disk.hasEarlier ? disk.match : std::string();
    disk.title      = FullSendDisk::MakeImageTitle (sequence, FullSendDisk::GetCatalogTitle (disk.image.sectors), disk.prefix);

    BuildRow (disk, sequence, hash);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::BuildRow
//
//  The disk's row, before its file and status are known. A raw retry's
//  counts are those of the sector read before it.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDiskSend::BuildRow (Sending & disk, int sequence, const std::string & hash)
{
    const std::vector<SectorMark>  & marks = disk.image.report.marks;
    int                              read  = disk.image.report.tracksVerified;
    FullSendRow                    & row   = disk.row;



    row.sequence       = sequence;
    row.drive          = disk.drive;
    row.fingerprint    = disk.checked->fingerprint;
    row.hash           = hash;
    row.time           = m_clock();
    row.unreadable     = (int) disk.image.unreadable.size();
    row.recovered      = (int) std::count_if (marks.begin(), marks.end(), [] (const SectorMark & mark) { return mark.mark == 'R'; });
    row.unreliable     = (int) std::count_if (marks.begin(), marks.end(), [] (const SectorMark & mark) { return mark.mark == 'U'; });
    row.verification   = read == 0                              ? FullSendManifest::kVerifyNone
                       : read >= NibblizationLayer::kTrackCount ? FullSendManifest::kVerifyFull
                                                                : FullSendManifest::kVerifyPartial;
    row.writeProtected = m_context.link.IsWriteProtected (disk.drive);

    if (disk.isRawRetry)
    {
        row.unreadable   = disk.first.unreadable;
        row.recovered    = disk.first.recovered;
        row.unreliable   = disk.first.unreliable;
        row.verification = disk.first.verification;
    }

    disk.image.writeProtected = row.writeProtected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::Tally
//
//  The disk's status in its row, and its counts and tone on the Apple.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDiskSend::Tally (Sending & disk)
{
    FullSendRow     & row     = disk.row;
    FullSendScreen  & screen  = m_context.screen;
    FullSendImage   & image   = disk.image;
    bool              isRaw   = !image.raw.empty();
    bool              isClean = image.unreadable.empty();
    std::string       name    = GetScreenName (row.sequence, FullSendDisk::GetCatalogTitle (image.sectors));



    screen.sent++;

    if (disk.hasEarlier)
    {
        row.status      = FullSendManifest::kStatusDuplicate;
        row.duplicateOf = disk.earlier;
        screen.tone     = FullSendTone::Duplicate;
        screen.GetName (disk.drive) = name + ", dupe of " + disk.earlier;
        screen.duplicates++;
    }
    else
    {
        row.status       = disk.isLikely ? FullSendManifest::kStatusProtected : isClean ? FullSendManifest::kStatusSent : FullSendDisk::AssessDamage (image.sectors, image.report.emptyTracks, (int) image.unreadable.size());
        disk.isProtected = row.status == FullSendManifest::kStatusProtected;
        row.status       = isRaw ? FullSendManifest::kStatusRawCapture : row.status;
        row.marks        = isRaw ? image.marks : std::string();
        row.duplicateOf  = disk.match;
        screen.tone      = disk.isProtected ? FullSendTone::Protected : isClean ? FullSendTone::Done : FullSendTone::BadSectors;
        screen.prot      += disk.isProtected ? 1 : 0;
        screen.bad       += (isClean || disk.isProtected) ? 0 : 1;
        screen.recovered += (row.recovered != 0 || row.unreliable != 0) ? 1 : 0;
        screen.GetName (disk.drive) = name;
    }

    if (disk.isProtected && !isRaw)
    {
        m_context.Report (std::format ("{} is likely copy-protected, and its image {} has the sectors that could be read.", disk.title, row.file));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::ReportSent
//
////////////////////////////////////////////////////////////////////////////////

void FullSendDiskSend::ReportSent (const Sending & disk)
{
    const FullSendRow  & row  = disk.row;
    std::string          took = FullSendStatus::FormatClock (m_context.status.GetSnapshot().disks.front().seconds);



    m_context.Report (disk.hasEarlier             ? std::format ("Disk {:04} is {}, a duplicate of {}. Took {}.", row.sequence, row.file, row.duplicateOf, took)
                      : !row.duplicateOf.empty()  ? std::format ("Disk {:04} is {}, sent again though it matches {}. Took {}.", row.sequence, row.file, row.duplicateOf, took)
                      : !disk.image.raw.empty()   ? std::format ("Disk {:04} is {}, a raw capture of a disk that is likely copy-protected. Took {}.", row.sequence, row.file, took)
                                                  : std::format ("Disk {:04} is {}, with {} unreadable sectors. Took {}.", row.sequence, row.file, row.unreadable, took));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::AwaitBootCheck
//
//  The image is booted on the PC while its disk is in the drive, so an
//  image that did not boot is on the Apple at the pause that follows. Where
//  the Apple sounds the results, the failure's tone is for an image that did
//  not boot or has unreadable sectors, and the success's for any other. A
//  duplicate isn't booted.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDiskSend::AwaitBootCheck (Sending & disk)
{
    HRESULT          hr     = S_OK;
    FullSendScreen & screen = m_context.screen;
    FullSendTone     tone   = screen.tone;
    bool             booted = false;
    bool             failed = false;



    BAIL_OUT_IF (disk.hasEarlier, S_OK);

    booted = m_context.link.NoteImage (m_context.folder.GetPath (disk.row.file));
    BAIL_OUT_IF (!booted, S_OK);

    screen.tone = FullSendTone::None;
    screen.SetDriveState (disk.drive, FullSendDriveState::Validating);
    m_context.status.SetActivity (FullSendActivity::Validating, disk.drive);

    hr = m_context.ShowScreen();
    CHR (hr);

    m_context.boots.ClearLatest();

    hr = m_context.link.AwaitBoot (disk.row.file);
    CHR (hr);

    m_context.boots.Apply();

    failed = m_context.boots.GetLatest() == FullSendManifest::kBootDidNotBoot || (disk.image.raw.empty() && !disk.image.unreadable.empty());

    screen.tone = !m_context.link.HasBootTones() || m_context.boots.GetLatest().empty() ? tone
                : failed                                                                ? FullSendTone::DidNotBoot
                                                                                        : FullSendTone::Booted;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDiskSend::GetScreenName
//
//  The image's number and the disk's name, with no prefix, as the prefix is
//  on every disk and the row is short.
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendDiskSend::GetScreenName (int sequence, const std::string & catalogTitle)
{
    return FullSendDisk::MakeImageTitle (sequence, catalogTitle, std::string());
}
