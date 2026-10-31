#pragma once

#include "Pch.h"

#include "CassoLink/FullSend/FullSendSession.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"

class CassoLinkSession;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSend
//
//  A full send as one operation on the session's thread: FullSendSession over
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

    bool     HasDrive   (int drive) override;
    HRESULT  ReadTrack  (int drive, int track, std::vector<Byte> & ioImage, bool & outHasDisk, bool & outWhole) override;
    HRESULT  ReadDisk   (int drive, std::vector<Byte> & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable) override;
    HRESULT  ShowScreen (const FullSendScreen & screen) override;
    HRESULT  WaitForKey (FullSendKey & outKey) override;
    void     Report     (const std::string & text) override;

    //  The time for a manifest's row: UTC, in ISO 8601.
    static std::string  GetUtcTime ();

private:
    HRESULT  CheckStop  ();

    CassoLinkSession   & m_session;
    LinkOperation      * m_operation = nullptr;
    std::atomic<bool>    m_canceled  { false };
};
