#include "Pch.h"

#include "CassoLink/Session/CassoLinkFullSend.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "CassoLink/CassoLinkSession.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::CassoLinkFullSend
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkFullSend::CassoLinkFullSend (CassoLinkSession & session)
    : m_session (session)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::Run
//
//  Q ends the session as Cancel does. The Apple's screen is cleared on
//  the way out unless the line is gone, and the drives' sectors and listings
//  are dropped, as the disks in them have changed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::Run (LinkOperation & operation)
{
    HRESULT            hr      = S_OK;
    HRESULT            cleared = S_OK;
    FullSendSession    session (*this, m_session.m_fileIo, GetUtcTime, operation.imagePath, operation.newName, operation.imagePrefix, &m_status);



    m_canceled  = false;
    m_operation = &operation;
    m_verifying = operation.verifyReads;

    hr = session.Run();

    operation.message         = session.GetMessage();
    operation.imagePrefix     = session.GetPrefix();
    operation.fullSendSummary = std::format ("{} SENT, {} BAD, {} DUP", session.GetScreen().sent, session.GetScreen().bad, session.GetScreen().duplicates);
    m_session.m_sink.OnActivity (operation.message);
    m_operation       = nullptr;

    for (size_t i = 0; i < CassoLinkSessionTypes::kDriveCount; i++)
    {
        m_session.m_disks[i]->Clear();
        m_session.m_listings[i].reset();
    }

    if (!CassoLinkSession::IsLineLost (hr))
    {
        m_session.m_host->SetProgress (0, 0);

        cleared = m_session.m_host->SendSessionScreen (false, (Byte) FullSendTone::None, {});
        IGNORE_RETURN_VALUE (cleared, S_OK);
    }

    if (hr == kQuit)
    {
        hr = HRESULT_FROM_WIN32 (ERROR_CANCELLED);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::CheckStop
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::CheckStop()
{
    HRESULT  hr      = S_OK;
    bool     quit    = m_session.m_host->TakeQuit();



    CBREx (!quit,       kQuit);
    CBREx (!m_canceled, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

    //  V takes effect at the next track, and the screen says so at once.
    if (m_session.m_host->TakeVerifyToggle())
    {
        m_verifying         = !m_verifying;
        m_lastScreen.verify = m_verifying;

        hr = m_session.m_host->SendSessionScreen (true, (Byte) FullSendTone::None, m_lastScreen.GetRows());
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::HasDrive
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkFullSend::HasDrive (int drive)
{
    return (m_session.GetDriveMap() & (1 << (drive - 1))) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::ReadTrack
//
//  A drive with no disk fails the whole track's request after several
//  revolutions, as a damaged track does, but a damaged track returns the
//  sectors it has. Those that are missing are then requested again as for
//  Read disk to image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::ReadTrack (int drive, int track, std::vector<Byte> & ioImage, bool & outHasDisk, bool & outWhole)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  physicals;
    std::vector<Byte>  data;
    std::vector<bool>  read;
    CassoLinkStatus    status;
    size_t             to        = 0;



    outHasDisk                    = false;
    outWhole                      = false;
    m_operation->drive            = drive;
    m_operation->fullSendChecking = true;

    m_session.Report (*m_operation, 0);

    hr = CheckStop();
    CHR (hr);

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    //  No share of the operation, so the drive's bar is left as it is, except
    //  just after a disk's read.
    if (!m_barFull)
    {
        m_session.m_host->SetProgress (0, 0);
    }

    m_barFull = false;

    hr = m_session.m_host->ReadSectors ((Byte) drive, (Byte) track, physicals, data, read, status);
    CHR (hr);

    BAIL_OUT_IF (status.error == CassoLinkHost::kErrorDrive, S_OK);

    outWhole = status.error == CassoLinkHost::kErrorNone;

    hr = CassoLinkSession::FromStatusError (status.error);
    BAIL_OUT_IF (!outWhole && !CassoLinkDiskTransfer::IsDamaged (status.error), hr);

    hr = S_OK;

    outHasDisk = std::find (read.begin(), read.end(), true) != read.end();
    BAIL_OUT_IF (!outHasDisk, S_OK);

    hr = CassoLinkDiskTransfer::ReadMissing (*m_session.m_host, (Byte) drive, (Byte) track, physicals, data, read, status);
    CHR (hr);

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        if (read[(size_t) physical])
        {
            to = (size_t) (track * NibblizationLayer::kSectorsPerTrack + CassoLinkSession::GetLogicalSector (physical)) * LinkedDisk::kSectorSize;

            std::copy_n (data.begin() + (ptrdiff_t) ((size_t) physical * LinkedDisk::kSectorSize), LinkedDisk::kSectorSize, ioImage.begin() + (ptrdiff_t) to);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::ReadDisk
//
//  Read disk to image's track reads, with an unreadable sector listed and left
//  as zeros, and the check for Q and Cancel between tracks. The session
//  writes the file only when every track has been read. The operation's drive
//  is the one being read, for the status bar.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::ReadDisk (int drive, std::vector<Byte> & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable, FullSendReadReport & outReport)
{
    HRESULT  hr      = S_OK;
    size_t   before  = 0;
    size_t   marked  = 0;
    bool     stop    = false;
    int      damaged = 0;



    m_operation->drive            = drive;
    m_operation->fullSendChecking = false;

    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        hr = CheckStop();
        CHR (hr);

        //  A track the check read once still has its three reads when verifying.
        if (!m_verifying && std::find (haveTracks.begin(), haveTracks.end(), track) != haveTracks.end())
        {
            continue;
        }

        m_session.Report (*m_operation, track * 100 / NibblizationLayer::kTrackCount);
        m_session.m_host->SetProgress ((Byte) (track * 100 / NibblizationLayer::kTrackCount), (Byte) ((track + 1) * 100 / NibblizationLayer::kTrackCount));

        before = outUnreadable.size();
        marked = outReport.marks.size();

        //  With no file system and a track with unreadable sectors, the rest is
        //  read once. Retries don't recover a copy-protected disk's nonstandard
        //  tracks, and a protected track can have a few readable sectors among
        //  its others, so it is never whole and never wholly empty.
        stop = damaged >= FullSendDisk::kDamagedTracksToStop && VolumeImage::DetectFilesystem (ioImage) == VolumeKind::Unknown;

        if (stop)
        {
            hr = ReadTrackOnce (drive, track, ioImage, outUnreadable);
        }
        else if (m_verifying)
        {
            hr = m_session.m_diskTransfer.ReadVerifiedTrack (drive, track, ioImage, outUnreadable, outReport.marks);
            outReport.tracksVerified++;
        }
        else
        {
            hr = m_session.m_diskTransfer.ReadTrack (drive, track, ioImage, outUnreadable, m_operation->autoVerify, &outReport.marks);
        }

        CHR (hr);

        m_status.MarkTrack (drive, track, outUnreadable.size() > before     ? FullSendTrackMark::Unreadable
                                        : outReport.marks.size() > marked   ? FullSendTrackMark::Unreliable
                                        : m_verifying                       ? FullSendTrackMark::Verified
                                                                            : FullSendTrackMark::Read);

        outReport.emptyTracks += outUnreadable.size() - before == NibblizationLayer::kSectorsPerTrack ? 1 : 0;
        damaged               += outUnreadable.size() > before ? 1 : 0;
    }

    //  The last track's share ends at 100 only when its request succeeds, so a
    //  disk whose last track has errors would be left short of it. The read
    //  after this one draws the bar full.
    m_session.m_host->SetProgress (100, 100);
    m_barFull = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::ReadTrackOnce
//
//  One request for the track, with no retries. Sectors not read are left
//  as zeros and listed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::ReadTrackOnce (int drive, int track, std::vector<Byte> & ioImage, std::vector<std::pair<int, int>> & outUnreadable)
{
    HRESULT            hr        = S_OK;
    std::vector<Byte>  physicals;
    std::vector<Byte>  data;
    std::vector<bool>  read;
    CassoLinkStatus    status;



    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    hr = m_session.m_host->ReadSectors ((Byte) drive, (Byte) track, physicals, data, read, status);
    CHR (hr);

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        int  logical = CassoLinkSession::GetLogicalSector (physical);

        if (read[(size_t) physical])
        {
            std::copy_n (data.begin() + (ptrdiff_t) ((size_t) physical * LinkedDisk::kSectorSize), LinkedDisk::kSectorSize,
                         ioImage.begin() + (ptrdiff_t) ((size_t) (track * NibblizationLayer::kSectorsPerTrack + logical) * LinkedDisk::kSectorSize));
        }
        else
        {
            outUnreadable.push_back ({ track, logical });
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::ShowScreen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::ShowScreen (const FullSendScreen & screen)
{
    HRESULT         hr    = S_OK;
    FullSendScreen  shown = screen;



    m_lastScreen      = screen;
    m_lastScreen.tone = FullSendTone::None;

    hr = CheckStop();
    CHR (hr);

    m_lastScreen.verify = m_verifying;

    shown.verify = m_verifying;

    hr = m_session.m_host->SendSessionScreen (true, (Byte) screen.tone, shown.GetRows());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::WaitForKey
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::WaitForKey (const std::vector<std::string> & lines, FullSendKey & outKey)
{
    HRESULT  hr   = S_OK;
    Byte     code = 0;



    hr = CheckStop();
    CHR (hr);

    m_operation->fullSendWaiting = true;
    m_session.Report (*m_operation, m_operation->progress);

    hr = m_session.m_host->WaitForKey (lines, code);
    CHR (hr);

    outKey = code == CassoLinkCodec::kKeyQuit  ? FullSendKey::Quit
           : code == CassoLinkCodec::kKeyDrive1 ? FullSendKey::Drive1
           : code == CassoLinkCodec::kKeyDrive2 ? FullSendKey::Drive2
           : code == CassoLinkCodec::kKeyVerify ? FullSendKey::Verify
           : code == CassoLinkCodec::kKeyPrefix ? FullSendKey::Prefix
                                                : FullSendKey::Space;

Error:
    m_operation->fullSendWaiting = false;
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::WaitForAnyKey
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::WaitForAnyKey (const std::vector<std::string> & lines, Byte & outCode)
{
    HRESULT  hr = S_OK;



    hr = CheckStop();
    CHR (hr);

    hr = m_session.m_host->WaitForAnyKey (lines, outCode);
    CHR (hr);

Error:
    return hr;
}




//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkFullSend::IsBootstrapped()
{
    return m_session.m_connector.WasBootstrapped();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::CaptureRaw
//
//  The status bar has the capture's track, pass and time left, as for
//  Capture raw disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::CaptureRaw (int drive, std::vector<Byte> & outWoz, std::vector<Byte> & outReads, std::string & outMarks)
{
    HRESULT                   hr     = S_OK;
    std::vector<Byte>         module;
    CassoLinkCapture::Result  result;
    std::string               date   = std::format ("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds> (std::chrono::system_clock::now()));



    outWoz.clear();
    outReads.clear();
    outMarks.clear();
    m_operation->drive = drive;

    hr = CheckStop();
    CHR (hr);

    hr = m_session.m_host->BuildCaptureModule (module);
    CHR (hr);

    hr = CassoLinkCapture::Capture (*m_session.m_host, (Byte) drive, module, date,
                                    [this, drive] (const CassoLinkCapture::Progress & where) { NoteCapture (drive, where); }, result);
    CHR (hr);

    //  The second pass ends at the last position it read, so the positions
    //  after that one are marked here.
    for (int position = 0; position < FullSendDriveStatus::kPositions; position++)
    {
        if (m_status.GetSnapshot().drives[(size_t) drive - 1].positions[(size_t) position] == FullSendPositionMark::Unread)
        {
            m_status.MarkPosition (drive, position, FullSendPositionMark::Skipped);
        }
    }

    for (int position : result.weak)
    {
        m_status.MarkPosition (drive, position, FullSendPositionMark::Weak);
    }

    for (const auto & [position, sector] : result.replaced)
    {
        m_status.AddLine (std::format ("Track {} sector {} was taken from another read", CassoLinkDiskTransfer::FormatPosition (position), sector));
    }

    for (int position : result.unstable)
    {
        m_status.MarkPosition (drive, position, FullSendPositionMark::Unstable);
    }

    outMarks = CassoLinkCapture::FormatMarks (result);

    if (m_session.m_sink.HasFullSendReadsOn())
    {
        CassoLinkCapture::SerializeReads (result.positions, outReads);
    }

    m_status.SetCaptureTime (drive, result.diskMs / 1000.0, result.lineMs / 1000.0);
    m_operation->SetDetail (std::string());
    m_session.m_sink.OnActivity (CassoLinkDiskTransfer::FormatCaptureTimes (result));

    outWoz = std::move (result.woz);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::NoteImage
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFullSend::NoteImage (const std::string & path)
{
    m_session.m_sink.OnImageWritten (path, TextEncoding::WideToNarrow (m_session.GetModelName()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::NoteCapture
//
//  A capture's progress to the status bar and the Full Send window. In the
//  second pass, a position below the one read that has no read was skipped.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFullSend::NoteCapture (int drive, const CassoLinkCapture::Progress & where)
{
    m_session.m_diskTransfer.ReportCapture (*m_operation, where);
    m_status.SetCapture (drive, where.pass, where.position, where.read, where.reads, where.percent, where.secondsLeft);

    if (where.pass == 2 && where.read == 1)
    {
        FullSendSnapshot  snapshot = m_status.GetSnapshot();

        for (int position = 0; position < where.position; position++)
        {
            if (snapshot.drives[(size_t) drive - 1].positions[(size_t) position] == FullSendPositionMark::Unread)
            {
                m_status.MarkPosition (drive, position, FullSendPositionMark::Skipped);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::GetSeconds
//
////////////////////////////////////////////////////////////////////////////////

double CassoLinkFullSend::GetSeconds()
{
    return (double) GetTickCount64() / 1000.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::GetClockTime
//
//  The time of an activity line, as 14:05:09.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkFullSend::GetClockTime()
{
    SYSTEMTIME  now = {};



    ::GetLocalTime (&now);

    return std::format ("{:02}:{:02}:{:02}", now.wHour, now.wMinute, now.wSecond);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::Report
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFullSend::Report (const std::string & text)
{
    m_session.m_sink.OnActivity (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::GetUtcTime
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkFullSend::GetUtcTime()
{
    SYSTEMTIME  now = {};



    GetSystemTime (&now);

    return std::format ("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
}
