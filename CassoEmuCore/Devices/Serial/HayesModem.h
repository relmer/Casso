#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem
//
//  A Hayes-compatible modem's command interpreter, with no I/O.
//  The guest's bytes come in through ReceiveFromGuest with the emulated
//  cycle each arrived at; what the modem sends the guest (echo, result
//  codes) and what it sends the remote end collect for TakeToGuest and
//  TakeToRemote; and dialing, answering and hanging up are requests the
//  attachment takes with TakeLinkRequests and performs on a TCP link,
//  reporting back through OnConnected, OnDialFailed, OnRemoteClosed,
//  OnIncomingCall and OnIncomingGone. Tick runs the timers: the escape's
//  closing guard time, the ring every kRingSeconds, and S7's wait for a
//  connection. Every time is in emulated cycles. A dial string is
//  host[:port], port 23 when none is given.
//
////////////////////////////////////////////////////////////////////////////////

class HayesModem
{
public:
    enum class Mode
    {
        Command,
        Dialing,
        Data,
        OnlineCommand,
    };

    enum class Result
    {
        Ok         = 0,
        Connect    = 1,
        Ring       = 2,
        NoCarrier  = 3,
        Error      = 4,
    };

    enum class LinkRequestKind
    {
        Dial,
        Answer,
        HangUp,
    };

    struct LinkRequest
    {
        LinkRequestKind  kind = LinkRequestKind::HangUp;
        std::string      host;
        uint16_t         port = 0;
    };

    static constexpr uint16_t  kDefaultDialPort   = 23;
    static constexpr size_t    kCommandBufferSize = 40;
    static constexpr size_t    kRegisterCount     = 38;
    static constexpr uint32_t  kRingSeconds       = 6;

    // S-registers the modem reads.
    static constexpr size_t    kRegAutoAnswer     = 0;
    static constexpr size_t    kRegRingCount      = 1;
    static constexpr size_t    kRegEscape         = 2;
    static constexpr size_t    kRegCarriageReturn = 3;
    static constexpr size_t    kRegLineFeed       = 4;
    static constexpr size_t    kRegBackspace      = 5;
    static constexpr size_t    kRegDialToneWait   = 6;
    static constexpr size_t    kRegCarrierWait    = 7;
    static constexpr size_t    kRegCommaPause     = 8;
    static constexpr size_t    kRegCarrierDetect  = 9;
    static constexpr size_t    kRegCarrierLoss    = 10;
    static constexpr size_t    kRegGuardTime      = 12;

    explicit HayesModem (uint32_t cpuClockHz);

    // The 6551's baud rate, for CONNECT's rate with X1 and up.
    void  SetBaudRate (double baud) { m_baud = baud; }

    void  ReceiveFromGuest (Byte value, uint64_t cycle);
    void  Tick             (uint64_t cycle);
    void  OnDtrChanged     (bool isOn, uint64_t cycle);

    void  OnConnected      (uint64_t cycle);
    void  OnDialFailed     ();
    void  OnRemoteClosed   ();
    void  OnIncomingCall   (uint64_t cycle);
    void  OnIncomingGone   ();

    // The line was dropped from outside the modem: the machine resumed from
    // a point in the past. Hangs up, with NO CARRIER when a call is up or
    // the guest had carrier at that point.
    void  DropCarrier      (bool hadCarrier);

    std::vector<Byte>         TakeToGuest      ();
    std::vector<Byte>         TakeToRemote     ();
    std::vector<LinkRequest>  TakeLinkRequests ();

    Mode  GetMode              () const { return m_mode; }
    bool  IsOnline             () const { return m_mode == Mode::Data || m_mode == Mode::OnlineCommand; }
    bool  IsRinging            () const { return m_isRinging; }
    bool  IsDataMode           () const { return m_mode == Mode::Data; }
    bool  GetDcd               () const { return !m_followsCarrier || IsOnline(); }
    bool  DoesDcdFollowCarrier () const { return m_followsCarrier; }
    Byte  GetRegister          (size_t index) const { return m_registers[index]; }

private:
    enum class Prefix
    {
        None,
        SawA,
        InLine,
    };

    void  ResetSettings      ();
    void  ReceiveCommandByte (Byte value, uint64_t cycle);
    void  ReceiveDataByte    (Byte value, uint64_t cycle);
    void  ExecuteLine        (const std::string & line, uint64_t cycle);
    bool  ExecuteCommand     (const std::string & line, size_t & at, uint64_t cycle, bool & outHasResult);
    bool  ExecuteRegister    (const std::string & line, size_t & at);
    bool  ExecuteAmpersand   (const std::string & line, size_t & at);
    bool  Dial               (const std::string & dialString, uint64_t cycle);
    void  Answer             (uint64_t cycle);
    void  HangUp             ();
    void  GoOnline           (uint64_t cycle);
    void  EndCall            (bool sendNoCarrier);
    void  SendResult         (Result result);
    void  SendText           (const std::string & text);
    void  Echo               (Byte value);
    uint64_t  GetGuardCycles () const;
    uint64_t  SecondsToCycles (uint32_t seconds) const;
    bool  IsEscapeEnabled    () const;
    static int  ReadNumber   (const std::string & line, size_t & at, bool & outHasDigits);

    uint32_t                          m_cpuClockHz     = 0;
    double                            m_baud           = 0.0;
    Mode                              m_mode           = Mode::Command;
    std::array<Byte, kRegisterCount>  m_registers      = {};
    bool                              m_isEcho         = true;
    bool                              m_isQuiet        = false;
    bool                              m_isVerbose      = true;
    int                               m_xLevel         = 0;
    bool                              m_followsCarrier = true;     // &C1
    int                               m_dtrMode        = 0;        // &D
    Prefix                            m_prefix         = Prefix::None;
    std::string                       m_line;
    std::string                       m_lastLine;
    bool                              m_isRinging      = false;
    uint64_t                          m_nextRing       = 0;
    uint64_t                          m_dialDeadline   = 0;
    int                               m_escapeCount    = 0;
    uint64_t                          m_lastDataCycle  = 0;
    std::vector<Byte>                 m_toGuest;
    std::vector<Byte>                 m_toRemote;
    std::vector<LinkRequest>          m_requests;
};
