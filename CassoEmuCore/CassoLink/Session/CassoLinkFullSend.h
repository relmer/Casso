#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendSession.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"

class CassoLinkSession;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend
//
//  A Full Send as one operation on the session's thread: FullSendSession over
//  the link. A flag here has Cancel between requests, as the host clears its
//  flag when its next request starts.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkFullSend : public IFullSendLink
{
public:
    explicit CassoLinkFullSend (CassoLinkSession & session);

    HRESULT  Run           (LinkOperation & operation);
    void     RequestCancel () { m_canceled = true; }

    bool     HasDrive       (int drive) override;
    HRESULT  ReadTrack      (int drive, int track, std::vector<Byte> & ioImage, bool & outHasDisk, bool & outWhole) override;
    HRESULT  ReadDisk       (int drive, std::vector<Byte> & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable, FullSendReadReport & outReport) override;
    HRESULT  ShowScreen     (const FullSendScreen & screen) override;
    HRESULT  WaitForKey     (const std::vector<std::string> & lines, FullSendKey & outKey) override;
    HRESULT  WaitForAnyKey  (const std::vector<std::string> & lines, Byte & outCode) override;
    bool     IsVerifying    () override { return m_verifying; }
    void     ToggleVerify   () override { m_verifying = !m_verifying; }
    bool     IsBootstrapped () override;
    HRESULT  CaptureRaw     (int drive, bool withEarlier, std::vector<Byte> & outWoz, std::vector<Byte> & outReads, std::string & outMarks) override;
    bool     NoteImage      (const std::string & path) override;
    HRESULT  AwaitBoot      (const std::string & file) override;
    bool     HasBootTones   () override;
    void     Report         (const std::string & text) override;

    //  The time for a manifest's row: UTC, in ISO 8601.
    static std::string  GetUtcTime ();

    //  The Full Send window's model of this connection's session.
    const FullSendStatus &  GetStatus () const { return m_status; }
    FullSendStatus       &  GetStatus ()       { return m_status; }

private:
    HRESULT  CheckStop     ();
    void     NoteCapture   (int drive, const CassoLinkCapture::Progress & where);
    HRESULT  ReadTrackOnce (int drive, int track, std::vector<Byte> & ioImage, std::vector<std::pair<int, int>> & outUnreadable);

    static double       GetSeconds   ();
    static std::string  GetClockTime ();

    static constexpr DWORD  s_kBootWaitMs = 90000;
    static constexpr DWORD  s_kBootPollMs = 100;

    CassoLinkSession   & m_session;
    LinkOperation      * m_operation = nullptr;
    std::atomic<bool>    m_canceled  { false };
    bool                 m_verifying = false;
    bool                 m_barFull   = false;   // the next request draws the drive's bar full
    FullSendScreen       m_lastScreen;
    FullSendStatus       m_status    { GetSeconds, GetClockTime };

    //  The reads of each drive's latest raw capture, for a capture of the
    //  same disk again.
    std::array<std::vector<CassoLinkCapture::Position>, 2>  m_lastReads;
};
