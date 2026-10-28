#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugLogDelta.h"
#include "Debugger/IoEventLog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommLane
//
//  One lane of the serial or network log's graph, over a span of cycles: the
//  segments where its level is other than its rest level, each with the
//  words its tip has, and the data a socket sent and received. A segment
//  covers [first, last).
//
////////////////////////////////////////////////////////////////////////////////

struct CommSegment
{
    uint64_t      first = 0;
    uint64_t      last  = 0;
    int           level = 0;
    std::wstring  note;
};

struct CommMark
{
    uint64_t  cycle         = 0;
    bool      isReceived    = false;
    size_t    length        = 0;
    uint64_t  sentTotal     = 0;   // the socket's bytes sent and received up to and with this mark
    uint64_t  receivedTotal = 0;
};

struct CommLane
{
    std::wstring              name;
    int                       restLevel = 1;
    std::wstring              restNote;
    std::vector<CommSegment>  segments;
    std::vector<CommMark>     marks;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraph
//
//  The lanes the serial and network logs draw, from their rows.
//
//  The serial graph has TxD and RxD at bit level, each byte laid out by
//  SerialWaveform from the control and command registers it was sent or
//  received with, at the baud rate its control register gives: a byte sent
//  starts at its row's cycle, and a byte received ends there. The line rests
//  at mark. RTS, CTS, DTR, DSR, DCD and IRQ follow the line levels each row
//  of the port has. Data levels are mark (1) and space (0); control lines
//  are on (1) and off (0).
//
//  The network graph has a lane for each W5100 socket, its level the
//  socket's status from each state change, and a mark for each send and
//  receive with the socket's totals up to it.
//
////////////////////////////////////////////////////////////////////////////////

class CommGraph
{
public:
    enum SerialLane : size_t
    {
        kTxd,
        kRxd,
        kRts,
        kCts,
        kDtr,
        kDsr,
        kDcd,
        kIrq,
        kSerialLaneCount,
    };

    static constexpr size_t    kSocketCount = 4;
    static constexpr uint32_t  kCpuClockHz  = 1022727;

    static void  BuildSerial  (const std::vector<IoLogRow> & rows, Byte port, uint64_t first, uint64_t last, std::vector<CommLane> & outLanes);
    static void  BuildNetwork (const std::vector<IoLogRow> & rows, uint64_t first, uint64_t last, std::vector<CommLane> & outLanes);

    //  Each shown lane's state at `cycle`, a line each: "TxD: space, start bit of $41".
    static std::wstring  GetTip (const std::vector<CommLane> & lanes, const std::vector<bool> & shown, uint64_t cycle);

    static const wchar_t *  GetSerialLaneName (size_t lane);
    static std::wstring     GetSocketStateName (Byte status);

    //  The level a lane's segment covering `cycle` has, or its rest level.
    static const CommSegment *  FindSegment (const CommLane & lane, uint64_t cycle);

private:
    static void  AddBits       (CommLane & lane, Byte value, Byte control, Byte command, uint64_t start, uint64_t first, uint64_t last);
    static void  AddLineLevels (std::vector<CommLane> & lanes, const std::vector<std::pair<uint64_t, Byte>> & changes, Byte initial, uint64_t first, uint64_t last);
    static bool  IsSerial      (IoEventKind kind) { return kind < IoEventKind::SocketState; }
};
