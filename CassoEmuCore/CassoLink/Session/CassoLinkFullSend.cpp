#include "Pch.h"

#include "CassoLink/Session/CassoLinkFullSend.h"
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
    FullSendSession    session (*this, m_session.m_fileIo, GetUtcTime, operation.imagePath, operation.newName);
    const Byte         off[2]  = { (Byte) FullSendDriveState::Off, (Byte) FullSendDriveState::Off };



    m_canceled  = false;
    m_operation = &operation;

    hr = session.Run();

    operation.message = session.GetMessage();
    m_session.m_sink.OnActivity (operation.message);
    m_operation       = nullptr;

    for (size_t i = 0; i < CassoLinkSessionTypes::kDriveCount; i++)
    {
        m_session.m_disks[i]->Clear();
        m_session.m_listings[i].reset();
    }

    if (!CassoLinkSession::IsLineLost (hr))
    {
        m_session.m_host->SetProgress (0, 100);

        cleared = m_session.m_host->SendSessionScreen (off, 0, 0, 0, (Byte) FullSendTone::None, std::string());
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



    outHasDisk = false;
    outWhole   = false;

    hr = CheckStop();
    CHR (hr);

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    m_session.m_host->SetProgress (0, 100);

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
//  writes the file only when every track has been read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::ReadDisk (int drive, std::vector<Byte> & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable)
{
    HRESULT  hr = S_OK;



    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        hr = CheckStop();
        CHR (hr);

        if (std::find (haveTracks.begin(), haveTracks.end(), track) != haveTracks.end())
        {
            continue;
        }

        m_session.Report (*m_operation, track * 100 / NibblizationLayer::kTrackCount);
        m_session.m_host->SetProgress ((Byte) (track * 100 / NibblizationLayer::kTrackCount), (Byte) ((track + 1) * 100 / NibblizationLayer::kTrackCount));

        hr = m_session.m_diskTransfer.ReadTrack (drive, track, ioImage, outUnreadable);
        CHR (hr);
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
    HRESULT     hr       = S_OK;
    const Byte  drives[] = { (Byte) screen.drives[0], (Byte) screen.drives[1] };



    hr = CheckStop();
    CHR (hr);

    hr = m_session.m_host->SendSessionScreen (drives, (Word) screen.sent, (Word) screen.duplicates, (Word) screen.bad, (Byte) screen.tone, screen.name);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend::WaitForKey
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFullSend::WaitForKey (FullSendKey & outKey)
{
    HRESULT  hr   = S_OK;
    Byte     code = 0;



    hr = CheckStop();
    CHR (hr);

    hr = m_session.m_host->WaitForKey (code);
    CHR (hr);

    outKey = (code == CassoLinkCodec::kKeyQuit) ? FullSendKey::Quit : FullSendKey::Space;

Error:
    return hr;
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
