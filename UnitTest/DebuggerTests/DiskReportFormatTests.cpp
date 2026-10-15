#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/GSSquaredFormatter.h"
#include "Debugger/MonitorFormatter.h"
#include "Debugger/ReplyJson.h"
#include "Debugger/WinDbgFormatter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReportFormatTests
//
//  Disk stops and disk breakpoints in every output: AppleWin's text, which
//  Casso's and GSSquared's stops use, the Monitor's and WinDbg's, each
//  mode's listing, and the debug channel's JSON. The sector read is the
//  contract's: DOS 3.3's RWTS reading track $11 sector 0.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiskReportFormatTests)
    {
    public:

        static StopEvent MakeSectorReadStop()
        {
            StopEvent       stop;
            DiskReport      report;
            DiskFieldEvent  field;



            field.kind                 = DiskFieldEventKind::DataChecksum;
            field.track                = 0x11;
            field.sector               = 0x00;
            field.volume               = 0xFE;
            field.storedChecksum       = 0xEF;
            field.expectedChecksum     = 0xEF;
            field.storedDataChecksum   = 0x2C;
            field.expectedDataChecksum = 0x2C;
            field.isDataChecksumGood   = true;
            field.addressInstruction   = 0xB94F;
            field.dataInstruction      = 0xB8E1;

            report.breakpointId = 3;
            report.event        = DiskEvent::SectorRead;
            report.slot         = 6;
            report.drive        = 0;
            report.quarterTrack = 0x44;
            report.isMotorOn    = true;
            report.instruction  = 0xB925;
            report.field        = field;
            report.epilogue     = { 0xDE, 0xAA, 0xEB };

            stop.reason       = StopReason::Disk;
            stop.pc           = 47400;
            stop.breakpointId = 3;
            stop.disk         = report;
            return stop;
        }

        static StopEvent MakeRwtsBesideABreakpoint()
        {
            StopEvent        stop;
            DiskReport       report;
            DiskCallRequest  request;



            request.slot    = 6;
            request.drive   = 1;
            request.track   = 0x11;
            request.sector  = 0x05;
            request.command = 1;
            request.buffer  = 0x3000;

            report.breakpointId = 1;
            report.event        = DiskEvent::RwtsCall;
            report.slot         = 6;
            report.request      = request;
            report.vector       = 0x03D9;
            report.instruction  = 0x2000;

            stop.reason       = StopReason::Breakpoint;
            stop.pc           = 0x2000;
            stop.breakpointId = 0;
            stop.disk         = report;
            return stop;
        }

        static BreakpointInfo MakeRwtsInfo (std::optional<Word> resolved)
        {
            BreakpointInfo  info;
            DiskBreakSpec   spec;



            spec.event    = DiskEvent::RwtsCall;
            spec.track    = 0x11;
            spec.command  = 1;
            info.kind     = BreakpointKind::Disk;
            info.disk     = spec;
            info.resolved = resolved;
            return info;
        }

        static std::vector<std::string> Lines (const std::string & text)
        {
            std::vector<std::string>  lines;



            for (size_t start = 0, end = 0; start <= text.size(); start = end + 1)
            {
                end = std::min (text.find ('\n', start), text.size());
                lines.push_back (text.substr (start, end - start));
            }

            return lines;
        }

        static std::wstring Wide (const std::string & text)
        {
            return TextEncoding::NarrowToWide (text);
        }


        TEST_METHOD (TheSectorReadStopIsTheContracts)
        {
            std::vector<std::string>  lines = Lines (AppleWinFormatter::FormatStop (MakeSectorReadStop()));



            Assert::AreEqual ((size_t) 3, lines.size());
            Assert::AreEqual (std::string ("Disk breakpoint #3: sector read, track $11 (17) sector $00 volume $FE, drive 1, 16-sector"), lines[0]);
            Assert::AreEqual (std::string ("  address checksum $EF, expected $EF; data checksum $2C, expected $2C; epilogue DE AA EB"), lines[1]);
            Assert::AreEqual (std::string ("  head on track 17 (quarter track $44), motor on; read by $B925, address field found by $B94F, data field by $B8E1"), lines[2]);
        }

        //  A breakpoint's stop at the routine is its own line, with the call's
        //  report after it (FR-153).
        TEST_METHOD (AnotherStopHasTheReportAfterItsLine)
        {
            std::vector<std::string>  lines = Lines (AppleWinFormatter::FormatStop (MakeRwtsBesideABreakpoint()));



            Assert::AreEqual (std::string ("Breakpoint #0 at $2000"), lines.at (0));
            Assert::AreEqual (std::string ("Disk breakpoint #1: RWTS call, track $11 sector $05 READ, drive 1"), lines.at (1));
            Assert::IsTrue   (lines.at (2).find ("track $11 sector $05 command $01 buffer $3000; through $03D9") != std::string::npos, Wide (lines.at (2)).c_str());
            Assert::IsTrue   (lines.at (3).ends_with ("; routine at $2000"), Wide (lines.at (3)).c_str());
        }

        TEST_METHOD (TheMonitorsStopIsItsRegistersThenTheReport)
        {
            std::vector<std::string>  lines = Lines (MonitorFormatter::FormatStop (MakeSectorReadStop()));



            Assert::IsTrue   (lines.at (0).starts_with ("A="), Wide (lines.at (0)).c_str());
            Assert::IsTrue   (lines.at (1).starts_with ("Disk breakpoint #3: sector read"));
            Assert::AreEqual ((size_t) 4, lines.size());
        }

        TEST_METHOD (WinDbgsStopIsAppleWinsWithRegisters)
        {
            std::string  text = WinDbgFormatter::FormatStop (MakeSectorReadStop());



            Assert::IsTrue (text.starts_with (AppleWinFormatter::FormatStop (MakeSectorReadStop()) + "\n"));
        }

        //  Each listing has the event and "unresolved" while the vector has no
        //  target, and the routine once it has.
        TEST_METHOD (EachListingHasTheEvent)
        {
            Reply  winDbg;
            Reply  gssquared;



            winDbg.data    = BreakpointListData { { MakeRwtsInfo (std::nullopt) } };
            gssquared.data = BreakpointListData { { MakeRwtsInfo (0x2000) } };
            WinDbgFormatter::Format    (winDbg);
            GSSquaredFormatter::Format (gssquared);

            Assert::AreEqual (std::string ("0 e disk 0001 (0001)  RWTS call, track $11, read, unresolved"), winDbg.text.at (0));
            Assert::AreEqual (std::string ("[0] disk RWTS call, track $11, read at $2000"), gssquared.text.at (1));
            Assert::AreEqual (std::string ("on disk RWTS call, track $11, read, unresolved"), AppleWinFormatter::DescribeBreakpoint (MakeRwtsInfo (std::nullopt)));
        }

        TEST_METHOD (TheJsonStopIsTheContracts)
        {
            std::string  json = ReplyJson::WriteStopped (MakeSectorReadStop(), std::nullopt);



            Assert::IsTrue (json.find ("\"reason\":\"disk\",\"pc\":47400,\"breakpointId\":3,") != std::string::npos, Wide (json).c_str());
            Assert::IsTrue (json.find ("\"disk\":{\"event\":\"sectorRead\",\"slot\":6,\"drive\":1,\"quarterTrack\":68,\"motorOn\":true,\"instruction\":47397,"
                                       "\"field\":{\"format\":16,\"track\":17,\"sector\":0,\"volume\":254,\"storedChecksum\":239,\"expectedChecksum\":239,"
                                       "\"storedDataChecksum\":44,\"expectedDataChecksum\":44,\"dataChecksumGood\":true,\"addressInstruction\":47439,"
                                       "\"dataInstruction\":47329,\"passed\":false},\"epilogue\":[222,170,235],\"epilogueRandom\":false}") != std::string::npos,
                           Wide (json).c_str());
        }

        TEST_METHOD (TheJsonCallStopHasTheRequestAndVector)
        {
            std::string  json = ReplyJson::WriteStopped (MakeRwtsBesideABreakpoint(), std::nullopt);



            Assert::IsTrue (json.find ("\"reason\":\"breakpoint\"") != std::string::npos);
            Assert::IsTrue (json.find ("\"request\":{\"slot\":6,\"drive\":1,\"volume\":0,\"track\":17,\"sector\":5,\"command\":1,\"buffer\":12288},\"vector\":985") != std::string::npos,
                            Wide (json).c_str());
        }

        TEST_METHOD (TheJsonBreakpointHasItsDefinition)
        {
            Reply        unresolved;
            Reply        resolved;
            std::string  json;



            unresolved.data = BreakpointSetData { MakeRwtsInfo (std::nullopt) };
            resolved.data   = BreakpointSetData { MakeRwtsInfo (0x2000) };

            json = ReplyJson::WriteReply (unresolved, std::nullopt);
            Assert::IsTrue (json.find ("\"kind\":\"disk\",\"disk\":{\"event\":\"rwtsCall\",\"track\":17,\"command\":1,\"unresolved\":true}") != std::string::npos,
                            Wide (json).c_str());

            json = ReplyJson::WriteReply (resolved, std::nullopt);
            Assert::IsTrue (json.find ("\"disk\":{\"event\":\"rwtsCall\",\"track\":17,\"command\":1},\"resolved\":8192") != std::string::npos, Wide (json).c_str());
        }
    };
}
