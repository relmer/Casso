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

    static constexpr DWORD    kPeerOffMs      = 500;

    //  A Wi-Fi modem's port not opened because the firewall does not admit it.
    static constexpr HRESULT  kDeferredByFirewall = HRESULT_FROM_WIN32 (ERROR_NETWORK_ACCESS_DENIED);

    //  Typed at the bootstrap speed after Ctrl-X: Applesoft stores $A5 in the
    //  6551 of the slot IN# set, which sends it to the PC. KSW's high byte at
    //  $39 is $Cn for slot n, and the 6551's data register is at $C088 +
    //  n * 16. Under DOS 3.3, KSW is DOS's input routine at $9E81, and the
    //  slot IN# set is in DOS's copy of KSW at $AA55. For any address
    //  outside $C098 to $C0F8, the POKE is skipped: a store anywhere else
    //  can overwrite DOS's code. Each line fits after the prompt on a
    //  40-column row, so the screen scrolls only after a carriage return,
    //  in the pause after each line. After IN#, the //c's serial firmware
    //  doesn't buffer input, and the 6551 has room for one byte, so only
    //  one of the bytes that arrive during a scroll is read. On a real //c,
    //  the first characters typed after a line runs are sometimes dropped,
    //  so each line starts with spaces, which Applesoft ignores. For any
    //  value of S, the POKE is a store to a slot's I/O or is skipped.
    static constexpr const char *  kPromptProbe[]     = { "    S=PEEK(57)\r",
                                                          "    IF S=158 THEN S=PEEK(43606)\r",
                                                          "    S=S*16+46216\r",
                                                          "    IF ABS(S-49352)<49 THEN POKE S,165\r" };
    static constexpr DWORD         kPromptProbeWaitMs = 1500;
    static constexpr DWORD         kSpeedSliceMs      = 2000;   // each speed's turn while a line waits for a Ready at two
    HRESULT  StartWithBootstrap (const CassoLinkSettings & settings, std::shared_ptr<LinkOperation> & outOperation);
    HRESULT  Connect            (std::stop_token stop, DWORD openAt = 0);
    HRESULT  RunBootstrap       (std::stop_token stop, LinkOperation & operation);
    HRESULT  OpenStream         (DWORD baud, std::stop_token stop);

    //  Connect, and with no reply from an Apple on a serial line, the probe:
    //  an Apple waiting at a prompt with IN# set to the port is bootstrapped.
    HRESULT  ConnectOrBootstrap (std::stop_token stop);
    HRESULT  ProbeForPrompt     (std::stop_token stop, bool & outWaiting);    void     ServeUntilRetry    (std::stop_token stop);

    //  An ADTPro-compatible server's whole run: the line opened at the
    //  link's speed and ADTPro's client served on it, opened again after
    //  the retry interval when it fails or drops, until the stop.
    void     ServeAdtPro     (std::stop_token stop);

    static std::string  DescribeOpenFailure (HRESULT hr, uint16_t port, const std::wstring & comPort);
    bool                WasBootstrapped     () const { return m_bootstrapped; }
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
    bool                             m_bootstrapped = false;
    std::wstring                     m_modelName;
    std::shared_ptr<LinkOperation>   m_bootstrap;
    std::jthread                     m_thread;
};
