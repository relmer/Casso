#include "Pch.h"

#include "Devices/Serial/HayesModem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::HayesModem
//
////////////////////////////////////////////////////////////////////////////////

HayesModem::HayesModem (uint32_t cpuClockHz)
{
    m_cpuClockHz = cpuClockHz;

    ResetSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ResetSettings
//
//  The factory settings: echo on, results as words, CONNECT without a rate,
//  DCD following the call, a DTR drop hanging up, and the S-registers' usual
//  defaults.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::ResetSettings()
{
    constexpr Byte  kEscape         = '+';
    constexpr Byte  kCarriageReturn = '\r';
    constexpr Byte  kLineFeed       = '\n';
    constexpr Byte  kBackspace      = '\b';
    constexpr Byte  kDialToneWait   = 2;
    constexpr Byte  kCarrierWait    = 50;
    constexpr Byte  kCommaPause     = 2;
    constexpr Byte  kCarrierDetect  = 6;
    constexpr Byte  kCarrierLoss    = 14;
    constexpr Byte  kGuardTime      = 50;
    constexpr int   kHangUpOnDtr    = 2;



    m_registers.fill (0);

    m_registers[kRegEscape]         = kEscape;
    m_registers[kRegCarriageReturn] = kCarriageReturn;
    m_registers[kRegLineFeed]       = kLineFeed;
    m_registers[kRegBackspace]      = kBackspace;
    m_registers[kRegDialToneWait]   = kDialToneWait;
    m_registers[kRegCarrierWait]    = kCarrierWait;
    m_registers[kRegCommaPause]     = kCommaPause;
    m_registers[kRegCarrierDetect]  = kCarrierDetect;
    m_registers[kRegCarrierLoss]    = kCarrierLoss;
    m_registers[kRegGuardTime]      = kGuardTime;

    m_isEcho         = true;
    m_isQuiet        = false;
    m_isVerbose      = true;
    m_xLevel         = 0;
    m_followsCarrier = true;
    m_dtrMode        = kHangUpOnDtr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ReceiveFromGuest
//
//  In data mode a byte goes to the remote end; while dialing any byte but a
//  line feed abandons the call, as on a Hayes; otherwise it is command
//  input.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::ReceiveFromGuest (
    Byte      value,
    uint64_t  cycle)
{
    switch (m_mode)
    {
    case Mode::Data:
        ReceiveDataByte (value, cycle);
        break;

    case Mode::Dialing:
        if (value != m_registers[kRegLineFeed])
        {
            HangUp();
            m_mode = Mode::Command;
            SendResult (Result::NoCarrier);
        }

        break;

    case Mode::Command:
    case Mode::OnlineCommand:
        ReceiveCommandByte (value, cycle);
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ReceiveDataByte
//
//  Counts toward the escape: the first escape character after at least the
//  guard time of silence, then two more each within the guard time of the
//  one before. Any other byte, or one too late, starts the count again. The
//  byte goes to the remote end either way.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::ReceiveDataByte (
    Byte      value,
    uint64_t  cycle)
{
    constexpr int  kEscapeLength = 3;
    uint64_t       silence       = cycle - m_lastDataCycle;
    bool           isEscapeChar  = IsEscapeEnabled() && value == m_registers[kRegEscape];



    if (!isEscapeChar)
    {
        m_escapeCount = 0;
    }
    else if (m_escapeCount == 0)
    {
        m_escapeCount = (silence >= GetGuardCycles()) ? 1 : 0;
    }
    else if (m_escapeCount < kEscapeLength && silence <= GetGuardCycles())
    {
        m_escapeCount++;
    }
    else
    {
        m_escapeCount = 0;
    }

    m_lastDataCycle = cycle;
    m_toRemote.push_back (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ReceiveCommandByte
//
//  A command line starts with AT, in either case, and ends with S3; A/
//  repeats the last line at once. Characters outside a line are echoed and
//  ignored. A line longer than the buffer is an error when it ends.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::ReceiveCommandByte (
    Byte      value,
    uint64_t  cycle)
{
    char  upper = (char) toupper (value);



    Echo (value);

    switch (m_prefix)
    {
    case Prefix::None:
        m_prefix = (upper == 'A') ? Prefix::SawA : Prefix::None;
        break;

    case Prefix::SawA:
        if (upper == 'T')
        {
            m_prefix = Prefix::InLine;
            m_line.clear();
        }
        else if (value == '/')
        {
            m_prefix = Prefix::None;
            ExecuteLine (m_lastLine, cycle);
        }
        else
        {
            m_prefix = (upper == 'A') ? Prefix::SawA : Prefix::None;
        }

        break;

    case Prefix::InLine:
        if (value == m_registers[kRegCarriageReturn])
        {
            m_prefix   = Prefix::None;
            m_lastLine = m_line;
            ExecuteLine (m_line, cycle);
        }
        else if (value == m_registers[kRegBackspace])
        {
            if (!m_line.empty())
            {
                m_line.pop_back();
            }
        }
        else if (m_line.size() <= kCommandBufferSize)
        {
            m_line += (char) value;
        }

        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ExecuteLine
//
//  Runs each command on the line in turn. A command that fails stops the
//  line with ERROR; one that has its own result (CONNECT, NO CARRIER) or
//  starts a dial stops it without OK.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::ExecuteLine (
    const std::string  & line,
    uint64_t             cycle)
{
    size_t  at        = 0;
    bool    isOk      = true;
    bool    hasResult = false;



    if (line.size() > kCommandBufferSize)
    {
        SendResult (Result::Error);
        return;
    }

    while (isOk && !hasResult && at < line.size())
    {
        if (line[at] == ' ')
        {
            at++;
            continue;
        }

        isOk = ExecuteCommand (line, at, cycle, hasResult);
    }

    if (!isOk)
    {
        SendResult (Result::Error);
    }
    else if (!hasResult)
    {
        SendResult (Result::Ok);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ExecuteCommand
//
//  One command at `at`, which is moved past it. False for an unknown
//  command or a bad argument. outHasResult is set when the command sent
//  its own result or left the result for later.
//
////////////////////////////////////////////////////////////////////////////////

bool HayesModem::ExecuteCommand (
    const std::string  & line,
    size_t             & at,
    uint64_t             cycle,
    bool               & outHasResult)
{
    constexpr std::string_view  kIgnored  = "BLMNWY";
    char                        command   = (char) toupper (line[at++]);
    bool                        hasDigits = false;
    bool                        isOk      = true;



    switch (command)
    {
    case 'D':
        isOk         = Dial (line.substr (at), cycle);
        at           = line.size();
        outHasResult = isOk;
        break;

    case 'A':
        Answer (cycle);
        outHasResult = true;
        break;

    case 'O':
        (void) ReadNumber (line, at, hasDigits);

        if (IsOnline())
        {
            m_mode          = Mode::Data;
            m_escapeCount   = 0;
            m_lastDataCycle = cycle;
        }

        SendResult (IsOnline() ? Result::Connect : Result::NoCarrier);
        outHasResult = true;
        break;

    case 'H':
        (void) ReadNumber (line, at, hasDigits);
        HangUp();
        break;

    case 'Z':
        (void) ReadNumber (line, at, hasDigits);
        HangUp();
        ResetSettings();
        break;

    case 'E': m_isEcho    = ReadNumber (line, at, hasDigits) != 0; break;
    case 'Q': m_isQuiet   = ReadNumber (line, at, hasDigits) != 0; break;
    case 'V': m_isVerbose = ReadNumber (line, at, hasDigits) != 0; break;
    case 'X': m_xLevel    = ReadNumber (line, at, hasDigits);      break;

    case 'I':
        (void) ReadNumber (line, at, hasDigits);
        SendText ("Casso modem");
        break;

    case 'S':
        isOk = ExecuteRegister (line, at);
        break;

    case '&':
        isOk = ExecuteAmpersand (line, at);
        break;

    default:
        isOk = kIgnored.find (command) != std::string_view::npos;
        (void) ReadNumber (line, at, hasDigits);
        break;
    }

    return isOk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ExecuteRegister
//
//  Sn=v sets a register and Sn? sends its value as three digits.
//
////////////////////////////////////////////////////////////////////////////////

bool HayesModem::ExecuteRegister (
    const std::string  & line,
    size_t             & at)
{
    constexpr int  kMaxValue = 255;
    bool           hasDigits = false;
    int            index     = ReadNumber (line, at, hasDigits);
    int            value     = 0;
    bool           isOk      = hasDigits && index < (int) kRegisterCount && at < line.size();



    if (!isOk)
    {
        return false;
    }

    if (line[at] == '?')
    {
        at++;
        SendText (std::format ("{:03}", m_registers[(size_t) index]));
        return true;
    }

    if (line[at] != '=')
    {
        return false;
    }

    at++;
    value = ReadNumber (line, at, hasDigits);

    if (!hasDigits || value > kMaxValue)
    {
        return false;
    }

    m_registers[(size_t) index] = (Byte) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ExecuteAmpersand
//
//  &F restores the factory settings, &C selects whether DCD follows the
//  call, &D what a DTR drop does; &K, &S and &W are accepted and ignored.
//
////////////////////////////////////////////////////////////////////////////////

bool HayesModem::ExecuteAmpersand (
    const std::string  & line,
    size_t             & at)
{
    constexpr std::string_view  kIgnored  = "KSW";
    char                        command   = (at < line.size()) ? (char) toupper (line[at++]) : '\0';
    bool                        hasDigits = false;
    int                         number    = ReadNumber (line, at, hasDigits);
    bool                        isOk      = true;



    switch (command)
    {
    case 'F': ResetSettings();                break;
    case 'C': m_followsCarrier = number != 0; break;
    case 'D': m_dtrMode        = number;      break;
    default:  isOk = command != '\0' && kIgnored.find (command) != std::string_view::npos; break;
    }

    return isOk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::Dial
//
//  The dial string is the rest of the line: a T or P right after the D,
//  then host[:port]. A space before the host keeps one that begins with T
//  or P whole. A trailing semicolon, which keeps a Hayes in command mode
//  after dialing, is dropped.
//
////////////////////////////////////////////////////////////////////////////////

bool HayesModem::Dial (
    const std::string  & dialString,
    uint64_t             cycle)
{
    constexpr int  kMaxPort = 65535;
    std::string    target   = dialString;
    size_t         first    = 0;
    size_t         colon    = 0;
    int            port     = kDefaultDialPort;
    char           method   = target.empty() ? '\0' : (char) toupper (target.front());
    LinkRequest    request;



    if (method == 'T' || method == 'P')
    {
        target.erase (0, 1);
    }

    first  = target.find_first_not_of (' ');
    target = (first == std::string::npos) ? std::string() : target.substr (first);

    while (!target.empty() && (target.back() == ' ' || target.back() == ';'))
    {
        target.pop_back();
    }

    colon = target.rfind (':');

    if (colon != std::string::npos)
    {
        port   = atoi (target.c_str() + colon + 1);
        target = target.substr (0, colon);
    }

    if (target.empty() || port < 1 || port > kMaxPort)
    {
        return false;
    }

    request.kind = LinkRequestKind::Dial;
    request.host = target;
    request.port = (uint16_t) port;

    m_requests.push_back (request);
    m_mode         = Mode::Dialing;
    m_isRinging    = false;
    m_dialDeadline = cycle + SecondsToCycles (m_registers[kRegCarrierWait]);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::Answer
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::Answer (uint64_t cycle)
{
    if (!m_isRinging)
    {
        SendResult (Result::NoCarrier);
        return;
    }

    m_requests.push_back ({ LinkRequestKind::Answer, {}, 0 });
    GoOnline (cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::HangUp
//
//  A call up, being dialed or ringing ends; nothing else changes.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::HangUp()
{
    bool  hasCall = IsOnline() || m_mode == Mode::Dialing || m_isRinging;



    if (hasCall)
    {
        m_requests.push_back ({ LinkRequestKind::HangUp, {}, 0 });
    }

    m_mode                      = Mode::Command;
    m_isRinging                 = false;
    m_registers[kRegRingCount]  = 0;
    m_escapeCount               = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::GoOnline
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::GoOnline (uint64_t cycle)
{
    m_mode                     = Mode::Data;
    m_isRinging                = false;
    m_registers[kRegRingCount] = 0;
    m_escapeCount              = 0;
    m_lastDataCycle            = cycle;

    SendResult (Result::Connect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::EndCall
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::EndCall (bool sendNoCarrier)
{
    m_mode        = Mode::Command;
    m_escapeCount = 0;
    m_prefix      = Prefix::None;

    if (sendNoCarrier)
    {
        SendResult (Result::NoCarrier);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::Tick
//
//  The escape completes after the closing guard time; a dial that has not
//  connected in S7 seconds fails; a waiting call rings again, and is
//  answered once S1 reaches a nonzero S0.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::Tick (uint64_t cycle)
{
    constexpr int  kEscapeLength = 3;
    Byte           autoAnswer    = m_registers[kRegAutoAnswer];



    if (m_mode == Mode::Data && m_escapeCount == kEscapeLength && cycle - m_lastDataCycle >= GetGuardCycles())
    {
        m_escapeCount = 0;
        m_mode        = Mode::OnlineCommand;
        m_prefix      = Prefix::None;
        SendResult (Result::Ok);
    }

    if (m_mode == Mode::Dialing && cycle >= m_dialDeadline)
    {
        HangUp();
        SendResult (Result::NoCarrier);
    }

    if (m_isRinging && cycle >= m_nextRing)
    {
        m_nextRing = cycle + SecondsToCycles (kRingSeconds);
        m_registers[kRegRingCount]++;
        SendResult (Result::Ring);

        if (autoAnswer != 0 && m_registers[kRegRingCount] >= autoAnswer)
        {
            m_requests.push_back ({ LinkRequestKind::Answer, {}, 0 });
            GoOnline (cycle);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::OnDtrChanged
//
//  With &D2, DTR turning off ends a call.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::OnDtrChanged (
    bool      isOn,
    uint64_t  cycle)
{
    constexpr int  kHangUpOnDtr = 2;



    (void) cycle;

    if (!isOn && m_dtrMode == kHangUpOnDtr && IsOnline())
    {
        HangUp();
        SendResult (Result::NoCarrier);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::OnConnected
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::OnConnected (uint64_t cycle)
{
    if (m_mode == Mode::Dialing)
    {
        GoOnline (cycle);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::OnDialFailed
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::OnDialFailed()
{
    if (m_mode == Mode::Dialing)
    {
        EndCall (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::OnRemoteClosed
//
//  A call that was up ends with NO CARRIER; a caller who hangs up before
//  the call is answered stops the ringing.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::OnRemoteClosed()
{
    if (IsOnline())
    {
        EndCall (true);
    }

    OnIncomingGone();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::OnIncomingCall
//
//  The first ring is at once.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::OnIncomingCall (uint64_t cycle)
{
    if (m_mode != Mode::Command || m_isRinging)
    {
        return;
    }

    m_isRinging                = true;
    m_registers[kRegRingCount] = 0;
    m_nextRing                 = cycle;

    Tick (cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::OnIncomingGone
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::OnIncomingGone()
{
    m_isRinging                = false;
    m_registers[kRegRingCount] = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::DropCarrier
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::DropCarrier (bool hadCarrier)
{
    bool  wasOnline = IsOnline();



    HangUp();

    if (wasOnline || hadCarrier)
    {
        SendResult (Result::NoCarrier);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::SendResult
//
//  A word between S3 S4 pairs, or a number and S3, unless Q1. CONNECT has
//  the baud rate with X1 and up: as a word, and as the extended codes for
//  the rates a Hayes modem reported.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::SendResult (Result result)
{
    static constexpr std::pair<int, int>  kConnectCodes[] =
    {
        { 300, 1 }, { 1200, 5 }, { 2400, 10 }, { 4800, 11 }, { 9600, 12 },
    };
    std::string  word;
    int          code = (int) result;
    long long    rate = llround (m_baud);



    if (m_isQuiet)
    {
        return;
    }

    switch (result)
    {
    case Result::Ok:        word = "OK";         break;
    case Result::Connect:   word = "CONNECT";    break;
    case Result::Ring:      word = "RING";       break;
    case Result::NoCarrier: word = "NO CARRIER"; break;
    case Result::Error:     word = "ERROR";      break;
    }

    if (result == Result::Connect && m_xLevel > 0 && rate > 0)
    {
        word += std::format (" {}", rate);

        for (const auto & [baud, extended] : kConnectCodes)
        {
            code = (baud == rate) ? extended : code;
        }
    }

    if (m_isVerbose)
    {
        SendText (word);
    }
    else
    {
        SendText (std::format ("{}", code));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::SendText
//
//  A line to the guest in the result format: S3 S4 text S3 S4 when verbose,
//  text S3 when numeric.
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::SendText (const std::string & text)
{
    Byte  cr = m_registers[kRegCarriageReturn];
    Byte  lf = m_registers[kRegLineFeed];



    if (m_isVerbose)
    {
        m_toGuest.push_back (cr);
        m_toGuest.push_back (lf);
    }

    m_toGuest.insert (m_toGuest.end(), text.begin(), text.end());
    m_toGuest.push_back (cr);

    if (m_isVerbose)
    {
        m_toGuest.push_back (lf);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::Echo
//
////////////////////////////////////////////////////////////////////////////////

void HayesModem::Echo (Byte value)
{
    if (m_isEcho)
    {
        m_toGuest.push_back (value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::GetGuardCycles
//
//  S12 is in fiftieths of a second.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HayesModem::GetGuardCycles() const
{
    constexpr uint64_t  kGuardUnitsPerSecond = 50;



    return (uint64_t) m_registers[kRegGuardTime] * m_cpuClockHz / kGuardUnitsPerSecond;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::SecondsToCycles
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HayesModem::SecondsToCycles (uint32_t seconds) const
{
    return (uint64_t) seconds * m_cpuClockHz;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::IsEscapeEnabled
//
//  S2 above 127 or S12 of 0 turns the escape off.
//
////////////////////////////////////////////////////////////////////////////////

bool HayesModem::IsEscapeEnabled() const
{
    constexpr Byte  kMaxEscape = 127;



    return m_registers[kRegEscape] <= kMaxEscape && m_registers[kRegGuardTime] != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::ReadNumber
//
//  The decimal digits at `at`, 0 when there are none.
//
////////////////////////////////////////////////////////////////////////////////

int HayesModem::ReadNumber (
    const std::string  & line,
    size_t             & at,
    bool               & outHasDigits)
{
    constexpr int  kDecimal = 10;
    constexpr int  kCap     = 100000;
    int            number   = 0;



    outHasDigits = false;

    while (at < line.size() && isdigit ((unsigned char) line[at]))
    {
        number       = std::min (kCap, number * kDecimal + (line[at] - '0'));
        outHasDigits = true;
        at++;
    }

    return number;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::TakeToGuest
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> HayesModem::TakeToGuest()
{
    return std::exchange (m_toGuest, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::TakeToRemote
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> HayesModem::TakeToRemote()
{
    return std::exchange (m_toRemote, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  HayesModem::TakeLinkRequests
//
////////////////////////////////////////////////////////////////////////////////

std::vector<HayesModem::LinkRequest> HayesModem::TakeLinkRequests()
{
    return std::exchange (m_requests, {});
}
