#include "Pch.h"

#include "Debugger/GSSquaredFormatter.h"
#include "EmuTests/FixtureProvider.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GSSquaredFormatterTests
    //
    //  Each reply kind GSSquared has a layout for, rendered and compared line
    //  for line with its fixture under Fixtures/Debugger/GSSquared, which
    //  records GSSquared's own layout (see the LICENSE note there for how).
    //  The data each test builds is the data the fixture was written for.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (GSSquaredFormatterTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }

        //  The fixture's lines. The file's last newline ends its last line
        //  rather than starting another, so a blank last line is written as
        //  an empty line before it.
        static std::vector<std::string> ReadFixture (const std::string & name)
        {
            FixtureProvider           provider;
            std::vector<uint8_t>      bytes;
            std::vector<std::string>  lines;
            std::string               text;
            size_t                    start = 0;
            HRESULT                   hr    = provider.OpenFixture ("Debugger/GSSquared/" + name, bytes);



            Assert::AreEqual (S_OK, hr, Widen ("fixture missing: " + name).c_str());

            text.assign (bytes.begin(), bytes.end());
            std::erase (text, '\r');
            Assert::IsFalse (text.empty(), Widen ("fixture empty: " + name).c_str());

            while (start < text.size())
            {
                size_t  end = text.find ('\n', start);

                end = (end == std::string::npos) ? text.size() : end;
                lines.push_back (text.substr (start, end - start));
                start = end + 1;
            }

            return lines;
        }

        static void AssertMatchesFixture (Reply & reply, const std::string & name)
        {
            std::vector<std::string>  expected = ReadFixture (name);



            GSSquaredFormatter::Format (reply);

            for (size_t i = 0; i < std::max (expected.size(), reply.text.size()); ++i)
            {
                std::string  want = i < expected.size()   ? expected[i]   : "<end>";
                std::string  got  = i < reply.text.size() ? reply.text[i] : "<end>";

                Assert::AreEqual (Widen (want), Widen (got), Widen (std::format ("{} line {}", name, i + 1)).c_str());
            }
        }

        static MemoryData MakeMemory (Word first, const std::vector<Byte> & bytes)
        {
            static constexpr size_t  kRowBytes = 8;
            MemoryData               data;



            for (size_t i = 0; i < bytes.size(); i += kRowBytes)
            {
                MemoryRow  row;

                row.address = (Word) (first + i);

                for (size_t j = i; j < std::min (bytes.size(), i + kRowBytes); ++j)
                {
                    row.bytes.push_back (bytes[j]);
                }

                data.rows.push_back (row);
            }

            return data;
        }

        static DisassemblyLine MakeInstruction (Word address, std::vector<Byte> bytes, const char * mnemonic, const char * operand)
        {
            DisassemblyLine  line;



            line.instruction.address  = address;
            line.instruction.bytes    = std::move (bytes);
            line.instruction.mnemonic = mnemonic;
            line.instruction.operand  = operand;
            line.label                = "START";
            return line;
        }



        TEST_METHOD (Examine_OneByte)
        {
            Reply  reply;



            reply.verb = DebugVerb::DumpMemory;
            reply.data = MakeMemory (0x300, { 0xAD });
            AssertMatchesFixture (reply, "examine.txt");
        }

        //  Rows arrive eight bytes long, as AppleWin's D makes them; GSSquared
        //  prints sixteen a line from the first address.
        TEST_METHOD (Dump_SixteenALine_WithCharacters_AndABlankLine)
        {
            Reply  reply;



            reply.verb = DebugVerb::DumpMemory;
            reply.data = MakeMemory (0x300, { 0xAD, 0x19, 0xC0, 0x4C, 0x00, 0x03, 0xC8, 0xC5, 0xCC, 0xCC, 0xCF, 0xA0,
                                              0xD7, 0xCF, 0xD2, 0xCC, 0xC4, 0x00 });
            AssertMatchesFixture (reply, "dump.txt");
        }

        TEST_METHOD (List_WidthsThenInstructions_WithNoLabels)
        {
            Reply            reply;
            DisassemblyData  data;



            data.lines.push_back (MakeInstruction (0x300, { 0xAD, 0x19, 0xC0 }, "LDA", "$C019"));
            data.lines.push_back (MakeInstruction (0x303, { 0x4C, 0x00, 0x03 }, "JMP", "$0300"));

            reply.verb = DebugVerb::Disassemble;
            reply.data = data;
            AssertMatchesFixture (reply, "list.txt");
        }

        TEST_METHOD (BreakpointList_IdKindPlaceAndAccess)
        {
            Reply               reply;
            BreakpointListData  data;
            BreakpointInfo      exec;
            BreakpointInfo      read;
            BreakpointInfo      both;
            BreakpointInfo      range;



            exec.id        = 0;
            exec.address   = 0x300;
            exec.last      = 0x300;

            read.id        = 1;
            read.kind      = BreakpointKind::Memory;
            read.address   = 0xC019;
            read.last      = 0xC019;
            read.access    = WatchAccess::Read;

            both.id        = 2;
            both.kind      = BreakpointKind::Memory;
            both.address   = 0xC010;
            both.last      = 0xC010;
            both.access    = WatchAccess::ReadWrite;

            range.id       = 3;
            range.address  = 0x400;
            range.last     = 0x40F;

            data.breakpoints = { exec, read, both, range };
            reply.verb       = DebugVerb::ListBreakpoints;
            reply.data       = data;
            AssertMatchesFixture (reply, "breakpoints.txt");
        }

        TEST_METHOD (BreakpointSet_ById)
        {
            Reply              reply;
            BreakpointSetData  data;



            data.breakpoint.id = 1;
            reply.verb         = DebugVerb::SetBreakpoint;
            reply.data         = data;
            AssertMatchesFixture (reply, "breakpoint-set.txt");
        }

        TEST_METHOD (WatchList_AndWatchesSet)
        {
            Reply          list;
            Reply          added;
            WatchListData  data;



            data.entries = { WatchEntry { 0, 0x0006 }, WatchEntry { 1, 0x0007 } };

            list.verb  = DebugVerb::ListWatches;
            list.data  = data;
            added.verb = DebugVerb::AddWatch;
            added.data = data;

            AssertMatchesFixture (list,  "watches.txt");
            AssertMatchesFixture (added, "watch-set.txt");
        }

        TEST_METHOD (Save_Slookup_Sclear)
        {
            Reply       save;
            Reply       lookup;
            Reply       clear;
            FileIoData  file;
            SymbolData  symbols;



            file.path        = "out.bin";
            file.requested   = 16;
            file.transferred = 16;
            save.verb        = DebugVerb::SaveBinary;
            save.data        = file;

            symbols.symbols = { SymbolInfo { "COUT", 0xFDED } };
            lookup.verb     = DebugVerb::LookupSymbol;
            lookup.data     = symbols;

            clear.verb = DebugVerb::ClearSymbols;
            clear.data = MessageData { { "Cleared Main." } };

            AssertMatchesFixture (save,   "save.txt");
            AssertMatchesFixture (lookup, "slookup.txt");
            AssertMatchesFixture (clear,  "sclear.txt");
        }

        //  GSSquared prints nothing for a deposit, though the reply carries
        //  the rows written.
        TEST_METHOD (Deposit_PrintsNothing)
        {
            Reply  reply;



            reply.verb = DebugVerb::EnterBytes;
            reply.data = MakeMemory (0x300, { 0xA9, 0x41 });
            GSSquaredFormatter::Format (reply);

            Assert::IsTrue (reply.text.empty());
        }

        //  A reply kind GSSquared has no layout for keeps the AppleWin text,
        //  as the Monitor formatter does; errors too.
        TEST_METHOD (NoGSSquaredLayout_KeepsTheAppleWinText)
        {
            Reply  registers;
            Reply  error;



            registers.verb = DebugVerb::ShowRegisters;
            registers.data = RegistersData { { 0x0300, 0x41, 0, 0, 0xFF, 0x30 } };
            GSSquaredFormatter::Format (registers);

            error.SetError (CommandStatus::NotAvailable, "command not available", "MAP needs a IIgs.");
            GSSquaredFormatter::Format (error);

            Assert::IsTrue   (registers.text.front().starts_with ("A:41"));
            Assert::AreEqual (std::string ("Error: command not available"), error.text.front());
        }
    };
}
