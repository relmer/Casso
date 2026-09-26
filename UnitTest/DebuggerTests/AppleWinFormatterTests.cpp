#include "Pch.h"

#include "Debugger/AppleWinFormatter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatterTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AppleWinFormatterTests)
    {
    public:

        static std::vector<std::string> Render (ReplyData data)
        {
            Reply reply;



            reply.data = std::move (data);
            AppleWinFormatter::Format (reply);
            return reply.text;
        }



        TEST_METHOD (CallStack_AFrameALineAndABreakBetweenDashes)
        {
            CallStackData             data;
            CallStackFrame            inner;
            CallStackFrame            outer;
            CallStackBreak            txs;
            std::vector<std::string>  text;



            inner.callSite   = 0x0806;
            inner.target     = 0x0820;
            inner.symbol     = "THREE";
            outer.callSite   = 0x0800;
            outer.target     = 0x0810;
            outer.provenance = CallProvenance::Guessed;
            outer.isVerified = false;
            txs.kind         = CallBreakKind::Txs;
            txs.pc           = 0x0812;
            data.rows        = { { inner, std::nullopt }, { std::nullopt, txs }, { outer, std::nullopt } };
            text             = Render (data);

            Assert::AreEqual ((size_t) 3, text.size());
            Assert::AreEqual (std::string ("$0806  JSR $0820 THREE        recorded"),              text[0]);
            Assert::AreEqual (std::string ("-- TXS at $0812 --"),                                    text[1]);
            Assert::AreEqual (std::string ("$0800  JSR $0810              guessed, unverified"),   text[2]);
            Assert::AreEqual (std::string ("No calls are on the stack."), Render (CallStackData()).at (0));
            Assert::AreEqual (std::string ("Call stack: WALK"),           Render (CallStackModeData { CallStackMechanism::Walk }).at (0));
        }



        TEST_METHOD (Registers_WithFlags)
        {
            RegistersData             data;
            std::vector<std::string>  text;



            data.registers = { 0x0300, 0x00, 0x00, 0x00, 0xFF, 0x30 };
            text           = Render (data);

            Assert::AreEqual ((size_t) 1, text.size());
            Assert::AreEqual (std::string ("A:00 X:00 Y:00 P:30 S:FF PC:0300  ..RB...."), text[0]);
            Assert::AreEqual (std::string ("NVRBDIZC"), AppleWinFormatter::FormatFlags (0xFF));
            Assert::AreEqual (std::string ("N......C"), AppleWinFormatter::FormatFlags (0x81));
        }



        TEST_METHOD (Memory_EightBytesAndAscii)
        {
            MemoryData                data;
            MemoryRow                 full;
            MemoryRow                 partial;
            std::vector<std::string>  text;



            full.address    = 0x0300;
            full.bytes      = { 0xC8, 0xC5, 0xCC, 0xCC, 0xCF, 0x00, 0x41, 0x7F };
            partial.address = 0xC000;
            partial.bytes   = { std::nullopt, 0x41 };
            data.rows       = { full, partial };
            text            = Render (data);

            Assert::AreEqual (std::string ("0300: C8 C5 CC CC CF 00 41 7F  HELLO.A."), text[0]);
            Assert::AreEqual (std::string ("C000: ?? 41                    .A"),       text[1]);
        }



        TEST_METHOD (Disassembly_Lines)
        {
            DisassemblyData           data;
            DisassemblyLine           lda;
            DisassemblyLine           jsr;
            std::vector<std::string>  text;



            lda.instruction.address  = 0x0300;
            lda.instruction.bytes    = { 0xA9, 0x41 };
            lda.instruction.mnemonic = "LDA";
            lda.instruction.operand  = "#$41";
            jsr.instruction.address  = 0x0302;
            jsr.instruction.bytes    = { 0x20, 0xED, 0xFD };
            jsr.instruction.mnemonic = "JSR";
            jsr.instruction.operand  = "$FDED";
            data.lines               = { lda, jsr };
            text                     = Render (data);

            Assert::AreEqual (std::string ("0300: A9 41    LDA  #$41"),  text[0]);
            Assert::AreEqual (std::string ("0302: 20 ED FD JSR  $FDED"), text[1]);
        }



        TEST_METHOD (Disassembly_OperandSymbolReplacesTheAddress)
        {
            DisassemblyData           data;
            DisassemblyLine           sta;
            std::vector<std::string>  text;



            sta.instruction.address           = 0x0300;
            sta.instruction.bytes             = { 0x91, 0x06 };
            sta.instruction.mnemonic          = "STA";
            sta.instruction.operand           = "($06),Y";
            sta.instruction.hasOperandAddress = true;
            sta.instruction.operandAddress    = 0x0006;
            sta.operandSymbol                 = "PTR";
            data.lines                        = { sta };
            text                              = Render (data);

            Assert::AreEqual (std::string ("0300: 91 06    STA  (PTR),Y"), text[0]);
        }



        TEST_METHOD (Disassembly_LabelsHaveTheirOwnColumn)
        {
            DisassemblyData           data;
            DisassemblyLine           lda;
            DisassemblyLine           jsr;
            std::vector<std::string>  text;



            lda.instruction.address           = 0x0300;
            lda.instruction.bytes             = { 0xA9, 0x41 };
            lda.instruction.mnemonic          = "LDA";
            lda.instruction.operand           = "#$41";
            lda.label                         = "START";
            jsr.instruction.address           = 0x0302;
            jsr.instruction.bytes             = { 0x20, 0xED, 0xFD };
            jsr.instruction.mnemonic          = "JSR";
            jsr.instruction.operand           = "$FDED";
            jsr.instruction.hasOperandAddress = true;
            jsr.instruction.operandAddress    = 0xFDED;
            jsr.operandSymbol                 = "COUT";
            data.lines                        = { lda, jsr };
            text                              = Render (data);

            Assert::AreEqual (std::string ("0300: A9 41    START LDA  #$41"), text[0]);
            Assert::AreEqual (std::string ("0302: 20 ED FD       JSR  COUT"), text[1], L"a line without a label keeps the column");
        }



        TEST_METHOD (Breakpoints_SetAndList)
        {
            BreakpointSetData         set;
            BreakpointListData        list;
            BreakpointInfo            watch;
            BreakpointInfo            cond;



            set.breakpoint.id      = 0;
            set.breakpoint.address = 0x0300;
            set.breakpoint.last    = 0x0300;

            watch.id      = 1;
            watch.kind    = BreakpointKind::Memory;
            watch.address = 0xC019;
            watch.last    = 0xC019;
            watch.access  = WatchAccess::Read;
            watch.hits    = 2;

            cond.id        = 2;
            cond.kind      = BreakpointKind::Register;
            cond.condition = "A=0";
            cond.enabled   = false;

            list.breakpoints = { watch, cond };

            Assert::AreEqual (std::string ("Breakpoint #0 set at $0300"),              Render (set)[0]);
            Assert::AreEqual (std::string ("#1 enabled  on read of $C019, hits 2"),    Render (list)[0]);
            Assert::AreEqual (std::string ("#2 disabled when A=0, hits 0"),            Render (list)[1]);
            Assert::AreEqual (std::string ("No breakpoints."),                         Render (BreakpointListData())[0]);
        }



        TEST_METHOD (Stops)
        {
            StopEvent  write;
            StopEvent  read;
            StopEvent  before;
            StopEvent  breakpoint;
            StopEvent  budget;



            write.reason  = StopReason::Watchpoint;
            write.pc      = 0x0806;
            write.watch   = WatchHit { 1, 0x0400, 0x41, (Byte) 0xA0, WatchAccess::Write, 0x0803, WatchMode::After };

            read          = write;
            read.watch    = WatchHit { 2, 0xC019, 0x80, std::nullopt, WatchAccess::Read, 0x0303, WatchMode::After };

            before        = write;
            before.watch  = WatchHit { 3, 0xC030, 0x00, std::nullopt, WatchAccess::Write, 0x0810, WatchMode::Before };

            breakpoint.reason       = StopReason::Breakpoint;
            breakpoint.pc           = 0x0300;
            breakpoint.breakpointId = 0;

            budget.reason = StopReason::Budget;
            budget.pc     = 0xFCA8;

            Assert::AreEqual (std::string ("Watchpoint #1: Write $41 to $0400 by $0803 (was $A0)"),     AppleWinFormatter::FormatStop (write));
            Assert::AreEqual (std::string ("Watchpoint #2: Read $80 from $C019 by $0303"),             AppleWinFormatter::FormatStop (read));
            Assert::AreEqual (std::string ("Watchpoint #3: write of $C030 by $0810, before the access"), AppleWinFormatter::FormatStop (before));
            Assert::AreEqual (std::string ("Breakpoint #0 at $0300"),                                    AppleWinFormatter::FormatStop (breakpoint));
            Assert::AreEqual (std::string ("Budget at $FCA8"),                                           AppleWinFormatter::FormatStop (budget));
        }



        TEST_METHOD (Stops_RunToAndNegativeCondition)
        {
            StopEvent  runTo;
            StopEvent  negative;



            runTo.reason = StopReason::RunTo;
            runTo.pc     = 0x0310;

            negative.reason         = StopReason::Breakpoint;
            negative.pc             = 0x0300;
            negative.breakpointId   = 1;
            negative.condition      = "A-42";
            negative.conditionValue = -1;

            Assert::AreEqual (std::string ("Stopped at $0310"),                   AppleWinFormatter::FormatStop (runTo));
            Assert::AreEqual (std::string ("Breakpoint #1 at $0300, IF A-42 is -$1"), AppleWinFormatter::FormatStop (negative));
        }



        TEST_METHOD (Errors_TwoLines)
        {
            Reply  unavailable;
            Reply  failed;



            unavailable.status       = CommandStatus::NotAvailable;
            unavailable.error.label  = "command not available";
            unavailable.error.detail = "HGR needs the debugger window.";
            AppleWinFormatter::Format (unavailable);

            failed.status      = CommandStatus::Error;
            failed.error.label = "already running";
            AppleWinFormatter::Format (failed);

            Assert::AreEqual ((size_t) 2, unavailable.text.size());
            Assert::AreEqual (std::string ("Error: command not available"),          unavailable.text[0]);
            Assert::AreEqual (std::string ("       HGR needs the debugger window."), unavailable.text[1]);
            Assert::AreEqual ((size_t) 1, failed.text.size());
        }



        TEST_METHOD (Trace_StateThenOneLinePerEntry)
        {
            TraceData                 data;
            TraceRecord               write;
            TraceRecord               entry;
            std::vector<std::string>  lines;



            write.index         = 41;
            write.cycles        = 5678;
            write.pc            = 0x0302;
            write.opcode        = 0x8D;
            write.op1           = 0x00;
            write.op2           = 0x04;
            write.length        = 3;
            write.instruction   = "STA $0400";
            write.symbol        = "START";
            write.sp            = 0xFF;
            write.p             = 0x30;
            write.hasAccess     = true;
            write.accessIsWrite = true;
            write.accessAddress = 0x0400;
            write.accessData    = 0x05;
            write.accessSymbol  = "SCREEN";
            entry.index         = 42;
            entry.cycles        = 5682;
            entry.pc            = 0x0305;
            entry.opcode        = 0xE8;
            entry.op1           = 0x9A;
            entry.length        = 1;
            entry.instruction   = "INX";
            entry.isInterrupt   = true;
            data.isOn           = false;
            data.total          = 100000;
            data.entries        = { write, entry };

            lines = Render (data);

            Assert::AreEqual ((size_t) 3, lines.size());
            Assert::AreEqual (std::string ("Trace off, 100000 entries retained."), lines[0]);
            Assert::AreEqual (std::string ("    41        5678  0302  8D 00 04  START    STA $0400      A=00 X=00 Y=00 SP=FF ..RB....  W 0400=05 SCREEN"), lines[1]);
            Assert::AreEqual (std::string ("    42        5682  0305  E8                 INX            A=00 X=00 Y=00 SP=00 ........  [interrupt]"),
                              lines[2], L"only the instruction's own byte, not the one after it");
        }


        TEST_METHOD (TraceBytes_AnEntryNotYetDisassembledShowsAllThree)
        {
            TraceRecord  record;



            record.opcode = 0xA9;
            record.op1    = 0x41;
            record.op2    = 0x60;

            Assert::AreEqual (std::string ("A9 41 60"), AppleWinFormatter::FormatTraceBytes (record));
        }



        TEST_METHOD (OtherKinds)
        {
            WatchListData   zp;
            SearchHitsData  hits;
            StackData       stack;
            SoftSwitchData  switches;
            SymbolData      symbols;
            FileIoData      file;



            zp.kind     = WatchListKind::ZeroPage;
            zp.entries  = { { 0, 0x0036, true, (Word) 0xFDF0 } };
            hits.addresses = { 0x0300, 0x0410 };
            stack.sp       = 0xFD;
            stack.entries  = { { 0x01FE, 0x12 }, { 0x01FF, 0x34 } };
            switches.switches = { { "RAMRD", true } };
            symbols.symbols   = { { "HOME", 0xFC58, SymbolTableId::Main } };
            file.path         = "out.bin";
            file.requested    = 16;
            file.transferred  = 12;
            file.mismatch     = true;

            Assert::AreEqual (std::string ("#0 $0036 -> $FDF0"),          Render (zp)[0]);
            Assert::AreEqual (std::string ("No bookmarks."),              Render (WatchListData { WatchListKind::Bookmark, {} })[0]);
            Assert::AreEqual (std::string ("Found 2: $0300 $0410"),       Render (hits)[0]);
            Assert::AreEqual (std::string ("Not found."),                 Render (SearchHitsData())[0]);
            Assert::AreEqual (std::string ("S:FD"),                       Render (stack)[0]);
            Assert::AreEqual (std::string ("01FF: 34"),                   Render (stack)[2]);
            Assert::AreEqual (std::string ("RAMRD      on"),              Render (switches)[0]);
            Assert::AreEqual (std::string ("$FC58 HOME (main)"),          Render (symbols)[0]);
            Assert::AreEqual (std::string ("out.bin: 12 of 16 bytes; the file and the range differ in size"), Render (file)[0]);
            Assert::AreEqual (std::string ("Cycles: 1234"),               Render (CyclesData { 1234 })[0]);
            Assert::AreEqual (std::string ("Mode: MONITOR"),              Render (ModeData { CommandMode::Monitor })[0]);
        }



        TEST_METHOD (HandlerKinds)
        {
            MessageData        message;
            CompareData        compare;
            DataBlockListData  blocks;
            ProfileData        profile;
            BreakpointListData flagged;
            BreakpointInfo     entry;



            message.lines        = { "one", "two" };
            compare.compared     = 16;
            compare.differences  = { { 0x0301, 0x41, 0x0401, 0x42 } };
            blocks.blocks        = { { "B_0300", 0x0300, 0x030F, DataBlockKind::Bytes }, { "T_0800", 0x0800, 0x0803, DataBlockKind::Text } };
            profile.instructions = 10;
            profile.cycles       = 30;
            profile.opcodes      = { { "LDA", "#Immediate", 6, 18 }, { "STA", "Absolute", 4, 10 } };
            profile.pageCross    = 2;
            entry.id             = 3;
            entry.address        = 0x0300;
            entry.last           = 0x0300;
            entry.temporary      = true;
            entry.stops          = false;
            flagged.breakpoints  = { entry };

            Assert::AreEqual ((size_t) 2,                                       Render (message).size());
            Assert::AreEqual (std::string ("two"),                              Render (message)[1]);
            Assert::AreEqual (std::string ("Compared 16 bytes, 1 differ."),     Render (compare)[0]);
            Assert::AreEqual (std::string ("0301: 41  0401: 42"),               Render (compare)[1]);
            Assert::AreEqual (std::string ("B_0300       $0300-$030F  bytes"),  Render (blocks)[0]);
            Assert::AreEqual (std::string ("T_0800       $0800-$0803  text"),   Render (blocks)[1]);
            Assert::AreEqual (std::string ("No data blocks."),                  Render (DataBlockListData())[0]);
            Assert::AreEqual (std::string ("Instructions: 10, cycles: 30"),     Render (profile)[0]);
            Assert::AreEqual (std::string ("Opcode Mode                      Count     Cycles Percent"), Render (profile)[1]);
            Assert::AreEqual (std::string ("LDA    #Immediate                    6         18   60.0%"), Render (profile)[2]);
            Assert::AreEqual (std::string ("Penalty                                    Cycles Percent"), Render (profile)[4]);
            Assert::AreEqual (std::string ("Page crossing                                   2    6.7%"), Render (profile)[5]);
            Assert::AreEqual ((size_t) 8,                                       Render (profile).size(), L"three penalty rows, even when zero");
            Assert::AreEqual ((size_t) 1,                                       Render (ProfileData()).size());
            profile.isByAddress  = true;
            profile.addresses    = { { 0x0300, "LOOP", 30 } };
            Assert::AreEqual (std::string ("Address Symbol                   Cycles Percent"), Render (profile)[1]);
            Assert::AreEqual (std::string ("$0300   LOOP                         30  100.0%"), Render (profile)[2]);
            Assert::AreEqual (std::string ("$0041  0z01000001     65  'A'"),             Render (CalcData { 0x41 })[0]);
            Assert::AreEqual (std::string ("$000D  0z00001101     13  ' ' (Ctrl)"),      Render (CalcData { 0x0D })[0]);
            Assert::AreEqual (std::string ("$00C1  0z11000001    193  'A' (High)"),      Render (CalcData { 0xC1 })[0]);
            Assert::AreEqual (std::string ("$008D  0z10001101    141  ' ' (High Ctrl)"), Render (CalcData { 0x8D })[0]);
            Assert::AreEqual (std::string ("Scanline 42, cycle 17"),            Render (VideoInfoData { 42, 17 })[0]);
            Assert::AreEqual (std::string ("Last branch at $0303"),             Render (BranchRecordData { (Word) 0x0303 })[0]);
            Assert::AreEqual (std::string ("No branch recorded."),              Render (BranchRecordData())[0]);
            Assert::AreEqual (std::string ("#3 enabled  at $0300, temporary, counts only, hits 0"), Render (flagged)[0]);
        }
    };
}
