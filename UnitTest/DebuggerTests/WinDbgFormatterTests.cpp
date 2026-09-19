#include "Pch.h"

#include "Debugger/WinDbgFormatter.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgFormatterTests
    //
    //  Each layout as WinDbg's command reference shows it, with 6502 widths.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgFormatterTests)
    {
    public:
        static std::vector<std::string> Render (const std::string & command, ReplyData data)
        {
            Reply  reply;



            reply.command = command;
            reply.data    = std::move (data);
            WinDbgFormatter::Format (reply);
            return reply.text;
        }

        static MemoryData MakeMemory (Word address, std::vector<Byte> bytes)
        {
            MemoryData  data;
            MemoryRow   row;



            row.address = address;

            for (Byte b : bytes)
            {
                row.bytes.push_back (b);
            }

            data.rows.push_back (row);
            return data;
        }



        TEST_METHOD (Registers_FlagsCaseShowsSetOrClear)
        {
            RegistersData  data;



            data.registers = { 0x0300, 0x41, 0x00, 0x00, 0xF9, 0x81 };

            Assert::AreEqual (std::string ("a=41 x=00 y=00 sp=f9 pc=0300 Nv-bdizC"), Render ("r", data)[0]);
            Assert::AreEqual (std::string ("nv-bdizc"), WinDbgFormatter::FormatFlags (0x20));
        }

        TEST_METHOD (Db_SixteenBytesADashAndTheCharacters)
        {
            std::vector<Byte>          bytes = { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0x60, 0x00, 0x00,
                                                 0x48, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7A };
            std::vector<std::string>   lines = Render ("db 300 l11", MakeMemory (0x0300, bytes));



            Assert::AreEqual ((size_t) 2, lines.size());
            Assert::AreEqual (std::string ("0300  a9 41 8d 00 04 60 00 00-48 49 00 00 00 00 00 00  .A...`..HI......"), lines[0]);
            Assert::AreEqual (std::string ("0310  7a                                               z"), lines[1]);
        }

        TEST_METHOD (DwAndDd_AreLittleEndian)
        {
            MemoryData  data = MakeMemory (0x0300, { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0x60, 0x12, 0x34 });



            Assert::AreEqual (std::string ("0300  41a9 008d 6004 3412"), Render ("dw 300 l4", data)[0]);
            Assert::AreEqual (std::string ("0300  008d41a9 34126004"),   Render ("dd 300 l2", data)[0]);
        }

        TEST_METHOD (Da_StopsAtZero_AndClearsTheHighBit)
        {
            MemoryData  data = MakeMemory (0x0400, { 0xC8, 0xC9, 'x', 0x00, 'y' });



            Assert::AreEqual (std::string ("0400  \"HIx\""), Render ("da 400", data)[0]);
        }

        TEST_METHOD (U_LabelLineThenAddressBytesInstruction)
        {
            DisassemblyData           data;
            DisassemblyLine           line;
            std::vector<std::string>  lines;



            line.label                = "start";
            line.instruction.address  = 0x0300;
            line.instruction.bytes    = { 0xA9, 0x41 };
            line.instruction.mnemonic = "LDA";
            line.instruction.operand  = "#$41";
            data.lines.push_back (line);

            lines = Render ("u 300", data);

            Assert::AreEqual (std::string ("start:"),               lines[0]);
            Assert::AreEqual (std::string ("0300 a941    lda #$41"), lines[1]);
        }

        TEST_METHOD (Bl_OneLineABreakpoint)
        {
            BreakpointListData        data;
            BreakpointInfo            code;
            BreakpointInfo            write;
            std::vector<std::string>  lines;



            code.id           = 0;
            code.address      = 0x0300;
            code.last         = 0x0300;
            write.id          = 1;
            write.kind        = BreakpointKind::Memory;
            write.access      = WatchAccess::Write;
            write.address     = 0xC010;
            write.last        = 0xC010;
            write.enabled     = false;
            data.breakpoints  = { code, write };

            lines = Render ("bl", data);

            Assert::AreEqual (std::string ("0 e 0300 0001 (0001)"),    lines[0]);
            Assert::AreEqual (std::string ("1 d c010 w1 0001 (0001)"), lines[1]);
        }

        TEST_METHOD (K_HeaderFramesAndBreaks)
        {
            CallStackData             data;
            CallStackFrame            frame;
            CallStackRow              row;
            CallStackRow              broken;
            std::vector<std::string>  lines;



            frame.callSite   = 0x0305;
            frame.target     = 0xFDED;
            frame.symbol     = "COUT";
            row.frame        = frame;
            broken.chainBreak = CallStackBreak { CallBreakKind::Txs, 0x0812, 0x9A };
            data.rows        = { row, broken };

            lines = Render ("k", data);

            Assert::AreEqual (std::string ("#  call site  target        how"), lines[0]);
            Assert::IsTrue   (lines[1].starts_with ("00 0305       fded COUT"));
            Assert::IsTrue   (lines[1].find ("recorded") != std::string::npos);
            Assert::IsTrue   (lines[2].starts_with ("-- "));
        }

        TEST_METHOD (Evaluate_AndFormats)
        {
            std::vector<std::string>  lines;



            Assert::AreEqual (std::string ("Evaluate expression: 784 = 0310"), Render ("? 300+10", CalcData { 0x0310 })[0]);

            lines = Render (".formats 41", CalcData { 0x41 });

            Assert::AreEqual (std::string ("Evaluate expression:"),       lines[0]);
            Assert::AreEqual (std::string ("  Hex:     0041"),            lines[1]);
            Assert::AreEqual (std::string ("  Decimal: 65"),              lines[2]);
            Assert::AreEqual (std::string ("  Binary:  00000000 01000001"), lines[3]);
            Assert::AreEqual (std::string ("  Chars:   .A"),              lines[4]);
        }

        //  A kind with no WinDbg layout, and an error, keep the AppleWin text.
        TEST_METHOD (OtherKinds_KeepTheAppleWinText)
        {
            Reply  reply;



            Assert::AreEqual (std::string ("Cycles: 5"), Render ("!budget", CyclesData { 5 })[0]);

            reply.SetError (CommandStatus::NotAvailable, "no meaning on this machine", "lm belongs to WinDbg's modules and symbol paths commands.");
            WinDbgFormatter::Format (reply);
            Assert::AreEqual (std::string ("Error: no meaning on this machine"), reply.text[0]);
        }

        TEST_METHOD (Stop_IsTheAppleWinLineThenTheRegisters)
        {
            StopEvent  stop;



            stop.reason       = StopReason::Breakpoint;
            stop.breakpointId = 0;
            stop.pc           = 0x0300;
            stop.registers    = { 0x0300, 0x41, 0x00, 0x00, 0xFF, 0x30 };

            Assert::AreEqual (std::string ("Breakpoint #0 at $0300\na=41 x=00 y=00 sp=ff pc=0300 nv-Bdizc"), WinDbgFormatter::FormatStop (stop));
        }
    };
}
