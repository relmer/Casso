#include "Pch.h"

#include "CassoLink/FullSend/FullSendSession.h"

#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DurableCommit.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen::GetRows
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> FullSendScreen::GetRows() const
{
    std::vector<std::string>  rows = { "FULL SEND    VERIFY: OFF", std::format ("SENT {}   DUP {}   BAD {}", sent, duplicates, bad), std::string (40, '-') };



    if (verify)
    {
        std::string  flag = "VERIFY: ON";

        for (char & ch : flag)
        {
            ch = (char) (ch | 0x80);
        }

        rows[0] = "FULL SEND    " + flag;
    }

    if (recovered != 0)
    {
        rows[1] += std::format ("   FIXED {}", recovered);
    }

    if (prot != 0)
    {
        rows[1] += std::format ("   PROT {}", prot);
    }

    for (size_t i = 0; i < drives.size(); i++)
    {
        std::string  row = std::format ("D{}: ", i + 1);

        switch (drives[i])
        {
            case FullSendDriveState::Off:      row.clear();                 break;
            case FullSendDriveState::Idle:     row += "IDLE";               break;
            case FullSendDriveState::Checking: row += "CHECKING";           break;
            case FullSendDriveState::Sending:  row += (verify ? "VERIFYING " : "SENDING ") + names[i]; break;
            case FullSendDriveState::Empty:    row += "NO DISK";            break;
            case FullSendDriveState::Repeat:   row += "ALREADY SENT";       break;
            case FullSendDriveState::Sent:     row += "SENT " + names[i];   break;
            case FullSendDriveState::Protected: row += "LIKELY COPY-PROTECTED " + names[i]; break;
            case FullSendDriveState::OnDeck:
                row += "ON DECK--";
                row += quip == 0 ? "INSERT A DISK TO SEND NEXT" : GetQuipText (quip);
                break;
        }

        rows.push_back (row);
        rows.push_back (maps[i]);
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen::GetQuipText
//
////////////////////////////////////////////////////////////////////////////////

const char * FullSendScreen::GetQuipText (Byte quip)
{
    static constexpr std::array<const char *, kQuipCount>  s_kQuips =
    {
        "IS THAT ALL YOU'VE GOT?",
        "ANOTHER!",
        "I CAN HAZ DISK?",
        "THANK YOU SIR, ANOTHER?",
        "MORE! MOOORE!!!",
        "FEED ME, SEYMOUR!",
        "INSERT COIN... ER, DISK",
        "THIS DRIVE IS HUNGRY",
        "NEXT VICTIM, PLEASE",
        "PLEASE SIR, SOME MORE",
        "BIGGER STACK NEEDED",
        "NEED INPUT!",
        "ME WANT DISK! NOM NOM NOM",
        "HIT ME AGAIN, DEALER",
        "ONE DISK IS NEVER ENOUGH",
        "JUST ONE MORE, YOU SAID",
    };



    return s_kQuips[(size_t) (quip - 1) % s_kQuips.size()];
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::FullSendSession
//
////////////////////////////////////////////////////////////////////////////////

FullSendSession::FullSendSession (IFullSendLink & link, IDiskFileIo & fileIo, ClockFn clock, std::string folder, std::string extension, std::string prefix) :
    m_link      (link),
    m_fileIo    (fileIo),
    m_clock     (std::move (clock)),
    m_folder    (std::move (folder)),
    m_extension (std::move (extension)),
    m_prefix    (std::move (prefix))
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

    hr = AwaitBegin();
    CHR (hr);

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
    std::string        legacy = GetPath (FullSendManifest::kLegacyFileName);
    std::vector<Byte>  bytes;
    bool               exists = m_fileIo.Exists (path);



    if (!exists && m_fileIo.Exists (legacy))
    {
        hr = m_fileIo.RenameWithoutReplacing (legacy, path);
        CHRF (hr, m_endReason = EndReason::ManifestDamaged);

        exists = true;
    }

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
//  FullSendSession::AwaitBegin
//
//  No disk is read until Space, so a disk left in a drive isn't sent before the
//  screen has been read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::AwaitBegin()
{
    HRESULT                   hr    = S_OK;
    FullSendKey               key   = FullSendKey::Drive1;
    std::vector<std::string>  lines = { "V: VERIFY READS EACH TRACK THREE TIMES.", "SLOWER, BUT CATCHES BAD READS.", "" };



    if (!m_link.IsBootstrapped())
    {
        lines.push_back ("REMOVE THE CASSOLINK DISK");
    }

    lines.push_back ("INSERT DISKS TO BE SENT INTO THE DRIVES");
    lines.push_back ("THEN PRESS SPACE TO BEGIN");

    while (key != FullSendKey::Space)
    {
        m_screen.verify = m_link.IsVerifying();

        hr = m_link.ShowScreen (m_screen);
        CHR (hr);

        hr = m_link.WaitForKey (lines, key);
        CHR (hr);

        CBREx (key != FullSendKey::Quit, IFullSendLink::kQuit);

        if (key == FullSendKey::Verify)
        {
            m_link.ToggleVerify();
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::RunRound
//
//  The other drive and then the drive just read, a new disk in either sent at
//  once. At the start drive 1 is first. A drive whose disk was just sent is
//  passed over, as its catalog track was read again a moment ago; one whose
//  disk changed during the read is checked, so its new disk goes with no key.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::RunRound()
{
    HRESULT             hr    = S_OK;
    std::array<int, 2>  order = { m_lastRead == 1 ? 2 : 1, m_lastRead == 1 ? 1 : 2 };
    CheckedDisk         disk;



    for (int drive : order)
    {
        if (drive == m_lastRead && m_lastSent)
        {
            continue;
        }

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
//  Space checks both drives, and a new disk is sent. A repeated disk isn't read:
//  the prompt lists its drive, and that drive's number checks it again and sends
//  it. The whole-disk hash discards an exact copy.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::Pause()
{
    HRESULT                   hr      = S_OK;
    FullSendKey               key     = FullSendKey::Space;
    std::array<int, 2>        order   = { m_lastRead == 1 ? 2 : 1, m_lastRead == 1 ? 1 : 2 };
    std::array<bool, 2>       repeats = { false, false };
    int                       chosen  = 0;
    CheckedDisk               disk;
    std::vector<std::string>  lines;



    for (;;)
    {
        m_state         = State::Paused;
        m_screen.quip   = PickQuip();
        m_screen.verify = m_link.IsVerifying();

        hr = m_link.ShowScreen (m_screen);
        CHR (hr);

        lines = GetPauseLines (repeats);
        lines.push_back (FullSendScreen::GetQuipText (m_screen.quip));

        hr = m_link.WaitForKey (lines, key);
        CHR (hr);

        m_screen.quip = 0;
        m_lastSent    = false;

        CBREx (key != FullSendKey::Quit, IFullSendLink::kQuit);

        if (key == FullSendKey::Verify)
        {
            m_link.ToggleVerify();
            continue;
        }

        if (key == FullSendKey::Space)
        {
            repeats = { false, false };

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

                if (disk.check == DiskCheck::Repeat)
                {
                    repeats[(size_t) drive - 1] = true;
                    ReportRepeat (drive, disk);
                }
            }

            continue;
        }

        chosen = key == FullSendKey::Drive1 ? 1 : 2;

        if (repeats[(size_t) chosen - 1])
        {
            hr = CheckDrive (chosen, disk);
            CHR (hr);

            repeats[(size_t) chosen - 1] = false;

            if (disk.check != DiskCheck::None)
            {
                hr = SendDisk (chosen, disk);
                CHR (hr);
                BAIL_OUT_IF (true, S_OK);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::GetPauseLines
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> FullSendSession::GetPauseLines (const std::array<bool, 2> & repeats)
{
    std::vector<std::string>  lines = { "PUT IN A NEW DISK AND PRESS SPACE" };



    if (repeats[0] && repeats[1])
    {
        lines.push_back ("D1 AND D2 MAY ALREADY HAVE BEEN SENT.");
        lines.push_back ("CHECK CASSO EXPLORER FOR DETAILS.");
        lines.push_back ("PRESS 1 OR 2 TO SEND D1 OR D2 ANYWAY.");
    }
    else if (repeats[0] || repeats[1])
    {
        int  drive = repeats[0] ? 1 : 2;

        lines.push_back (std::format ("D{} MAY ALREADY HAVE BEEN SENT.", drive));
        lines.push_back ("CHECK CASSO EXPLORER FOR DETAILS.");
        lines.push_back (std::format ("PRESS {} TO SEND D{} ANYWAY.", drive, drive));
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::ReportRepeat
//
////////////////////////////////////////////////////////////////////////////////

void FullSendSession::ReportRepeat (int drive, const CheckedDisk & disk)
{
    const FullSendRow  * match = m_manifest.FindByFingerprint (disk.fingerprint);
    std::string          file  = match == nullptr ? std::string ("an earlier disk") : match->file.empty() ? match->duplicateOf : match->file;



    m_link.Report (std::format ("The disk in drive {} matches {}. A match compares only a few tracks, so different disks can match. Press {} on the Apple to send it anyway.", drive, file, drive));
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
        for (int line = 1; line <= FullSendScreen::kQuipCount; line++)
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
    std::string                         title            = FullSendDisk::MakeImageTitle (sequence, FullSendDisk::GetCatalogTitle (checked.image), m_prefix);
    int                                 other            = 3 - drive;
    std::string                         fingerprint      = checked.fingerprint;
    std::vector<Byte>                   image            = checked.image;
    std::vector<Byte>                   after;
    std::vector<std::pair<int, int>>    unreadable;
    FullSendReadReport                  report;
    bool                                isProtected      = false;
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
    m_lastSent    = false;
    m_screen.names[(size_t) drive - 1] = GetScreenName (sequence, FullSendDisk::GetCatalogTitle (checked.image));
    m_screen.maps[(size_t) drive - 1].clear();
    SetDriveState (drive, FullSendDriveState::Sending);

    //  A drive's first time on deck has the instruction, and later ones a line.
    if (m_link.HasDrive (other))
    {
        SetDriveState (other, FullSendDriveState::OnDeck);
        m_screen.quip = m_deckShown[(size_t) other - 1] ? PickQuip() : (Byte) 0;
        m_deckShown[(size_t) other - 1] = true;
    }

    hr = m_link.ShowScreen (m_screen);
    CHR (hr);

    m_screen.verify = m_link.IsVerifying();

    hr = m_link.ShowScreen (m_screen);
    CHR (hr);

    hr = m_link.ReadDisk (drive, image, checked.wholeTracks, unreadable, report);
    CHR (hr);

    hr = FullSendDisk::ComputeFingerprint (image, imageFingerprint);
    CHR (hr);

    after = image;

    hr = m_link.ReadTrack (drive, FullSendDisk::IsCatalogOnTrackZero (image) ? 0 : FullSendDisk::kDosCatalogTrack, after, hasDisk, whole);
    CHR (hr);

    hr = FullSendDisk::ComputeFingerprint (after, afterFingerprint);
    CHR (hr);

    //  A sector that reads differently from read to read isn't a swap, which a
    //  verified read has already voted on.
    same          = hasDisk && ((imageFingerprint == fingerprint && afterFingerprint == fingerprint) || !report.marks.empty());
    m_screen.quip = 0;

    if (!same)
    {
        m_changed.insert (fingerprint);
        m_link.Report (std::format ("The disk in drive {} changed while it was read, so no image was written", drive));

        SetDriveState (drive, FullSendDriveState::Idle);
        m_screen.names[(size_t) drive - 1].clear();

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
    row.recovered   = (int) std::count_if (report.marks.begin(), report.marks.end(), [] (const SectorMark & mark) { return mark.mark == 'R'; });
    row.unreliable  = (int) std::count_if (report.marks.begin(), report.marks.end(), [] (const SectorMark & mark) { return mark.mark == 'U'; });
    row.verification = report.tracksVerified == 0                                ? FullSendManifest::kVerifyNone
                     : report.tracksVerified >= NibblizationLayer::kTrackCount ? FullSendManifest::kVerifyFull
                                                                                : FullSendManifest::kVerifyPartial;

    if (earlier != nullptr)
    {
        row.status      = FullSendManifest::kStatusDuplicate;
        row.duplicateOf = earlier->file;
        m_screen.tone   = FullSendTone::Duplicate;
        m_screen.names[(size_t) drive - 1] = std::to_string (sequence);
        m_screen.duplicates++;
    }
    else
    {
        title = FullSendDisk::MakeImageTitle (sequence, FullSendDisk::GetCatalogTitle (image), m_prefix);

        hr = WriteImage (title, image, unreadable, report.marks, row.file);
        CHRF (hr, m_fileFailed = true);

        row.status    = unreadable.empty() ? FullSendManifest::kStatusSent : FullSendDisk::AssessDamage (image, report.emptyTracks, (int) unreadable.size());
        isProtected   = row.status == FullSendManifest::kStatusProtected;
        m_screen.tone = unreadable.empty() ? FullSendTone::Done : isProtected ? FullSendTone::Protected : FullSendTone::BadSectors;
        m_screen.prot += isProtected ? 1 : 0;

        if (isProtected)
        {
            m_link.Report (std::format ("{} is likely copy-protected, so it was read once with no retries. The image may not run. Use a flux tool such as an Applesauce for a working copy.", title));
        }

        m_screen.names[(size_t) drive - 1] = GetScreenName (sequence, FullSendDisk::GetCatalogTitle (image));
        m_screen.sent++;
        m_screen.bad += (unreadable.empty() || isProtected) ? 0 : 1;
        m_screen.recovered += (row.recovered != 0 || row.unreliable != 0) ? 1 : 0;
    }

    m_link.Report (row.file.empty() ? std::format ("Disk {:04} is a duplicate of {}", sequence, row.duplicateOf)
                                    : std::format ("Disk {:04} is {}, with {} unreadable sectors", sequence, row.file, row.unreadable));

    m_manifest.Add (row);

    hr = WriteManifest();
    CHRF (hr, m_fileFailed = true);

    if (!unreadable.empty() || !report.marks.empty())
    {
        std::string  & map = m_screen.maps[(size_t) drive - 1];

        map.assign (NibblizationLayer::kTrackCount, '.');

        //  The worst of a track's sectors.
        for (const SectorMark & mark : report.marks)
        {
            char  & cell = map[(size_t) mark.track];

            cell = (cell == '.' || mark.mark == 'X' || (mark.mark == 'U' && cell == 'R')) ? mark.mark : cell;
        }

        for (const auto & [track, sector] : unreadable)
        {
            map[(size_t) track] = 'X';
        }
    }

    m_lastSent = true;
    SetDriveState (drive, isProtected ? FullSendDriveState::Protected : FullSendDriveState::Sent);

    hr = m_link.ShowScreen (m_screen);
    m_screen.tone = FullSendTone::None;
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::GetScreenName
//
//  The sequence number with no leading zeros and the disk's name, with no
//  prefix, as the prefix is on every disk and the row is short.
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendSession::GetScreenName (int sequence, const std::string & catalogTitle)
{
    std::string  title = FullSendDisk::MakeImageTitle (sequence, catalogTitle, std::string());



    return title.substr (title.find_first_not_of ('0'));
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

HRESULT FullSendSession::WriteImage (const std::string & title, const std::vector<Byte> & image, const std::vector<std::pair<int, int>> & unreadable, const std::vector<SectorMark> & marks, std::string & outFile)
{
    HRESULT               hr        = S_OK;
    std::string           extension = FullSendDisk::GetImageExtension (image, m_extension);
    std::string           text;
    std::vector<Byte>     bytes;
    DiskFormat            format    = DiskFormat::Dsk;
    CommitPlan::Progress  progress;



    hr = DiskImageStore::GetSourceFormatByExtension (extension, format);
    CHR (hr);

    hr = BlankDiskBuilder::WrapInContainer (format, NibbleImageCodec::GetTrackSizeForPath (extension), false, 0, image, bytes);
    CHR (hr);

    outFile = title + extension;

    for (int copy = 2; m_fileIo.Exists (GetPath (outFile)); copy++)
    {
        outFile = std::format ("{} ({}){}", title, copy, extension);
    }

    hr = DurableCommit::Commit (m_fileIo, GetPath (outFile), bytes, m_commitTag++, CommitMode::CreateNew, progress);
    CHR (hr);

    if (!unreadable.empty() || !marks.empty())
    {
        text = CassoLinkDiskTransfer::FormatUnreadableList (unreadable, marks);

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
