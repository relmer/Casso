#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendSession.h"
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
    void     Report         (const std::string & text) override;

    //  The time for a manifest's row: UTC, in ISO 8601.
    static std::string  GetUtcTime ();

private:
    HRESULT  CheckStop     ();
    HRESULT  ReadTrackOnce (int drive, int track, std::vector<Byte> & ioImage, std::vector<std::pair<int, int>> & outUnreadable);

    CassoLinkSession   & m_session;
    LinkOperation      * m_operation = nullptr;
    std::atomic<bool>    m_canceled  { false };
    bool                 m_verifying = false;
    FullSendScreen       m_lastScreen;
};
