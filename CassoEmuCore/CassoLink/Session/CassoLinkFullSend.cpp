#include "Pch.h"

#include "CassoLink/Session/CassoLinkFullSend.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "CassoLink/CassoLinkSession.h"
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
    FullSendSession    session (*this, m_session.m_fileIo, GetUtcTime, operation.imagePath, operation.newName, operation.imagePrefix);



    m_canceled  = false;
    m_operation = &operation;
    m_verifying = operation.verifyReads;

    hr = session.Run();

    operation.message         = session.GetMessage();
    operation.imagePrefix     = session.GetPrefix();
    operation.fullSendSummary = std::format ("{} SENT, {} DUP, {} BAD", session.GetScreen().sent, session.GetScreen().duplicates, session.GetScreen().bad);
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
//  revolutions, as a damaged track does. Reading two sectors alone separates the
//  two cases at a fraction of the cost of all sixteen, and a damaged track is then
//  read as for Read disk to image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::ReadTrack (int drive, int track, std::vector<Byte> & ioImage, bool & outHasDisk, bool & outWhole)
{
    constexpr std::array<Byte, 2>     kProbes = { 0, 8 };
    HRESULT                           hr      = S_OK;
    std::vector<Byte>                 physicals;
    std::vector<Byte>                 data;
    std::vector<std::pair<int, int>>  unreadable;
    CassoLinkStatus                   status;
    size_t                            to      = 0;



    outHasDisk         = false;
    outWhole           = false;
    m_operation->drive = drive;

    hr = CheckStop();
    CHR (hr);

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    //  No share of the operation, so the drive's bar is left as it is.
    m_session.m_host->SetProgress (0, 0);

    hr = m_session.m_host->ReadSectors ((Byte) drive, (Byte) track, physicals, data, status);
    CHR (hr);

    BAIL_OUT_IF (status.error == CassoLinkHost::kErrorDrive, S_OK);

    if (status.error == CassoLinkHost::kErrorNone)
    {
        for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
        {
            to = (size_t) (track * NibblizationLayer::kSectorsPerTrack + CassoLinkSession::GetLogicalSector (physical)) * LinkedDisk::kSectorSize;

            std::copy_n (data.begin() + (ptrdiff_t) ((size_t) physical * LinkedDisk::kSectorSize), LinkedDisk::kSectorSize, ioImage.begin() + (ptrdiff_t) to);
        }

        outHasDisk = true;
        outWhole   = true;
        BAIL_OUT_IF (true, S_OK);
    }

    hr = CassoLinkSession::FromStatusError (status.error);
    BAIL_OUT_IF (status.error != CassoLinkHost::kErrorNotFound && status.error != CassoLinkHost::kErrorData, hr);

    hr = S_OK;

    for (Byte probe : kProbes)
    {
        const Byte  one[] = { probe };

        hr = m_session.m_host->ReadSectors ((Byte) drive, (Byte) track, one, data, status);
        CHR (hr);

        outHasDisk = outHasDisk || status.error == CassoLinkHost::kErrorNone;
    }

    BAIL_OUT_IF (!outHasDisk, S_OK);

    hr = m_session.m_diskTransfer.ReadTrack (drive, track, ioImage, unreadable);
    CHR (hr);

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
    HRESULT  hr     = S_OK;
    size_t   before = 0;
    bool     stop   = false;



    m_operation->drive = drive;

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

        //  With no file system and whole tracks gone, the rest is read once.
        //  Retries don't recover a copy-protected disk's nonstandard tracks.
        stop = outReport.emptyTracks >= FullSendDisk::kEmptyTracksToStop && VolumeImage::DetectFilesystem (ioImage) == VolumeKind::Unknown;

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

        outReport.emptyTracks += outUnreadable.size() - before == NibblizationLayer::kSectorsPerTrack ? 1 : 0;
    }

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
    CassoLinkStatus    status;
    int                done      = 0;



    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    hr = m_session.m_host->ReadSectors ((Byte) drive, (Byte) track, physicals, data, status);
    CHR (hr);

    done = status.error == CassoLinkHost::kErrorNone ? NibblizationLayer::kSectorsPerTrack : status.done;

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        int  logical = CassoLinkSession::GetLogicalSector (physical);

        if (physical < done)
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
