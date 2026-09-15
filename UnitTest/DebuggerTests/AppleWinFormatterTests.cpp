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
            jsr.symbol               = "COUT";
            data.lines               = { lda, jsr };
            text                     = Render (data);

            Assert::AreEqual (std::string ("0300: A9 41    LDA  #$41"),         text[0]);
            Assert::AreEqual (std::string ("0302: 20 ED FD JSR  $FDED  ; COUT"), text[1]);
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
    };
}
