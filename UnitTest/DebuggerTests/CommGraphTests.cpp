#include "Pch.h"

#include "Devices/Network/W5100Registers.h"
#include "Ui/Debugger/Panes/CommGraph.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommGraphTests
//
//  The serial log's lanes, TxD and RxD at bit level and the control lines
//  from each row's line levels, and the network log's lane for each socket,
//  with the tips each gives at a cycle.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CommGraphTests)
    {
    public:
        static constexpr Byte      kControl  = 0x1E;     // 9600 baud, 8 bits, 1 stop bit
        static constexpr Byte      kCommand  = 0x0B;     // no parity, DTR on
        static constexpr uint64_t  kStart    = 10'000;
        static constexpr uint64_t  kEnd      = 100'000;


        static IoLogRow MakeSerial (IoEventKind kind, uint64_t cycle, Byte value, Byte lines)
        {
            IoLogRow  row;



            row.cycle         = cycle;
            row.event.kind    = kind;
            row.event.cycle   = cycle;
            row.event.value   = value;
            row.event.control = kControl;
            row.event.command = kCommand;
            row.event.lines   = lines;

            return row;
        }


        static double PerBit()
        {
            return CommGraph::kCpuClockHz / 9600.0;
        }


        static std::wstring LaneTip (const std::vector<CommLane> & lanes, size_t lane, uint64_t cycle)
        {
            std::vector<bool>  shown (lanes.size(), false);



            shown[lane] = true;

            return CommGraph::GetTip (lanes, shown, cycle);
        }


        TEST_METHOD (AByteSentIsItsBitsOnTxdFromItsCycle)
        {
            std::vector<CommLane>  lanes;
            uint64_t               start = 20'000;



            CommGraph::BuildSerial ({ MakeSerial (IoEventKind::SerialSent, start, 'A', 0) }, 0, kStart, kEnd, lanes);

            Assert::AreEqual ((size_t) 10, lanes[CommGraph::kTxd].segments.size(), L"start, eight data bits, stop");
            Assert::AreEqual (start, lanes[CommGraph::kTxd].segments[0].first);
            Assert::AreEqual (0, lanes[CommGraph::kTxd].segments[0].level, L"the start bit is space");
            Assert::IsTrue   (LaneTip (lanes, CommGraph::kTxd, start + (uint64_t) (PerBit() / 2)).ends_with (L"TxD: space, start bit of $41"));
            Assert::IsTrue   (LaneTip (lanes, CommGraph::kTxd, start + (uint64_t) (1.5 * PerBit())).ends_with (L"TxD: mark, data bit 0 of $41"));
            Assert::IsTrue   (LaneTip (lanes, CommGraph::kTxd, start - 10).ends_with (L"TxD: mark"), L"the line rests at mark");
        }


        TEST_METHOD (AByteReceivedEndsAtItsCycle)
        {
            std::vector<CommLane>  lanes;
            uint64_t               end   = 50'000;



            CommGraph::BuildSerial ({ MakeSerial (IoEventKind::SerialReceived, end, 'A', 0) }, 0, kStart, kEnd, lanes);

            Assert::AreEqual ((size_t) 10, lanes[CommGraph::kRxd].segments.size());
            Assert::IsTrue   ((int64_t) lanes[CommGraph::kRxd].segments.back().last - (int64_t) end <= 1, L"the stop bit ends at the row's cycle");
        }


        TEST_METHOD (TheControlLinesFollowTheLineLevels)
        {
            std::vector<CommLane>  lanes;
            Byte                   on    = IoEventLog::kLineDtr | IoEventLog::kLineRts;



            CommGraph::BuildSerial ({ MakeSerial (IoEventKind::SerialLines, 20'000, 0, on),
                                      MakeSerial (IoEventKind::SerialLines, 40'000, 0, 0) }, 0, kStart, kEnd, lanes);

            Assert::IsTrue (LaneTip (lanes, CommGraph::kDtr, 30'000).ends_with (L"DTR: on"));
            Assert::IsTrue (LaneTip (lanes, CommGraph::kDtr, 50'000).ends_with (L"DTR: off"));
            Assert::IsTrue (LaneTip (lanes, CommGraph::kCts, 30'000).ends_with (L"CTS: off"));
        }


        TEST_METHOD (AnotherPortsRowsAreLeftOut)
        {
            std::vector<CommLane>  lanes;
            IoLogRow               row   = MakeSerial (IoEventKind::SerialSent, 20'000, 'A', 0);



            row.event.chip = 1;
            CommGraph::BuildSerial ({ row }, 0, kStart, kEnd, lanes);

            Assert::IsTrue (lanes[CommGraph::kTxd].segments.empty());
        }


        TEST_METHOD (ASocketsLaneHasItsStatesAndTotals)
        {
            std::vector<CommLane>  lanes;
            IoLogRow               open;
            IoLogRow               connected;
            IoLogRow               sent;



            open.event      = { .cycle = 20'000, .kind = IoEventKind::SocketState, .chip = 0, .reg = W5100Registers::kSockClosed, .value = W5100Registers::kSockInit };
            connected.event = { .cycle = 30'000, .kind = IoEventKind::SocketState, .chip = 0, .reg = W5100Registers::kSockInit,   .value = W5100Registers::kSockEstablished };
            sent.event      = { .cycle = 40'000, .kind = IoEventKind::SocketSent,  .chip = 0, .length = 5 };

            CommGraph::BuildNetwork ({ open, connected, sent }, kStart, kEnd, lanes);

            Assert::AreEqual ((size_t) CommGraph::kSocketCount, lanes.size());
            Assert::IsTrue   (LaneTip (lanes, 0, 15'000).ends_with (L"Socket 0: closed"));
            Assert::IsTrue   (LaneTip (lanes, 0, 25'000).ends_with (L"Socket 0: TCP, open"));
            Assert::IsTrue   (LaneTip (lanes, 0, 45'000).ends_with (L"Socket 0: TCP, connected; 5 bytes sent, 0 received"));
            Assert::AreEqual ((size_t) 1, lanes[0].marks.size());
        }
    };
}
