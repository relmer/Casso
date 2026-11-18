#include "Pch.h"

#include "CassoLink/FullSend/FullSendSession.h"

#include "CassoLink/CassoLinkCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::FullSendSession
//
//  A chosen extension has its leading period; empty is .po for ProDOS and
//  Pascal and .dsk for any other. The status is the Full Send window's
//  model, written as the session goes; nullptr for none.
//
////////////////////////////////////////////////////////////////////////////////

FullSendSession::FullSendSession (IFullSendLink & link, IDiskFileIo & fileIo, ClockFn clock, std::string folder, std::string extension, std::string prefix, FullSendStatus * status) :
    m_unread  ([] () { return 0.0; }, [] () { return std::string(); }),
    m_status  (status != nullptr ? *status : m_unread),
    m_folder  (fileIo, std::move (folder), std::move (extension)),
    m_context (link, m_status, m_folder),
    m_check   (m_context),
    m_send    (m_context, m_check, std::move (clock)),
    m_prefix  (std::move (prefix))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::Run
//
//  Returns when the session ends. Q returns kQuit and Cancel
//  ERROR_CANCELLED; no session ends in S_OK. A manifest that cannot be
//  parsed ends the session before any disk is read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::Run()
{
    HRESULT          hr   = S_OK;
    IFullSendLink  & link = m_context.link;



    m_status.Begin();

    //  A WOZ is built to each track's measured layout, so a title that times
    //  the turn from track to track runs from it.
    link.SetMeasuring (m_folder.GetExtension() == FullSendFolder::kRawExtension);

    hr = m_folder.Load();
    CHR (hr);

    for (int drive : { 1, 2 })
    {
        m_status.SetPresent (drive, link.HasDrive (drive));

        if (!link.HasDrive (drive))
        {
            m_context.screen.SetDriveState (drive, FullSendDriveState::Off);
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
//  FullSendSession::AwaitBegin
//
//  No disk is read until Space, so a disk left in a drive isn't sent before the
//  screen has been read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::AwaitBegin()
{
    HRESULT                   hr    = S_OK;
    IFullSendLink           & link  = m_context.link;
    FullSendKey               key   = FullSendKey::Drive1;
    std::vector<std::string>  lines;



    while (key != FullSendKey::Space)
    {
        lines = { "V: Verify reads each track three times.", "Slower, but catches bad reads.", GetPrefixLine(), "" };

        if (!link.IsBootstrapped())
        {
            lines.push_back ("Remove the CassoLink disk");
        }

        lines.push_back ("Insert disks to be sent into the drives");
        lines.push_back ("");
        lines.push_back ("Press <Space> to begin");

        m_context.screen.verify = link.IsVerifying();
        m_context.screen.prefix = m_prefix;
        m_status.SetActivity (FullSendActivity::Starting);

        hr = m_context.ShowScreen();
        CHR (hr);

        hr = link.WaitForKey (lines, key);
        CHR (hr);

        CBREx (key != FullSendKey::Quit, IFullSendLink::kQuit);

        if (key == FullSendKey::Verify)
        {
            link.ToggleVerify();
        }

        if (key == FullSendKey::Prefix)
        {
            hr = EditPrefix();
            CHR (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::EditPrefix
//
//  The prefix typed on the Apple, as ADTPro takes a file name there. Return
//  takes the line and Esc puts back the prefix as it was; Left arrow or
//  Delete erases a letter.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::EditPrefix()
{
    HRESULT      hr   = S_OK;
    std::string  text = m_prefix;
    Byte         code = 0;
    char         ch   = 0;



    m_status.SetActivity (FullSendActivity::EditingPrefix);

    for (;;)
    {
        hr = m_context.link.WaitForAnyKey ({ "Type a prefix for the images' names.", "Prefix: " + text + "_", "", "Return: use it    Esc: cancel", "<- or Delete: erase a letter" }, code);
        CHR (hr);

        if (code == CassoLinkCodec::kKeyReturn)
        {
            m_prefix = text;
            BAIL_OUT_IF (true, S_OK);
        }

        BAIL_OUT_IF (code == CassoLinkCodec::kKeyEscape, S_OK);

        if (code == CassoLinkCodec::kKeyLeft || code == CassoLinkCodec::kKeyDelete)
        {
            if (!text.empty())
            {
                text.pop_back();
            }

            continue;
        }

        ch = (char) (code & 0x7F);

        if (ch >= ' ' && ch <= '~' && text.size() < kPrefixLength)
        {
            text += ch;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::GetPrefixLine
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendSession::GetPrefixLine() const
{
    return "P: File name prefix: " + (m_prefix.empty() ? std::string ("none") : m_prefix);
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
    HRESULT              hr   = S_OK;
    FullSendCheckedDisk  disk;



    for (int drive : GetOrder())
    {
        if (drive == m_lastRead && m_lastSent)
        {
            continue;
        }

        hr = CheckDrive (drive, disk);
        CHR (hr);

        if (disk.check == FullSendDiskCheck::New)
        {
            hr = SendDisk (drive, disk, FullSendSendMode::New);
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
//  it, even when it is an exact copy.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::Pause()
{
    HRESULT                   hr     = S_OK;
    IFullSendLink           & link   = m_context.link;
    FullSendScreen          & screen = m_context.screen;
    FullSendKey               key    = FullSendKey::Space;
    bool                      sent   = false;
    std::vector<std::string>  lines;
    std::string               retry;



    while (!sent)
    {
        m_context.boots.Apply();

        m_state       = State::Paused;
        m_status.SetActivity (FullSendActivity::WaitingForDisk);
        screen.quip   = m_context.quips.Pick();
        screen.verify = link.IsVerifying();
        screen.prefix = m_prefix;

        hr = m_context.ShowScreen();
        CHR (hr);

        lines = GetPauseLines();
        lines.push_back (FullSendScreen::GetQuipText (screen.quip));

        hr = link.WaitForKey (lines, key);
        CHR (hr);

        screen.quip = 0;
        m_lastSent  = false;

        CBREx (key != FullSendKey::Quit, IFullSendLink::kQuit);

        if (key == FullSendKey::Verify)
        {
            link.ToggleVerify();
            continue;
        }

        if (key == FullSendKey::Prefix)
        {
            hr = EditPrefix();
            CHR (hr);
            continue;
        }

        m_context.boots.ClearLines();
        retry = m_context.boots.TakeRetryPrint();

        if (key == FullSendKey::Space)
        {
            hr = CheckDrives (retry, sent);
            CHR (hr);
            continue;
        }

        hr = SendRepeat (key == FullSendKey::Drive1 ? 1 : 2, sent);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::CheckDrives
//
//  Both drives on Space, the other drive first, and the first new disk sent.
//  The disk whose image did not boot, left in its drive, is read again,
//  once, with retry its fingerprint: a raw capture with the reads of the
//  one before it, and a sector image with verified reads.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::CheckDrives (const std::string & retry, bool & outSent)
{
    HRESULT              hr = S_OK;
    FullSendCheckedDisk  disk;



    outSent = false;

    for (int drive : GetOrder())
    {
        hr = CheckDrive (drive, disk);
        CHR (hr);

        if (disk.check == FullSendDiskCheck::New)
        {
            hr = SendDisk (drive, disk, FullSendSendMode::New);
            CHR (hr);

            outSent = true;
            BAIL_OUT_IF (true, S_OK);
        }

        if (disk.check == FullSendDiskCheck::Repeat && !retry.empty() && disk.fingerprint == retry)
        {
            hr = SendAgain (drive, disk);
            CHR (hr);

            outSent = true;
            BAIL_OUT_IF (true, S_OK);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::SendRepeat
//
//  A drive's number sends the repeat or blank the pause listed for it, once
//  a check finds a disk in the drive.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::SendRepeat (int drive, bool & outSent)
{
    HRESULT              hr       = S_OK;
    bool                 isListed = m_check.IsRepeat (drive);
    FullSendCheckedDisk  disk;



    outSent = false;

    BAIL_OUT_IF (!isListed, S_OK);

    hr = CheckDrive (drive, disk);
    CHR (hr);

    m_check.ClearRepeat (drive);

    BAIL_OUT_IF (disk.check == FullSendDiskCheck::None, S_OK);

    hr = SendDisk (drive, disk, FullSendSendMode::Anyway);
    CHR (hr);

    outSent = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::SendAgain
//
//  A disk whose image did not boot, sent as a new image beside the first. A
//  raw capture is built from its reads and the first capture's. A likely
//  copy-protected disk saved as its sectors, or one whose damage is unclear,
//  is captured raw, because protected sectors read the same each time. A
//  damaged disk is read with verification on, for sectors that read
//  differently each time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::SendAgain (int drive, const FullSendCheckedDisk & disk)
{
    HRESULT              hr            = S_OK;
    IFullSendLink      & link          = m_context.link;
    bool                 wasVerify     = link.IsVerifying();
    const FullSendRow  * first         = m_folder.FindWithImage ([&] (const FullSendRow & row) { return row.fingerprint == disk.fingerprint; });
    bool                 protectedCopy = first != nullptr && first->status == FullSendManifest::kStatusProtected;
    bool                 unclearCopy   = first != nullptr && first->status == FullSendManifest::kStatusUnclear;
    bool                 rawAgain      = protectedCopy || unclearCopy;
    bool                 verify        = !rawAgain && !wasVerify;



    m_context.boots.NoteRetried (disk.fingerprint);
    m_check.ClearRepeat (drive);

    m_context.Report (protectedCopy ? std::format ("The image captured from the disk in drive {} did not boot; the disk is likely copy-protected. Capturing the disk again in raw mode.", drive)
                    : unclearCopy   ? std::format ("The image captured from the disk in drive {} did not boot, and its unreadable sectors may be copy protection. Capturing the disk again in raw mode.", drive)
                                    : std::format ("The image captured from the disk in drive {} did not boot. Capturing the disk again with verified reads.", drive));

    if (verify)
    {
        link.ToggleVerify();
    }

    hr = SendDisk (drive, disk, rawAgain ? FullSendSendMode::RawAgain : FullSendSendMode::Again);

    if (verify && link.IsVerifying())
    {
        link.ToggleVerify();
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::CheckDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::CheckDrive (int drive, FullSendCheckedDisk & outDisk)
{
    HRESULT  hr = S_OK;



    if (m_context.link.HasDrive (drive))
    {
        m_state = State::Checking;
    }

    hr = m_check.Check (drive, outDisk);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::SendDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendSession::SendDisk (int drive, const FullSendCheckedDisk & disk, FullSendSendMode mode)
{
    HRESULT  hr = S_OK;



    m_state    = State::Sending;
    m_lastRead = drive;

    hr = m_send.Send (drive, disk, mode, m_prefix, m_lastSent);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::GetOrder
//
//  The other drive first, and drive 1 at the start.
//
////////////////////////////////////////////////////////////////////////////////

std::array<int, 2> FullSendSession::GetOrder() const
{
    return { m_lastRead == 1 ? 2 : 1, m_lastRead == 1 ? 1 : 2 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::GetPauseLines
//
//  The notes, what to do, and a blank row before the quip.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> FullSendSession::GetPauseLines() const
{
    std::vector<std::string>  lines = m_context.boots.GetLines();
    std::vector<int>          sent;
    bool                      both  = m_check.IsRepeat (1) && m_check.IsRepeat (2);
    int                       drive = m_check.IsRepeat (1) ? 1 : 2;



    for (int d : { 1, 2 })
    {
        if (m_check.IsRepeat (d) && m_check.IsSwapped (d))
        {
            lines.push_back (std::format ("D{} changed while it was read.", d));
        }
        else if (m_check.IsRepeat (d) && m_check.IsBlank (d))
        {
            lines.push_back (std::format ("D{} is blank, so it wasn't sent.", d));
        }
        else if (m_check.IsRepeat (d))
        {
            sent.push_back (d);
        }
    }

    if (!sent.empty())
    {
        lines.push_back (sent.size() == 2 ? std::string ("D1 and D2 may already have been sent.") : std::format ("D{} may already have been sent.", sent[0]));
        lines.push_back ("See Casso Explorer's activity log.");
    }

    if (both)
    {
        lines.insert (lines.end(), { "", "Put in a new disk and press Space,", "or press 1 or 2 to send D1 or D2 anyway.", "" });
    }
    else if (m_check.HasRepeat())
    {
        lines.insert (lines.end(), { "", "Put in a new disk and press Space,", std::format ("or press {} to send D{} anyway.", drive, drive), "" });
    }
    else
    {
        lines.insert (lines.end(), { "Put in a new disk and press Space.", "" });
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendSession::End
//
////////////////////////////////////////////////////////////////////////////////

void FullSendSession::End (HRESULT hr)
{
    std::string     why;
    std::string     model = m_context.link.GetModelName();
    std::string     times;
    FullSendTotals  totals;



    if (m_endReason == EndReason::None)
    {
        m_endReason = m_folder.IsDamaged()                       ? EndReason::ManifestDamaged
                    : hr == IFullSendLink::kQuit                 ? EndReason::Quit
                    : hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED) ? EndReason::Canceled
                    : m_send.HasFileFailed()                     ? EndReason::FileFailed
                                                                 : EndReason::LinkFailed;
    }

    m_state = State::Ended;

    switch (m_endReason)
    {
        case EndReason::Quit:            why = std::format ("Full Send exited on the {}.", model.empty() ? std::string ("Apple") : model);                                    break;
        case EndReason::Canceled:        why = "Full Send session exited from PC.";                                                                                           break;
        case EndReason::ManifestDamaged: why = std::format ("{} could not be read, so no disk was read. Repair or move it, then start again.", FullSendManifest::kFileName); break;
        case EndReason::FileFailed:      why = "A file could not be written to the folder.";                                                                                  break;
        default:                         why = "The link failed.";                                                                                                            break;
    }

    m_status.End (why);

    totals    = m_status.GetSnapshot().totals;
    times     = std::format ("{} active, {} idle", FullSendStatus::FormatClock (totals.elapsed), FullSendStatus::FormatClock (totals.idle));
    times    += totals.sent != 0 ? std::format (", average {} per disk", FullSendStatus::FormatClock (totals.average)) : std::string();
    m_message = std::format ("Full Send ended with {} sent, {} duplicates and {} with bad sectors. {}. {}", m_context.screen.sent, m_context.screen.duplicates, m_context.screen.bad, times, why);
}
