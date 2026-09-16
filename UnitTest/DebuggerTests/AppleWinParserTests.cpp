#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "MockExpressionContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinParserTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AppleWinParserTests)
    {
    public:

        static AppleWinParseResult ParseOk (const std::string & line)
        {
            MockExpressionContext  context;
            AppleWinParseResult    result = AppleWinParser::Parse (line, context);
            std::wstring           where (line.begin(), line.end());



            Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status, (where + L": " + std::wstring (result.error.begin(), result.error.end())).c_str());
            return result;
        }

        static AppleWinParseResult ParseFails (const std::string & line, ParseStatus status)
        {
            MockExpressionContext  context;
            AppleWinParseResult    result = AppleWinParser::Parse (line, context);
            std::wstring           where (line.begin(), line.end());



            Assert::AreEqual ((int) status, (int) result.status, where.c_str());
            Assert::IsFalse  (result.error.empty(), where.c_str());
            return result;
        }



        TEST_METHOD (Names_IgnoreCase_HexWithAndWithoutDollar)
        {
            Assert::AreEqual ((int) DebugVerb::SetReadWatchpoint, (int) ParseOk ("bpmr C019").command.verb);
            Assert::AreEqual ((Word) 0xC019,                      ParseOk ("BPMR $C019").command.a1);
            Assert::AreEqual ((Word) 0xC019,                      ParseOk ("BpMr c019").command.a1);
        }



        TEST_METHOD (Watchpoints_BeforeOrAfter)
        {
            Assert::AreEqual (std::string ("AFTER"),  ParseOk ("BPM C030").command.text);
            Assert::AreEqual (std::string ("BEFORE"), ParseOk ("BPMW 0400 before").command.text);
            Assert::AreEqual (std::string ("AFTER"),  ParseOk ("BPMR C0EC AFTER").command.text);
            Assert::AreEqual ((Word) 0x040F,          ParseOk ("BPM 400,10 BEFORE").command.a2);
            ParseFails ("BPM 400 SIDEWAYS", ParseStatus::Invalid);
        }



        TEST_METHOD (Expressions_ThroughEvaluator)
        {
            Assert::AreEqual ((Word) 0x0302, ParseOk ("BP PC+2").command.a1);
            Assert::AreEqual ((Word) 0xFC58, ParseOk ("BP HOME").command.a1);
            Assert::AreEqual ((Word) 10,     ParseOk ("BP #10").command.a1);
        }



        TEST_METHOD (Ranges_LengthAndLast)
        {
            AppleWinParseResult  length = ParseOk ("BPM 400,10");
            AppleWinParseResult  last   = ParseOk ("D 300:30F");
            AppleWinParseResult  single = ParseOk ("U 300");



            Assert::AreEqual ((Word) 0x0400, length.command.a1);
            Assert::AreEqual ((Word) 0x040F, length.command.a2);
            Assert::AreEqual ((Word) 0x030F, last.command.a2);
            Assert::IsTrue   (single.command.hasA1);
            Assert::IsFalse  (single.command.hasA2);
            ParseFails ("BPM 400,0", ParseStatus::Invalid);
        }



        TEST_METHOD (Run_StopSkipAndCounts)
        {
            AppleWinParseResult  go   = ParseOk ("G C600 D000,3000");
            AppleWinParseResult  bare = ParseOk ("GG");



            Assert::AreEqual ((Word) 0xC600, go.command.a1);
            Assert::AreEqual ((Word) 0xD000, go.command.a2);
            Assert::AreEqual ((Word) 0xFFFF, go.command.a3);
            Assert::IsFalse  (bare.command.hasA1);
            Assert::AreEqual ((int) DebugVerb::GoFullSpeed, (int) bare.command.verb);
            Assert::AreEqual ((uint32_t) 16, ParseOk ("T 10").command.count);
            Assert::AreEqual ((uint32_t) 1,  ParseOk ("P").command.count);
        }



        TEST_METHOD (Registers_ShowAndSet)
        {
            AppleWinParseResult  set  = ParseOk ("R A=41");
            AppleWinParseResult  pc   = ParseOk ("r pc = FA62");
            AppleWinParseResult  hash = ParseOk ("R A #10");



            Assert::AreEqual ((int) DebugVerb::ShowRegisters, (int) ParseOk ("R").command.verb);
            Assert::AreEqual ((int) DebugVerb::SetRegister,   (int) set.command.verb);
            Assert::AreEqual (std::string ("A"),  set.command.text);
            Assert::AreEqual ((Word) 0x41,        set.command.a1);
            Assert::AreEqual (std::string ("PC"), pc.command.text);
            Assert::AreEqual ((Word) 0xFA62,      pc.command.a1);
            Assert::AreEqual ((Word) 10,          hash.command.a1);
            ParseFails ("R Q 1", ParseStatus::Invalid);
        }



        TEST_METHOD (Flags_FromNameOrArgument)
        {
            Assert::AreEqual (std::string ("C"), ParseOk ("CLC").command.text);
            Assert::AreEqual (std::string ("Z"), ParseOk ("RZ").command.text);
            Assert::AreEqual (std::string ("N"), ParseOk ("SN").command.text);
            Assert::AreEqual (std::string ("D"), ParseOk ("SE d").command.text);
            ParseFails ("CL Q", ParseStatus::Invalid);
        }



        TEST_METHOD (Breakpoints_ConditionsIdsAndOpcodes)
        {
            AppleWinParseResult  bpr   = ParseOk ("BPR A ! 0");
            AppleWinParseResult  bprEq = ParseOk ("BPR A 0");
            AppleWinParseResult  bpx   = ParseOk ("BPX < FA62");
            AppleWinParseResult  op    = ParseOk ("BRKOP 6C");



            Assert::AreEqual ((int) DebugVerb::SetRegisterBreakpoint,    (int) bpr.command.verb);
            Assert::AreEqual (std::string ("A!=0"),  bpr.command.expression.text);
            Assert::AreEqual (std::string ("A=0"),   bprEq.command.expression.text);
            Assert::AreEqual ((int) DebugVerb::SetConditionalBreakpoint, (int) bpx.command.verb);
            Assert::AreEqual (std::string ("PC<FA62"), bpx.command.expression.text);
            Assert::AreEqual ((Byte) 0x6C,           op.command.values.at (0));
            Assert::AreEqual ((uint32_t) 3,          ParseOk ("BPC 3").command.count);
            Assert::AreEqual (std::string ("*"),     ParseOk ("BPC *").command.text);
            Assert::AreEqual (std::string ("ALL ON"), ParseOk ("brk all on").command.text);
            Assert::AreEqual ((Word) 0x00A0,         ParseOk ("BPV A0").command.a1);
            Assert::AreEqual ((Word) 0x00AF,         ParseOk ("BPV A0,10").command.a2);
            ParseFails ("BPC x", ParseStatus::Invalid);
            ParseFails ("BPM",   ParseStatus::Invalid);
            ParseFails ("BPA",   ParseStatus::Invalid);
            ParseFails ("BPV",   ParseStatus::Invalid);
        }



        TEST_METHOD (Memory_EnterFillSearchMoveAndFiles)
        {
            AppleWinParseResult  me   = ParseOk ("ME 300 A9 41 60");
            AppleWinParseResult  mew  = ParseOk ("MEW 300 1234");
            AppleWinParseResult  fill = ParseOk ("F 300,10 EA");
            AppleWinParseResult  move = ParseOk ("M 800 300,10");
            AppleWinParseResult  load = ParseOk ("BLOAD out.bin 300");



            Assert::IsTrue   (std::vector<Byte> ({ 0xA9, 0x41, 0x60 }) == me.command.values);
            Assert::IsTrue   (std::vector<Byte> ({ 0x34, 0x12 })       == mew.command.values);
            Assert::AreEqual ((Word) 0x030F, fill.command.a2);
            Assert::AreEqual ((Byte) 0xEA,   fill.command.values.at (0));
            Assert::AreEqual ((Word) 0x0800, move.command.a3);
            Assert::AreEqual ((Word) 0x0300, move.command.a1);
            Assert::AreEqual (std::string ("out.bin"), load.command.text);
            Assert::AreEqual ((Word) 0x0300, load.command.a1);
            ParseFails ("OUT C030 1234", ParseStatus::Invalid);
            ParseFails ("BSAVE",         ParseStatus::Invalid);
        }



        TEST_METHOD (CorrectedForms_FromAppleWinSource)
        {
            AppleWinParseResult  fill3  = ParseOk ("F 2000 3FFF 00");
            AppleWinParseResult  mebW   = ParseOk ("MEB 300 A9 1234 60");
            AppleWinParseResult  bpa    = ParseOk ("BPA C030");
            AppleWinParseResult  change = ParseOk ("BPCHANGE 2 Es");
            AppleWinParseResult  edit   = ParseOk ("BPEDIT 1 BPR A = 41");
            AppleWinParseResult  calc   = ParseOk ("CALC 'A'+1");



            Assert::AreEqual ((Word) 0x2000, fill3.command.a1);
            Assert::AreEqual ((Word) 0x3FFF, fill3.command.a2);
            Assert::IsTrue   (std::vector<Byte> ({ 0x00 }) == fill3.command.values);

            Assert::IsTrue   (std::vector<Byte> ({ 0xA9, 0x34, 0x12, 0x60 }) == mebW.command.values);

            Assert::AreEqual ((int) DebugVerb::SetBreakpointAndWatchpoint, (int) bpa.command.verb);
            Assert::AreEqual ((int) DebugVerb::SetMemoryWatchpoint,        (int) ParseOk ("BPIO C000").command.verb);
            Assert::AreEqual ((int) DebugVerb::StepInto,                   (int) ParseOk ("TL 3").command.verb);
            Assert::AreEqual ((uint32_t) 3,                                ParseOk ("TL 3").command.count);
            Assert::AreEqual ((int) DebugVerb::DefineBytes,                (int) ParseOk ("Z 300").command.verb);
            Assert::AreEqual ((int) DebugVerb::ListData,                   (int) ParseOk ("B").command.verb);

            Assert::AreEqual ((uint32_t) 2,       change.command.count);
            Assert::AreEqual (std::string ("Es"), change.command.text);
            Assert::AreEqual ((uint32_t) 1,       edit.command.count);
            Assert::AreEqual (std::string ("BPR A = 41"), edit.command.text);
            ParseFails ("BPEDIT 1", ParseStatus::Invalid);

            Assert::AreEqual ((Word) 0x42, calc.command.a1);
            Assert::IsTrue   (calc.command.hasA1);
            ParseFails ("CALC", ParseStatus::Invalid);
        }



        TEST_METHOD (Search_ItemsAndWildcards)
        {
            AppleWinParseResult  text   = ParseOk ("S F000,1000 'Ap' \"b\" AD ? C0 3? ?1 C030");



            Assert::AreEqual ((Word) 0xF000, text.command.a1);
            Assert::IsTrue   (std::vector<Byte> ({ 0xC1, 0xF0, 0x62, 0xAD, 0x00, 0xC0, 0x30, 0x01, 0x30, 0xC0 }) == text.command.values);
            Assert::IsTrue   (std::vector<Byte> ({ 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF }) == text.command.mask);
            Assert::IsTrue   (std::vector<Byte> ({ 0x00 }) == ParseOk ("SH 800,8000 ??").command.mask);
            ParseFails ("S 800,10",        ParseStatus::Invalid);
            ParseFails ("S 800,10 'open",  ParseStatus::Invalid);
        }



        TEST_METHOD (DataDirectives_NameForms)
        {
            AppleWinParseResult  plain = ParseOk ("DB 300:30F");
            AppleWinParseResult  named = ParseOk ("DW TABLE 3F0");
            AppleWinParseResult  eq    = ParseOk ("ASC GREETING = 800:80B");
            AppleWinParseResult  bare  = ParseOk ("DB");
            AppleWinParseResult  undef = ParseOk ("X 305");



            Assert::AreEqual ((Word) 0x0300,           plain.command.a1);
            Assert::IsTrue   (plain.command.text.empty());
            Assert::AreEqual (std::string ("TABLE"),    named.command.text);
            Assert::AreEqual ((Word) 0x03F0,           named.command.a1);
            Assert::AreEqual (std::string ("GREETING"), eq.command.text);
            Assert::AreEqual ((Word) 0x080B,           eq.command.a2);
            Assert::IsFalse  (bare.command.hasA1);
            Assert::AreEqual ((int) DebugVerb::RemoveData, (int) undef.command.verb);
            Assert::AreEqual ((Word) 0x0305,           undef.command.a1);
        }



        TEST_METHOD (Symbols_TableSubcommands)
        {
            AppleWinParseResult  load = ParseOk ("SYMUSER LOAD \"game.sym\",800");



            Assert::AreEqual ((int) DebugVerb::ShowSymbolInfo, (int) ParseOk ("SYM").command.verb);
            Assert::AreEqual ((int) DebugVerb::ClearSymbols,   (int) ParseOk ("SYMASM clear").command.verb);
            Assert::AreEqual ((int) DebugVerb::EnableSymbols,  (int) ParseOk ("SYMMAIN OFF").command.verb);
            Assert::AreEqual ((uint32_t) 0,                    ParseOk ("SYMMAIN OFF").command.count);
            Assert::AreEqual ((uint32_t) 1,                    ParseOk ("SYMMAIN on").command.count);
            Assert::AreEqual ((int) DebugVerb::LoadSymbols,    (int) load.command.verb);
            Assert::AreEqual (std::string ("\"game.sym\",800"), load.command.text);
            Assert::AreEqual ((int) DebugVerb::SaveSymbols,    (int) ParseOk ("SYMUSER SAVE \"out.sym\"").command.verb);
            Assert::AreEqual ((int) DebugVerb::RemoveSymbol,   (int) ParseOk ("SYM ~ LIFE").command.verb);
            Assert::AreEqual ((int) DebugVerb::LookupSymbol,   (int) ParseOk ("SYM game.sym").command.verb);
            ParseFails ("SYMUSER LOAD", ParseStatus::Invalid);
        }



        TEST_METHOD (Shorthand_ListAndMove)
        {
            AppleWinParseResult  list = ParseOk ("300L");
            AppleWinParseResult  move = ParseOk ("4000<2000.3FFFM");



            Assert::AreEqual ((int) DebugVerb::Disassemble, (int) list.command.verb);
            Assert::AreEqual ((Word) 0x0300,                list.command.a1);
            Assert::AreEqual ((int) DebugVerb::MoveMemory,  (int) move.command.verb);
            Assert::AreEqual ((Word) 0x4000,                move.command.a3);
            Assert::AreEqual ((Word) 0x2000,                move.command.a1);
            Assert::AreEqual ((Word) 0x3FFF,                move.command.a2);
            ParseFails ("FROG",  ParseStatus::Unknown);
            ParseFails ("FROGL", ParseStatus::Unknown);
        }



        TEST_METHOD (Shorthand_DepositAndGo)
        {
            AppleWinParseResult  deposit = ParseOk ("300:60");
            AppleWinParseResult  spaced  = ParseOk ("300: A9 00");
            AppleWinParseResult  go      = ParseOk ("300G");



            Assert::AreEqual ((int) DebugVerb::EnterBytes, (int) deposit.command.verb);
            Assert::AreEqual ((Word) 0x0300,               deposit.command.a1);
            Assert::IsTrue   (std::vector<Byte> ({ 0x60 }) == deposit.command.values);
            Assert::IsTrue   (std::vector<Byte> ({ 0xA9, 0x00 }) == spaced.command.values);
            Assert::AreEqual ((int) DebugVerb::Go,         (int) go.command.verb);
            Assert::AreEqual ((Word) 0x0300,               go.command.a3);
        }



        TEST_METHOD (Lists_AddressesIdsAndSlots)
        {
            AppleWinParseResult  zp = ParseOk ("ZP3 36");



            Assert::AreEqual ((Word) 0x0036,  ParseOk ("WA 36").command.a1);
            Assert::AreEqual ((uint32_t) 3,   zp.command.count);
            Assert::AreEqual ((Word) 0x0036,  zp.command.a1);
            Assert::AreEqual ((uint32_t) 2,   ParseOk ("BMC 2").command.count);
            Assert::AreEqual (std::string ("*"), ParseOk ("WC *").command.text);
        }



        TEST_METHOD (Symbols_LookupAddRemoveLoad)
        {
            AppleWinParseResult  add = ParseOk ("SYM LIFE = 300");



            Assert::AreEqual ((int) DebugVerb::AddSymbol,    (int) add.command.verb);
            Assert::AreEqual (std::string ("LIFE"),          add.command.text);
            Assert::AreEqual ((Word) 0x0300,                 add.command.a1);
            Assert::AreEqual ((int) DebugVerb::RemoveSymbol, (int) ParseOk ("SYM ! LIFE").command.verb);
            Assert::AreEqual ((int) DebugVerb::LookupSymbol, (int) ParseOk ("SYM HOME").command.verb);
        }



        TEST_METHOD (Engine_ModePauseBudget)
        {
            AppleWinParseResult  mode   = ParseOk ("MODE monitor");
            AppleWinParseResult  budget = ParseOk ("BUDGET 5000000");



            Assert::AreEqual ((int) DebugVerb::SetMode,     (int) mode.command.verb);
            Assert::AreEqual ((int) CommandMode::Monitor,   (int) mode.command.mode);
            Assert::AreEqual ((int) DebugVerb::ShowMode,    (int) ParseOk ("MODE").command.verb);
            Assert::AreEqual ((int) DebugVerb::Pause,       (int) ParseOk ("PAUSE").command.verb);
            Assert::AreEqual ((uint32_t) 5000000,           budget.command.count);
            Assert::AreEqual ((uint32_t) 0,                 ParseOk ("BUDGET 0").command.count);
            ParseFails ("MODE sideways", ParseStatus::Invalid);
            ParseFails ("BUDGET C000",   ParseStatus::Invalid);
        }



        TEST_METHOD (UnknownWindowOnlyAndNotAvailable)
        {
            MockExpressionContext  context;



            ParseFails ("FROB",  ParseStatus::Unknown);
            ParseFails ("HGR",   ParseStatus::WindowOnly);
            ParseFails ("SHR",   ParseStatus::NotAvailable);

            Assert::AreEqual (std::string ("HGR needs the debugger window."), AppleWinParser::Parse ("hgr", context).error);
            Assert::AreEqual ((int) ParseStatus::Empty, (int) AppleWinParser::Parse ("   ", context).status);
        }



        TEST_METHOD (TextArguments_KeepRestOfLine)
        {
            Assert::AreEqual (std::string ("hello,  world"), ParseOk ("ECHO hello,  world").command.text);
            Assert::AreEqual (std::string ("C:\\disks"),     ParseOk ("CD C:\\disks").command.text);
            Assert::AreEqual (std::string ("script.txt"),    ParseOk ("RUN script.txt").command.text);
            Assert::AreEqual (std::string ("Bp.txt"),        ParseOk ("BPSAVE Bp.txt").command.text);
            Assert::AreEqual (std::string ("ALL ON"),        ParseOk ("BRK all on").command.text);
        }
    };
}
