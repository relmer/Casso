#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/WinDbgParser.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgParserTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgParserTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }

        static WinDbgParseResult ParseOk (const std::string & line)
        {
            MockExpressionContext  context;
            WinDbgParseResult      result = WinDbgParser::Parse (line, context);



            Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status, Widen (line + ": " + result.error).c_str());
            return result;
        }

        static WinDbgParseResult ParseFails (const std::string & line, ParseStatus status)
        {
            MockExpressionContext  context;
            WinDbgParseResult      result = WinDbgParser::Parse (line, context);



            Assert::AreEqual ((int) status, (int) result.status, Widen (line).c_str());
            Assert::IsFalse  (result.error.empty(), Widen (line).c_str());
            return result;
        }



        TEST_METHOD (Numbers_AreHex_WithOrWithoutPrefix)
        {
            for (const char * line : { "db 300", "db 0x300", "db $300", "db 0X300" })
            {
                Assert::AreEqual ((Word) 0x0300, ParseOk (line).command.a1, Widen (line).c_str());
            }

            Assert::AreEqual ((Word) 10, ParseOk ("? 0n10").command.a1);
        }

        //  A prefix is rewritten only on a number standing alone, so a file
        //  name that starts like one is left as typed, and a decimal-only
        //  engine command takes 0n.
        TEST_METHOD (Numbers_InFileNamesAreLeftAlone_AndDecimalTakes0n)
        {
            Assert::AreEqual (std::string ("0x1.txt"),  ParseOk ("!tf 0x1.txt").command.text);
            Assert::AreEqual (std::string ("0n1.txt"),  ParseOk ("!history save 0n1.txt").command.text);
            Assert::AreEqual ((uint64_t) 100,           ParseOk ("!history 0n100 0n20").command.first.value_or (0));
            Assert::AreEqual ((uint32_t) 20,            ParseOk ("!history 0n100 0n20").command.count);
        }

        //  A `!` word that is no command anywhere is not sent to .help, which
        //  would only say the same.
        TEST_METHOD (EngineMarker_UnknownWord_IsNotACommand)
        {
            Assert::AreEqual (std::string ("!frob is not a command."), ParseFails ("!frob", ParseStatus::Unknown).error);
            Assert::IsTrue   (ParseFails ("!hgr", ParseStatus::NotAvailable).error.starts_with ("HGR"), L"a command of another mode gives the line help gives");
        }

        //  Nothing after the one argument is dropped without a word.
        TEST_METHOD (Breakpoints_ExtraArguments_AreErrors)
        {
            for (const char * line : { "bp 300 5", "bc 1 2 3", "bd 1 2", "be 1 2" })
            {
                ParseFails (line, ParseStatus::Invalid);
            }
        }

        TEST_METHOD (Lengths_AreInTheCommandsUnits)
        {
            Assert::AreEqual ((Word) 0x201F, ParseOk ("db 2000 l20").command.a2);
            Assert::AreEqual ((Word) 0x201F, ParseOk ("db 2000 L 20").command.a2);
            Assert::AreEqual ((Word) 0x203F, ParseOk ("dw 2000 l20").command.a2);
            Assert::AreEqual ((Word) 0x207F, ParseOk ("dd 2000 l20").command.a2);
            Assert::AreEqual ((Word) 0x207F, ParseOk ("db 2000").command.a2);
            Assert::AreEqual ((Word) 0x2010, ParseOk ("db 2000 2010").command.a2);
            Assert::AreEqual ((Word) 0xFFFF, ParseOk ("db ffc0 l100").command.a2);
            ParseFails ("db 2000 l0", ParseStatus::Invalid);
        }

        TEST_METHOD (AccessBreakpoints_ReadWriteExecute)
        {
            Assert::AreEqual ((int) DebugVerb::SetMemoryWatchpoint, (int) ParseOk ("ba r1 c000").command.verb);
            Assert::AreEqual ((int) DebugVerb::SetWriteWatchpoint, (int) ParseOk ("ba w1 400").command.verb);
            Assert::AreEqual ((int) DebugVerb::SetBreakpoint,      (int) ParseOk ("ba e1 300").command.verb);
            Assert::AreEqual ((Word) 0x0403,                        ParseOk ("ba w4 400").command.a2);
            ParseFails ("ba x1 300", ParseStatus::Invalid);
            ParseFails ("ba w0 300", ParseStatus::Invalid);
            ParseFails ("ba w1",     ParseStatus::Invalid);
        }

        //  WinDbg's ba size is decimal, so w10 covers ten bytes, not sixteen.
        TEST_METHOD (AccessBreakpoints_SizeIsDecimal)
        {
            Assert::AreEqual ((Word) 0x0409, ParseOk ("ba w10 400").command.a2);
            Assert::AreEqual ((Word) 0x040F, ParseOk ("ba r16 400").command.a2);
        }

        //  A bare a is hex, as WinDbg reads it; @a is the register.
        TEST_METHOD (BareA_IsHex_AndAtAIsTheRegister)
        {
            ControllerRig  rig;
            Reply          reply;



            rig.Run ("r a=41", CommandMode::WinDbg);
            rig.Run ("eb 300 a", CommandMode::WinDbg);
            Assert::AreEqual ((Byte) 0x0A, rig.machine.GetMemoryBus().ReadByte (0x0300));

            rig.Run ("eb 301 @a", CommandMode::WinDbg);
            Assert::AreEqual ((Byte) 0x41, rig.machine.GetMemoryBus().ReadByte (0x0301));

            reply = rig.Run ("? a+1", CommandMode::WinDbg);
            Assert::AreEqual (std::string ("Evaluate expression: 11 = 000b"), reply.text[0]);
        }

        TEST_METHOD (ClearDisableEnable_TakeAnIdOrStar)
        {
            Assert::AreEqual ((int) DebugVerb::ClearBreakpoint,   (int) ParseOk ("bc *").command.verb);
            Assert::AreEqual (std::string ("*"),                   ParseOk ("bc *").command.text);
            Assert::AreEqual ((int) DebugVerb::DisableBreakpoint, (int) ParseOk ("bd 1").command.verb);
            Assert::AreEqual ((uint32_t) 2,                        ParseOk ("be 2").command.count);
            Assert::AreEqual ((int) DebugVerb::ListBreakpoints,   (int) ParseOk ("bl").command.verb);
        }

        TEST_METHOD (Registers_ShowAndSet)
        {
            Assert::AreEqual ((int) DebugVerb::ShowRegisters, (int) ParseOk ("r").command.verb);
            Assert::AreEqual ((int) DebugVerb::SetRegister,   (int) ParseOk ("r a=41").command.verb);
            Assert::AreEqual ((Word) 0x41,                     ParseOk ("r a=41").command.a1);
            Assert::AreEqual (std::string ("S"),               ParseOk ("r sp = f0").command.text);
            Assert::AreEqual (std::string ("PC"),              ParseOk ("r pc=300").command.text);
            ParseFails ("r q=1", ParseStatus::Invalid);
        }

        TEST_METHOD (Registers_ANameAloneShowsThem)
        {
            Assert::AreEqual ((int) DebugVerb::ShowRegisters, (int) ParseOk ("r a").command.verb);
            Assert::AreEqual ((int) DebugVerb::ShowRegisters, (int) ParseOk ("r fl").command.verb);
            ParseFails ("r q", ParseStatus::Invalid);
        }

        TEST_METHOD (Eb_TakesBytesOnly)
        {
            WinDbgParseResult  result = ParseFails ("eb 300 1234", ParseStatus::Invalid);



            Assert::IsTrue   (result.error.find ("1234") != std::string::npos, Widen (result.error).c_str());
            Assert::AreEqual ((size_t) 2, ParseOk ("eb 300 ff 0").command.values.size());
        }

        TEST_METHOD (HugeLengths_StopAtTheEndOfMemory)
        {
            Assert::AreEqual ((Word) 0xFFFF, ParseOk ("dd 300 l40000000").command.a2);
        }

        TEST_METHOD (CommonExtensionsAndPointerDumps_ReportTheirFamily)
        {
            for (const char * line : { "!peb", "!teb", "!heap -s", "!handle", "!process 0 0", "!thread", "!address", "!gle",
                                       "dps 300", "dds 300", "dqs 300", "dpa 300", "dpu 300", "! analyze -v" })
            {
                WinDbgParseResult  result = ParseFails (line, ParseStatus::NotAvailable);



                Assert::IsTrue (result.error.find ("belongs to WinDbg's") != std::string::npos, Widen (std::string (line) +": " + result.error).c_str());
            }
        }

        TEST_METHOD (Breakpoint_WithMoreThanTheAddress_IsAnError)
        {
            ParseFails ("bp 300 + 3",     ParseStatus::Invalid);
            ParseFails ("bp 300 \"r; g\"", ParseStatus::Invalid);
        }

        TEST_METHOD (Step_TakesACount)
        {
            Assert::AreEqual ((uint32_t) 3, ParseOk ("p 3").command.count);
        }

        TEST_METHOD (SourceLines_WithAndWithoutBackquotes)
        {
            for (const char * line : { "bp main.s:12", "bp `main.s:12`" })
            {
                WinDbgParseResult  result = ParseOk (line);



                Assert::AreEqual ((int) DebugVerb::SetSourceBreakpoint, (int) result.command.verb, Widen (line).c_str());
                Assert::AreEqual (std::string ("main.s"),                result.command.text);
                Assert::AreEqual ((uint32_t) 12,                         result.command.count);
            }
        }

        TEST_METHOD (Steps_AndRuns)
        {
            Assert::AreEqual ((int) DebugVerb::StepInto, (int) ParseOk ("t").command.verb);
            Assert::AreEqual ((int) DebugVerb::StepOver, (int) ParseOk ("p").command.verb);
            Assert::AreEqual ((int) DebugVerb::StepOut,  (int) ParseOk ("gu").command.verb);
            Assert::AreEqual ((int) DebugVerb::Go,       (int) ParseOk ("g").command.verb);
            Assert::AreEqual ((Word) 0x0310,              ParseOk ("pa 310").command.a1);
            Assert::AreEqual ((Word) 0x0310,              ParseOk ("ta 310").command.a1);
            ParseFails ("pa", ParseStatus::Invalid);
        }

        TEST_METHOD (Memory_EditFillSearchMove)
        {
            WinDbgParseResult  text = ParseOk ("ea 400 \"HI\"");



            Assert::AreEqual ((size_t) 2,     text.command.values.size());
            Assert::AreEqual ((Byte) 'H',     text.command.values[0]);
            Assert::AreEqual ((size_t) 2,     ParseOk ("eb 300 a9 41").command.values.size());
            Assert::AreEqual ((int) DebugVerb::EnterWords,  (int) ParseOk ("ew 300 1234").command.verb);
            Assert::AreEqual ((int) DebugVerb::FillMemory,  (int) ParseOk ("f 2000 l20 00").command.verb);
            Assert::AreEqual ((int) DebugVerb::SearchMemory,(int) ParseOk ("s 300 l100 a9 41").command.verb);
            Assert::AreEqual ((Word) 0x2000,                ParseOk ("m 300 l10 2000").command.a3);
            ParseFails ("f 2000 00", ParseStatus::Invalid);
        }

        TEST_METHOD (Other_Commands)
        {
            Assert::AreEqual ((int) DebugVerb::Disassemble,       (int) ParseOk ("u 300").command.verb);
            Assert::AreEqual ((int) DebugVerb::LookupSymbol,      (int) ParseOk ("x cout*").command.verb);
            Assert::AreEqual ((int) DebugVerb::ShowCallStack,     (int) ParseOk ("k").command.verb);
            Assert::AreEqual ((Word) 0x0310,                       ParseOk ("? 300+10").command.a1);
            Assert::AreEqual ((Word) 0x0310,                       ParseOk ("?300+10").command.a1);
            Assert::AreEqual ((Word) 0x41,                         ParseOk (".formats 41").command.a1);
            Assert::AreEqual ((int) DebugVerb::SetSourceStepping, (int) ParseOk ("l+s").command.verb);
            Assert::AreEqual ((uint32_t) 0,                        ParseOk ("l-s").command.count);
            Assert::AreEqual ((int) DebugVerb::ShowSource,        (int) ParseOk ("lsa").command.verb);
        }

        //  `!` reaches Casso's commands, WinDbg's own forms among them, so a
        //  control or script can send one line in any mode; a name Casso
        //  lacks is unknown.
        TEST_METHOD (Bang_ReachesTheEngineCommands)
        {
            WinDbgParseResult  mode = ParseOk ("!mode applewin");



            Assert::AreEqual ((int) DebugVerb::SetMode,      (int) mode.command.verb);
            Assert::AreEqual ((int) CommandMode::AppleWin,   (int) mode.command.mode);
            Assert::AreEqual ((int) CommandMode::WinDbg,     (int) ParseOk ("!MODE WINDBG").command.mode);
            Assert::AreEqual ((int) DebugVerb::ShowSwitches, (int) ParseOk ("!switches").command.verb);
            Assert::AreEqual ((int) DebugVerb::ListStepFilter, (int) ParseOk ("!skip").command.verb);
            Assert::AreEqual ((int) DebugVerb::SetBreakpoint, (int) ParseOk ("!bp 300").command.verb);
            Assert::AreEqual ((int) DebugVerb::DiskCommand,   (int) ParseOk ("!disk").command.verb);
            ParseFails ("!frob",   ParseStatus::Unknown);
        }

        //  Every excluded example replies with its family, never unknown.
        TEST_METHOD (Exclusions_NameTheirFamily)
        {
            size_t  checked = 0;



            for (const WinDbgExclusion & exclusion : WinDbgParser::GetExclusions())
            {
                WinDbgParseResult  result = ParseFails (exclusion.name, ParseStatus::NotAvailable);



                Assert::AreEqual (std::string ("no meaning on this machine"), result.label, Widen (exclusion.name).c_str());
                Assert::IsTrue   (result.error.find (exclusion.family) != std::string::npos, Widen (exclusion.name).c_str());
                ++checked;
            }

            Assert::IsTrue (checked > 30);

            for (const char * line : { "~0s", "sxn av", "!ext.help", "$t1", "|1s" })
            {
                Assert::AreEqual (std::string ("no meaning on this machine"), ParseFails (line, ParseStatus::NotAvailable).label, Widen (line).c_str());
            }
        }

        TEST_METHOD (Deferred_SayWhy)
        {
            Assert::IsTrue (ParseFails ("wt",           ParseStatus::NotAvailable).label.empty());
            Assert::IsTrue (ParseFails ("s -a 300 l10", ParseStatus::NotAvailable).label.empty());
            Assert::IsTrue (ParseFails ("lsa main.s:3", ParseStatus::NotAvailable).label.empty());
        }

        TEST_METHOD (PrefixedLengths_AreNumbers)
        {
            Assert::AreEqual ((Word) 0x201F, ParseOk ("db 2000 l0x20").command.a2);
            Assert::AreEqual ((Word) 0x201F, ParseOk ("db 2000 l0n32").command.a2);
            Assert::AreEqual ((Word) 0x201F, ParseOk ("db 2000 L0X20").command.a2);
            ParseOk ("f 300 l0x20 00");
            ParseOk ("s 300 l0x20 00");
        }

        TEST_METHOD (DumpRanges_EndAtOrAfterTheirStart)
        {
            ParseFails ("db 2010 2000", ParseStatus::Invalid);
            Assert::AreEqual ((Word) 0xFFFF, ParseOk ("dd ffff l40000000").command.a2);
        }

        TEST_METHOD (LengthRanges_StopAtTheTopOfMemory)
        {
            Assert::AreEqual ((Word) 0xFFFF, ParseOk ("f fff0 l10 00").command.a2);
            ParseFails ("f fff0 l20 00",   ParseStatus::Invalid);
            ParseFails ("s fff0 l20 00",   ParseStatus::Invalid);
            ParseFails ("m fff0 l20 2000", ParseStatus::Invalid);
            ParseFails ("ba w4 fffe",      ParseStatus::Invalid);
        }

        //  The engine stops a run at one address; a second would be taken as
        //  AppleWin's skip range and end the run after one instruction.
        TEST_METHOD (Go_TakesOneAddress)
        {
            Assert::AreEqual ((Word) 0x0300, ParseOk ("g 300").command.a1);
            ParseFails ("g 300 310", ParseStatus::Invalid);
        }

        TEST_METHOD (SingleQuotedText_KeepsItsCharacters)
        {
            WinDbgParseResult  result = ParseOk ("s 300 l100 '0x41'");



            Assert::AreEqual ((size_t) 4, result.command.values.size());
            Assert::AreEqual ((Byte) ('0' | 0x80), result.command.values[0]);
        }

        TEST_METHOD (EngineText_KeepsPrefixesThatAreNotNumbers)
        {
            Assert::AreEqual (std::string ("0x41 and 0n10"), ParseOk ("!echo 0x41 and 0n10").command.text);
            Assert::AreEqual (std::string ("C:\\0x1.txt"),   ParseOk ("!run C:\\0x1.txt").command.text);
            Assert::AreEqual (std::string ("D:/0x2.bin"),    ParseOk ("!bload D:/0x2.bin 0x300").command.text);
            Assert::AreEqual ((Word) 0x0300,                 ParseOk ("!bload D:/0x2.bin 0x300").command.a1);
        }

        //  x only looks up, so a symbol whose name is a SYM subcommand is found.
        TEST_METHOD (SymbolLookup_NeverRunsTheSymSubcommands)
        {
            for (const char * line : { "x clear", "x on", "x off", "x load t.sym", "x save t.sym", "x name = 300", "x ! name" })
            {
                WinDbgParseResult  result = ParseOk (line);



                Assert::AreEqual ((int) DebugVerb::LookupSymbol, (int) result.command.verb, Widen (line).c_str());
                Assert::AreEqual (std::string (line + 2),         result.command.text,        Widen (line).c_str());
            }
        }

        TEST_METHOD (EnterWithoutValues_SaysWhatIsMissing)
        {
            for (const char * line : { "eb 300", "ew 300", "eb", "ew" })
            {
                ParseFails (line, ParseStatus::Invalid);
            }
        }

        TEST_METHOD (QuotedSearchText_KeepsItsSpaces)
        {
            WinDbgParseResult  result = ParseOk ("s 300 l100 \"A  B\"");



            Assert::AreEqual ((size_t) 4, result.command.values.size());
            Assert::AreEqual ((Byte) ' ', (Byte) (result.command.values[2] & 0x7F));
        }

        TEST_METHOD (Help_TakesACassoCommandWithItsMarker)
        {
            Assert::AreEqual (std::string ("!mode"), ParseOk (".help !mode").command.text);
            Assert::AreEqual (std::string ("bp"),    ParseOk (".help bp").command.text);
        }

        //  s searches for bytes, so a prefixed number that fits a byte is
        //  one byte, not a word with a zero high byte.
        TEST_METHOD (Search_PrefixedByteIsOneByte)
        {
            for (const char * line : { "s 300 l100 0x41", "s 300 l100 0n65" })
            {
                WinDbgParseResult  result = ParseOk (line);



                Assert::AreEqual ((size_t) 1,  result.command.values.size(), Widen (line).c_str());
                Assert::AreEqual ((Byte) 0x41, result.command.values[0],     Widen (line).c_str());
            }
        }

        //  A `!` line keeps text and file paths as typed, and a decimal
        //  argument takes WinDbg's prefixes as their values.
        TEST_METHOD (Bang_KeepsTextAndPaths_AndReadsDecimalPrefixes)
        {
            Assert::AreEqual (std::string ("0x41"),          ParseOk ("!echo 0x41").command.text);
            Assert::AreEqual (std::string ("C:/t/0x10.bin"), ParseOk ("!bsave C:/t/0x10.bin 0x300:30f").command.text);
            Assert::AreEqual ((Word) 0x0300,                 ParseOk ("!bsave C:/t/0x10.bin 0x300:30f").command.a1);
            Assert::AreEqual (std::string ("0x10.bin"),      ParseOk ("!bload 0x10.bin 300").command.text);
            Assert::AreEqual ((uint32_t) 1000,               ParseOk ("!budget 0n1000").command.count);
            Assert::AreEqual ((uint32_t) 256,                ParseOk ("!budget 0x100").command.count);
            Assert::AreEqual ((uint32_t) 1000,               ParseOk ("!budget 1000").command.count);
        }

        //  A reply quotes the command as it was typed in WinDbg mode, not
        //  the AppleWin command it was rewritten to.
        TEST_METHOD (SourceName_IsTheWinDbgName)
        {
            Assert::AreEqual (std::string ("eb"),      ParseOk ("eb 300 41").command.sourceName);
            Assert::AreEqual (std::string ("r"),       ParseOk ("r a=41").command.sourceName);
            Assert::AreEqual (std::string ("!budget"), ParseOk ("!budget 5").command.sourceName);
        }

        TEST_METHOD (Malformed_ProducesAnErrorAndNoCommand)
        {
            Assert::AreEqual ((int) DebugVerb::None, (int) ParseFails ("db zzz_nope",  ParseStatus::Invalid).command.verb);
            Assert::AreEqual ((int) DebugVerb::None, (int) ParseFails ("frobnicate",   ParseStatus::Unknown).command.verb);
            Assert::AreEqual ((int) DebugVerb::None, (int) ParseFails ("k 3",          ParseStatus::Invalid).command.verb);
        }
    };
}
