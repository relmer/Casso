#include "Pch.h"

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
            Assert::AreEqual ((int) DebugVerb::SetReadWatchpoint,  (int) ParseOk ("ba r1 c000").command.verb);
            Assert::AreEqual ((int) DebugVerb::SetWriteWatchpoint, (int) ParseOk ("ba w1 400").command.verb);
            Assert::AreEqual ((int) DebugVerb::SetBreakpoint,      (int) ParseOk ("ba e1 300").command.verb);
            Assert::AreEqual ((Word) 0x0403,                        ParseOk ("ba w4 400").command.a2);
            ParseFails ("ba x1 300", ParseStatus::Invalid);
            ParseFails ("ba w0 300", ParseStatus::Invalid);
            ParseFails ("ba w1",     ParseStatus::Invalid);
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

        TEST_METHOD (Malformed_ProducesAnErrorAndNoCommand)
        {
            Assert::AreEqual ((int) DebugVerb::None, (int) ParseFails ("db zzz_nope",  ParseStatus::Invalid).command.verb);
            Assert::AreEqual ((int) DebugVerb::None, (int) ParseFails ("frobnicate",   ParseStatus::Unknown).command.verb);
            Assert::AreEqual ((int) DebugVerb::None, (int) ParseFails ("k 3",          ParseStatus::Invalid).command.verb);
        }
    };
}
