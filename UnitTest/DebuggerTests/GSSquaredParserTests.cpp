#include "Pch.h"

#include "Debugger/GSSquaredParser.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GSSquaredParserTests
    //
    //  Every row of contracts/gssquared-mode.md's command table: the command
    //  a line produces, not its effect. What each command does to a machine
    //  is the sweep in GSSquaredCommandSweepTests, against AppleWin mode.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (GSSquaredParserTests)
    {
    public:
        MockExpressionContext  context;



        GSSquaredParseResult Parse (const std::string & line)
        {
            return GSSquaredParser::Parse (line, context);
        }

        //  The one command the line produced.
        DebugCommand One (const std::string & line)
        {
            GSSquaredParseResult  result = Parse (line);



            Assert::IsTrue   (result.status == ParseStatus::Ok, Widen (line + ": " + result.error).c_str());
            Assert::AreEqual (size_t (1), result.commands.size(), Widen (line).c_str());

            return result.commands.front();
        }

        //  A line that fails, with nothing to run.
        GSSquaredParseResult Refused (const std::string & line, ParseStatus status)
        {
            GSSquaredParseResult  result = Parse (line);



            Assert::AreEqual ((int) status, (int) result.status, Widen (line + ": " + result.error).c_str());
            Assert::IsTrue   (result.commands.empty(), Widen (line).c_str());
            Assert::IsFalse  (result.error.empty(), Widen (line).c_str());

            return result;
        }

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }

        static void AssertVerb (DebugVerb expected, const DebugCommand & command, const std::string & line)
        {
            Assert::AreEqual ((int) expected, (int) command.verb, Widen (line).c_str());
        }

        static void AssertRange (Word first, Word last, const DebugCommand & command, const std::string & line)
        {
            Assert::IsTrue   (command.hasA1 && command.hasA2, Widen (line).c_str());
            Assert::AreEqual ((int) first, (int) command.a1, Widen (line).c_str());
            Assert::AreEqual ((int) last,  (int) command.a2, Widen (line).c_str());
        }



        TEST_METHOD (Address_ExaminesOneByte)
        {
            DebugCommand  command = One ("300");



            AssertVerb  (DebugVerb::DumpMemory, command, "300");
            AssertRange (0x300, 0x300, command, "300");
            Assert::IsTrue (command.mode == CommandMode::GSSquared);
        }

        TEST_METHOD (Range_Dumps)
        {
            DebugCommand  command = One ("300.30F");



            AssertVerb  (DebugVerb::DumpMemory, command, "300.30F");
            AssertRange (0x300, 0x30F, command, "300.30F");
        }

        TEST_METHOD (Deposit_ColonAndSet_WriteTheSameBytes)
        {
            for (const char * line : { "300: A9 41", "300:A9 41", "set 300 A9 41", "SET 300 a9 41" })
            {
                DebugCommand  command = One (line);



                AssertVerb (DebugVerb::EnterBytes, command, line);
                Assert::AreEqual (0x300, (int) command.a1, Widen (line).c_str());
                Assert::IsTrue   ((command.values == std::vector<Byte> { 0xA9, 0x41 }), Widen (line).c_str());
            }
        }

        TEST_METHOD (Deposit_TakesBytesOnly)
        {
            Refused ("300: 100",  ParseStatus::Invalid);
            Refused ("300:",      ParseStatus::Invalid);
            Refused ("set 300",   ParseStatus::Invalid);
            Refused ("set 300 G", ParseStatus::Invalid);
        }

        TEST_METHOD (Move_CopiesARange)
        {
            DebugCommand  command = One ("move 300.30F 400");



            AssertVerb  (DebugVerb::MoveMemory, command, "move");
            AssertRange (0x300, 0x30F, command, "move");
            Assert::AreEqual (0x400, (int) command.a3);
            Refused ("move 300 400",     ParseStatus::Invalid);
            Refused ("move 300.30F",     ParseStatus::Invalid);
        }

        TEST_METHOD (List_WithAndWithoutAnAddress)
        {
            DebugCommand  at   = One ("l 300");
            DebugCommand  more = One ("l");



            AssertVerb (DebugVerb::Disassemble, at,   "l 300");
            AssertVerb (DebugVerb::Disassemble, more, "l");
            Assert::IsTrue   (at.hasA1);
            Assert::AreEqual (0x300, (int) at.a1);
            Assert::IsFalse  (more.hasA1);
            AssertVerb (DebugVerb::Disassemble, One ("list 300"), "list 300");
        }

        //  GSSquared splits a token ending in `l` into `l` and an address.
        //  Casso does so only when the front is an address, so a word that
        //  happens to end in `l` is still that word.
        TEST_METHOD (AddressL_Lists_ButAWordEndingInL_IsNotSplit)
        {
            DebugCommand  command = One ("300l");



            AssertVerb (DebugVerb::Disassemble, command, "300l");
            Assert::AreEqual (0x300, (int) command.a1);
            AssertVerb (DebugVerb::Disassemble, One ("300L"), "300L");

            //  bpl is Casso's BPL, which GSSquared reaches by name, not bp and l.
            AssertVerb (DebugVerb::ListBreakpoints, One ("bpl"), "bpl");
        }

        TEST_METHOD (Bp_ListsSetsAndTakesARange)
        {
            DebugCommand  one   = One ("bp 300");
            DebugCommand  range = One ("bp C000.C0FF");



            AssertVerb (DebugVerb::ListBreakpoints, One ("bp"), "bp");
            AssertVerb (DebugVerb::SetBreakpoint,   one,        "bp 300");
            Assert::AreEqual (0x300, (int) one.a1);
            Assert::IsFalse  (one.hasA2);
            AssertVerb  (DebugVerb::SetBreakpoint, range, "bp C000.C0FF");
            AssertRange (0xC000, 0xC0FF, range, "bp C000.C0FF");
        }

        TEST_METHOD (Bp_TakesAnIfExpression)
        {
            DebugCommand  command = One ("bp 300 if X == 3");



            AssertVerb (DebugVerb::SetBreakpoint, command, "bp IF");
            Assert::IsFalse (command.expression.postfix.empty());
            Refused ("bp 300 400", ParseStatus::Invalid);
        }

        TEST_METHOD (Bpd_ReadWriteAndBoth)
        {
            AssertVerb (DebugVerb::SetReadWatchpoint,   One ("bpd C019 r"),  "r");
            AssertVerb (DebugVerb::SetWriteWatchpoint,  One ("bpd C019 w"),  "w");
            AssertVerb (DebugVerb::SetMemoryWatchpoint, One ("bpd C019 rw"), "rw");
            AssertRange (0x400, 0x40F, One ("bpd 400.40F w"), "bpd range");

            Refused ("bpd C019",   ParseStatus::Invalid);
            Refused ("bpd C019 x", ParseStatus::Invalid);
        }

        TEST_METHOD (Bpi_IsTheSameWatchpoint_OnTheIoPageOnly)
        {
            DebugCommand  command = One ("bpi C010 rw");



            AssertVerb (DebugVerb::SetMemoryWatchpoint, command, "bpi");
            Assert::AreEqual (0xC010, (int) command.a1);
            Refused ("bpi 300 rw", ParseStatus::Invalid);
        }

        //  Whether `nobp 3` means id 3 or address $0003 depends on the tables,
        //  so the parser carries both and the session decides.
        TEST_METHOD (Nobp_CarriesAnIdAndAnAddress)
        {
            GSSquaredParseResult  byNumber  = Parse ("nobp 3");
            GSSquaredParseResult  byAddress = Parse ("nobp C000");



            Assert::IsTrue   (byNumber.status == ParseStatus::Ok);
            Assert::IsTrue   (byNumber.isIdOrAddress);
            AssertVerb       (DebugVerb::ClearBreakpoint, byNumber.commands.front(), "nobp 3");
            Assert::AreEqual (3u, byNumber.commands.front().count);
            Assert::AreEqual (3,  (int) byNumber.commands.front().a1);

            Assert::IsTrue   (byAddress.status == ParseStatus::Ok);
            Assert::IsTrue   (byAddress.commands.front().hasA1);
            Assert::AreEqual (0xC000, (int) byAddress.commands.front().a1);

            Refused ("nobp",      ParseStatus::Invalid);
            Refused ("nobp zz",   ParseStatus::Invalid);
        }

        TEST_METHOD (Watch_AddsListsAndClears)
        {
            GSSquaredParseResult  range = Parse ("watch 6.7");



            AssertVerb (DebugVerb::ListWatches, One ("watch"),     "watch");
            AssertVerb (DebugVerb::AddWatch,    One ("watch 6"),   "watch 6");
            AssertVerb (DebugVerb::ClearWatch,  One ("nowatch 1"), "nowatch 1");
            Assert::AreEqual (1u, One ("nowatch 1").count);

            Assert::IsTrue   (range.status == ParseStatus::Ok);
            Assert::AreEqual (size_t (2), range.commands.size());
            Assert::AreEqual (6, (int) range.commands[0].a1);
            Assert::AreEqual (7, (int) range.commands[1].a1);

            Refused ("nowatch", ParseStatus::Invalid);
        }

        TEST_METHOD (LoadAndSave_AreBloadAndBsave)
        {
            DebugCommand  load = One ("load \"prog.bin\" 300");
            DebugCommand  save = One ("save \"prog.bin\" 300.30F");



            AssertVerb (DebugVerb::LoadBinary, load, "load");
            Assert::AreEqual (0x300, (int) load.a1);
            Assert::IsTrue   (load.text.find ("prog.bin") != std::string::npos);

            AssertVerb  (DebugVerb::SaveBinary, save, "save");
            AssertRange (0x300, 0x30F, save, "save");

            Refused ("load \"prog.bin\"", ParseStatus::Invalid);
        }

        //  SYM's name selects the symbol table, so these keep it.
        TEST_METHOD (Symbols_LoadLookUpAndClear)
        {
            DebugCommand  load   = One ("sload \"labels.sym\"");
            DebugCommand  lookup = One ("slookup FDED");



            AssertVerb (DebugVerb::LoadSymbols,  load,              "sload");
            AssertVerb (DebugVerb::LookupSymbol, lookup,            "slookup");
            AssertVerb (DebugVerb::ClearSymbols, One ("sclear"),    "sclear");
            Assert::AreEqual (std::string ("SYM"), load.sourceName);
            Assert::IsTrue   (lookup.text.find ("FDED") != std::string::npos);
        }

        TEST_METHOD (Stepping_AndResuming)
        {
            AssertVerb (DebugVerb::StepInto, One ("s"), "s");
            AssertVerb (DebugVerb::StepOver, One ("o"), "o");
            AssertVerb (DebugVerb::StepOut,  One ("r"), "r");
            AssertVerb (DebugVerb::Go,       One ("g"), "g");
            AssertVerb (DebugVerb::StepOver, One ("O"), "O");

            Refused ("o 3", ParseStatus::Invalid);
        }

        TEST_METHOD (EmptyLine_IsNothing)
        {
            Assert::IsTrue (Parse ("").status    == ParseStatus::Empty);
            Assert::IsTrue (Parse ("   ").status == ParseStatus::Empty);
            Assert::IsTrue (Parse ("").commands.empty());
        }

        TEST_METHOD (Debug_AndTheIIgsCommands_AreNotAvailable_WithTheirReason)
        {
            for (const char * line : { "debug \"disk\"", "debug", "nodebug \"disk\"", "m 8", "x 16", "map", "video hgr1", "novideo 1", "verify" })
            {
                GSSquaredParseResult  result = Refused (line, ParseStatus::NotAvailable);



                Assert::IsTrue (result.error.find ("not a command") == std::string::npos, Widen (line).c_str());
            }

            Assert::IsTrue (Parse ("map").error.find ("IIgs")  != std::string::npos);
            Assert::IsTrue (Parse ("m").error.find   ("IIgs")  != std::string::npos);
            Assert::IsTrue (Parse ("video").error.find ("screen") != std::string::npos);
        }

        //  FR-022b: bank 00 is the address; any other bank does not exist here.
        TEST_METHOD (Bank00_IsTheAddress_OtherBanksAreRefused)
        {
            GSSquaredParseResult  e1 = Refused ("e1/300", ParseStatus::NotAvailable);



            AssertRange (0x300, 0x300, One ("00/300"), "00/300");
            AssertRange (0x300, 0x30F, One ("00/300.30F"), "00/300.30F");
            Assert::AreEqual (0x300, (int) One ("bp 00/300").a1);
            Assert::IsTrue   (e1.error.find ("E1") != std::string::npos);
            Assert::IsTrue   (e1.error.find ("Only bank 00 exists on this machine") != std::string::npos);
            Refused ("bp E1/300", ParseStatus::NotAvailable);
        }

        TEST_METHOD (Names_IgnoreCase)
        {
            AssertVerb (DebugVerb::SetBreakpoint, One ("BP 300"),   "BP");
            AssertVerb (DebugVerb::AddWatch,      One ("Watch 6"),  "Watch");
            AssertVerb (DebugVerb::LoadSymbols,   One ("SLOAD x"),  "SLOAD");
        }

        //  R-037: engine commands are bare names here; `/` is GSSquared's bank
        //  separator and never a marker.
        TEST_METHOD (EngineCommands_AreBareNames_AndSlashIsNoMarker)
        {
            DebugCommand  mode = One ("mode applewin");



            AssertVerb (DebugVerb::ShowSwitches, One ("switches"), "switches");
            AssertVerb (DebugVerb::SetMode,      mode,             "mode applewin");
            Assert::IsTrue (mode.mode == CommandMode::AppleWin);
            AssertVerb (DebugVerb::SetOutputFormat, One ("output applewin"), "output");

            Refused ("/switches", ParseStatus::Unknown);
            Refused ("/bpl",      ParseStatus::Unknown);
        }

        TEST_METHOD (MalformedLines_ProduceAnErrorAndNoCommand)
        {
            Refused ("frob",        ParseStatus::Unknown);
            Refused ("bp zz",       ParseStatus::Invalid);
            Refused ("300.2FF",     ParseStatus::Invalid);
            Refused ("300 400",     ParseStatus::Invalid);
            Refused ("12345",       ParseStatus::Invalid);
            Refused ("300.30F 400", ParseStatus::Invalid);
            Refused ("l 300 400",   ParseStatus::Invalid);
        }

        //  The list the sweep walks is GSSquared's own table plus the four
        //  additions, and nothing is in it twice.
        TEST_METHOD (CommandTable_IsGSSquaredsWithTheAdditions)
        {
            std::set<std::string>  names;



            for (const GSSquaredCommand & command : GSSquaredParser::GetCommands())
            {
                Assert::IsTrue (names.insert (command.name).second, Widen (command.name).c_str());
            }

            for (const char * name : { "set", "load", "save", "move", "verify", "watch", "nowatch", "help", "bp", "bpd", "bpi",
                                       "nobp", "list", "l", "map", "debug", "nodebug", "sload", "sclear", "slookup", "m", "x",
                                       "video", "novideo", "s", "o", "r", "g" })
            {
                Assert::IsTrue (names.contains (name), Widen (name).c_str());
            }

            Assert::AreEqual (size_t (28), names.size());
        }
    };
}
