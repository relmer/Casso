#include "Pch.h"

#include "Ui/Debugger/Panes/CommGraph.h"
#include "Debugger/SerialWaveform.h"
#include "Devices/Acia6551.h"
#include "Devices/Network/W5100Registers.h"
#include "Ui/Debugger/DebuggerStatusText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph::BuildSerial
//
//  The line levels at `first` are the last row's before it, or the first
//  row's when none is before it.
//
////////////////////////////////////////////////////////////////////////////////

void CommGraph::BuildSerial (
    const std::vector<IoLogRow>  & rows,
    Byte                           port,
    uint64_t                       first,
    uint64_t                       last,
    std::vector<CommLane>        & outLanes)
{
    std::vector<std::pair<uint64_t, Byte>>  changes;
    std::optional<Byte>                     initial;
    double                                  bits    = 0.0;
    double                                  perBit  = 0.0;
    uint64_t                                length  = 0;
    size_t                                  lane    = 0;



    outLanes.assign (kSerialLaneCount, CommLane());

    for (lane = 0; lane < kSerialLaneCount; lane++)
    {
        outLanes[lane].name      = GetSerialLaneName (lane);
        outLanes[lane].restLevel = (lane == kTxd || lane == kRxd) ? 1 : 0;
        outLanes[lane].restNote  = (lane == kTxd || lane == kRxd) ? L"mark" : L"off";
    }

    for (const IoLogRow & row : rows)
    {
        const IoEvent  & event = row.event;

        if (row.kind != DebugLogRowKind::Event || !IsSerial (event.kind) || event.chip != port)
        {
            continue;
        }

        if (event.cycle < first)
        {
            initial = event.lines;
        }
        else if (event.cycle < last)
        {
            initial = initial.value_or (event.lines);
            changes.push_back ({ event.cycle, event.lines });
        }

        if (event.kind == IoEventKind::SerialSent)
        {
            AddBits (outLanes[kTxd], event.value, event.control, event.command, event.cycle, first, last);
        }
        else if (event.kind == IoEventKind::SerialReceived)
        {
            bits   = SerialWaveform::GetCharacterBits (event.control, event.command);
            perBit = (double) kCpuClockHz / Acia6551::GetBaudRate (event.control & Acia6551::kControlBaudMask);
            length = (uint64_t) std::llround (bits * perBit);

            AddBits (outLanes[kRxd], event.value, event.control, event.command, (event.cycle > length) ? event.cycle - length : 0, first, last);
        }
    }

    AddLineLevels (outLanes, changes, initial.value_or (0), first, last);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph::AddBits
//
//  One byte's bits on a data lane, those inside [first, last).
//
////////////////////////////////////////////////////////////////////////////////

void CommGraph::AddBits (
    CommLane  & lane,
    Byte        value,
    Byte        control,
    Byte        command,
    uint64_t    start,
    uint64_t    first,
    uint64_t    last)
{
    std::vector<SerialBit>  bits;
    double                  perBit = (double) kCpuClockHz / Acia6551::GetBaudRate (control & Acia6551::kControlBaudMask);
    uint64_t                from   = 0;
    uint64_t                to     = 0;
    std::wstring            what;



    SerialWaveform::LayOut (value, control, command, bits);

    for (const SerialBit & bit : bits)
    {
        from = start + (uint64_t) std::llround (bit.start * perBit);
        to   = start + (uint64_t) std::llround ((bit.start + bit.length) * perBit);

        if (to <= first || from >= last)
        {
            continue;
        }

        switch (bit.kind)
        {
        case SerialBit::Kind::Start:  what = L"start bit";                            break;
        case SerialBit::Kind::Data:   what = std::format (L"data bit {}", bit.index); break;
        case SerialBit::Kind::Parity: what = L"parity bit";                           break;
        case SerialBit::Kind::Stop:   what = L"stop bit";                             break;
        }

        lane.segments.push_back ({ from, to, bit.isMark ? 1 : 0, std::format (L"{}, {} of ${:02X}", bit.isMark ? L"mark" : L"space", what, value) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph::AddLineLevels
//
//  A segment for each stretch a control line is on, from the level at
//  `first` through each change to `last`. A line on at `first` was on before
//  it, so its stretch begins a cycle before the span and has no edge there.
//
////////////////////////////////////////////////////////////////////////////////

void CommGraph::AddLineLevels (
    std::vector<CommLane>                         & lanes,
    const std::vector<std::pair<uint64_t, Byte>>  & changes,
    Byte                                            initial,
    uint64_t                                        first,
    uint64_t                                        last)
{
    static constexpr std::pair<size_t, Byte>  kBits[] =
    {
        { kRts, IoEventLog::kLineRts },
        { kCts, IoEventLog::kLineCts },
        { kDtr, IoEventLog::kLineDtr },
        { kDsr, IoEventLog::kLineDsr },
        { kDcd, IoEventLog::kLineDcd },
        { kIrq, IoEventLog::kLineIrq },
    };



    for (const auto & [lane, bit] : kBits)
    {
        bool      isOn  = (initial & bit) != 0;
        uint64_t  since = (first > 0) ? first - 1 : 0;

        for (const auto & [cycle, lines] : changes)
        {
            bool  isNowOn = (lines & bit) != 0;

            if (isNowOn == isOn)
            {
                continue;
            }

            if (isOn && cycle > since)
            {
                lanes[lane].segments.push_back ({ since, cycle, 1, L"on" });
            }

            isOn  = isNowOn;
            since = cycle;
        }

        if (isOn && last > since)
        {
            lanes[lane].segments.push_back ({ since, last, 1, L"on" });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph::BuildNetwork
//
//  A socket's status before its first change is the old status that change
//  has; the totals count every row, so a mark's are the socket's since the
//  oldest row.
//
////////////////////////////////////////////////////////////////////////////////

void CommGraph::BuildNetwork (
    const std::vector<IoLogRow>  & rows,
    uint64_t                       first,
    uint64_t                       last,
    std::vector<CommLane>        & outLanes)
{
    std::array<std::optional<Byte>, kSocketCount>  status   = {};
    std::array<uint64_t, kSocketCount>             since    = {};
    std::array<uint64_t, kSocketCount>             sent     = {};
    std::array<uint64_t, kSocketCount>             received = {};
    size_t                                         socket   = 0;
    auto                                           close    = [&] (size_t s, uint64_t at)
    {
        uint64_t  from = std::max (since[s], first);
        uint64_t  to   = std::min (at, last);

        if (status[s].has_value() && to > from)
        {
            outLanes[s].segments.push_back ({ from, to, *status[s], GetSocketStateName (*status[s]) });
        }
    };



    outLanes.assign (kSocketCount, CommLane());

    for (socket = 0; socket < kSocketCount; socket++)
    {
        outLanes[socket].name      = std::format (L"Socket {}", socket);
        outLanes[socket].restLevel = W5100Registers::kSockClosed;
        outLanes[socket].restNote  = GetSocketStateName (W5100Registers::kSockClosed);
    }

    for (const IoLogRow & row : rows)
    {
        const IoEvent  & event = row.event;

        socket = event.chip;

        if (row.kind != DebugLogRowKind::Event || IsSerial (event.kind) || socket >= kSocketCount)
        {
            continue;
        }

        if (event.kind == IoEventKind::SocketState)
        {
            if (!status[socket].has_value())
            {
                status[socket] = event.reg;
                since[socket]  = 0;
            }

            close (socket, event.cycle);

            status[socket] = event.value;
            since[socket]  = event.cycle;
            continue;
        }

        if (event.kind == IoEventKind::SocketSent)
        {
            sent[socket] += event.length;
        }
        else
        {
            received[socket] += event.length;
        }

        outLanes[socket].marks.push_back ({ event.cycle, event.kind == IoEventKind::SocketReceived, event.length, sent[socket], received[socket] });
    }

    for (socket = 0; socket < kSocketCount; socket++)
    {
        close (socket, last);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph::FindSegment
//
////////////////////////////////////////////////////////////////////////////////

const CommSegment * CommGraph::FindSegment (
    const CommLane  & lane,
    uint64_t          cycle)
{
    auto  after = std::upper_bound (lane.segments.begin(), lane.segments.end(), cycle,
                                    [] (uint64_t c, const CommSegment & segment) { return c < segment.first; });



    if (after == lane.segments.begin())
    {
        return nullptr;
    }

    --after;

    return (cycle < after->last) ? &*after : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph::GetTip
//
//  A network lane adds the socket's totals at the last mark at or before
//  the cycle.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommGraph::GetTip (
    const std::vector<CommLane>  & lanes,
    const std::vector<bool>      & shown,
    uint64_t                       cycle)
{
    std::wstring  text = std::format (L"Cycle {}", DebuggerStatusText::FormatCount (cycle));



    for (size_t i = 0; i < lanes.size(); i++)
    {
        const CommLane     & lane    = lanes[i];
        const CommSegment  * segment = FindSegment (lane, cycle);
        const CommMark     * mark    = nullptr;

        if (i < shown.size() && !shown[i])
        {
            continue;
        }

        text += std::format (L"\n{}: {}", lane.name, (segment != nullptr) ? segment->note : lane.restNote);

        for (const CommMark & each : lane.marks)
        {
            if (each.cycle > cycle)
            {
                break;
            }

            mark = &each;
        }

        if (mark != nullptr)
        {
            text += std::format (L"; {} bytes sent, {} received", DebuggerStatusText::FormatCount (mark->sentTotal), DebuggerStatusText::FormatCount (mark->receivedTotal));
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph::GetSerialLaneName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CommGraph::GetSerialLaneName (size_t lane)
{
    static constexpr const wchar_t * kNames[] = { L"TxD", L"RxD", L"RTS", L"CTS", L"DTR", L"DSR", L"DCD", L"IRQ" };



    return (lane < std::size (kNames)) ? kNames[lane] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph::GetSocketStateName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommGraph::GetSocketStateName (Byte status)
{
    using R = W5100Registers;

    switch (status)
    {
    case R::kSockClosed:      return L"closed";
    case R::kSockInit:        return L"TCP, open";
    case R::kSockListen:      return L"TCP, listening";
    case R::kSockSynSent:     return L"TCP, connecting";
    case R::kSockSynReceived: return L"TCP, accepting";
    case R::kSockEstablished: return L"TCP, connected";
    case R::kSockFinWait:
    case R::kSockClosing:
    case R::kSockTimeWait:
    case R::kSockCloseWait:
    case R::kSockLastAck:     return L"TCP, closing";
    case R::kSockUdp:         return L"UDP";
    case R::kSockIpRaw:       return L"IP raw";
    case R::kSockMacRaw:      return L"MAC raw";
    default:                  return std::format (L"${:02X}", status);
    }
}
