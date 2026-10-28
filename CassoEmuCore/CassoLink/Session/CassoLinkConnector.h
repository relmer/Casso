#pragma once

#include "Pch.h"

#include "CassoLink/Session/CassoLinkSessionTypes.h"
#include "CassoLink/LinkBootstrap.h"

class IByteStream;
class CassoLinkSession;
struct CassoLinkSettings;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector
//
//  Reaching the Apple: the bootstrap typed or played, the line opened and
//  its failure described, the program awaited and loaded, ADTPro served
//  until a retry, and the peer's loss noticed.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkConnector
{
public:
    explicit CassoLinkConnector (CassoLinkSession & session);

    static constexpr DWORD  kPeerOffMs        = 500;

    HRESULT  StartWithBootstrap (const CassoLinkSettings & settings, std::shared_ptr<LinkOperation> & outOperation);
    HRESULT  Connect            (std::stop_token stop, DWORD openAt = 0);
    HRESULT  RunBootstrap       (std::stop_token stop, LinkOperation & operation);
    HRESULT  OpenStream         (DWORD baud, std::stop_token stop);
    void     ServeUntilRetry    (std::stop_token stop);

    //  An ADTPro-compatible server's whole run: the line opened at the
    //  link's speed and ADTPro's client served on it, opened again after
    //  the retry interval when it fails or drops, until the stop.
    void     ServeAdtPro     (std::stop_token stop);

    static std::string  DescribeOpenFailure (HRESULT hr, uint16_t port);
    bool                IsLineOpen          () const;
    HRESULT             LogOnApple          (const std::string & text);
    HRESULT             AwaitProgram        (std::stop_token stop, DWORD timeoutMs, bool & outReady);
    bool                IsPeerGone          ();

private:
    friend class CassoLinkSession;

    CassoLinkSession  & m_session;

    std::wstring                                m_port;
    std::shared_ptr<IByteStream>                m_stream;
    std::atomic<LinkState>                      m_state            { LinkState::Closed };
    std::atomic<Byte>                           m_driveMap         { 0 };
    std::atomic<bool>                           m_adtPro           { false };
    mutable std::mutex                          m_lock;
    std::condition_variable_any                 m_wake;
    std::deque<std::shared_ptr<LinkOperation>>  m_queue;
    uint64_t                                    m_nextId           = 1;
    DWORD                                       m_connectTimeoutMs = CassoLinkSessionTypes::kConnectTimeoutMs;
    DWORD                                       m_bootstrapPauseMs = LinkBootstrap::kLinePauseMs;
    DWORD                                       m_charPauseUs      = LinkBootstrap::kCharPauseUs;
    DWORD                                       m_retryMs          = CassoLinkSessionTypes::kRetryMs;
    bool                                        m_peerSeen         = false;
    uint64_t                                    m_peerOffSinceMs   = 0;
    DWORD                                       m_openBaud         = 0;
    bool                                        m_prompted         = false;

    //  Why the line was dropped, for the activity text; the session's thread
    //  sets it just before Disconnect.
    std::string                      m_lostReason;
    std::string                      m_openFailure;
    std::wstring                     m_modelName;
    std::shared_ptr<LinkOperation>   m_bootstrap;
    std::jthread                     m_thread;
};
