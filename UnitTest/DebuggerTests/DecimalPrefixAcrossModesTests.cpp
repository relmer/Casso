#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "Debugger/CommandModeHelp.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/MonitorParser.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"
#include "Debugger/MonitorState.h"
#include "Core/TextEncoding.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DecimalPrefixAcrossModesTests
    //
    //  0n marks a decimal number in AppleWin, Casso and GSSquared modes, as it
    //  does in WinDbg mode, everywhere a number is read. Monitor mode stays hex
    //  only.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DecimalPrefixAcrossModesTests)
    {
    public:

        static std::wstring Widen (const std::string & text)
        {
            return TextEncoding::NarrowToWide (text);
        }

        static int32_t Evaluate (const std::string & text)
        {
            MockExpressionContext  context;
            std::string            error;
            int32_t                value = 0;
            HRESULT                hr    = DebugExpressionEvaluator::ParseAndEvaluate (text, context, value, error);



            Assert::AreEqual (S_OK, hr, Widen (text + ": " + error).c_str());
            return value;
        }

        static DebugCommand AppleWinOne (const std::string & line)
        {
            MockExpressionContext  context;
            AppleWinParseResult    result = AppleWinParser::Parse (line, context);



            Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status, Widen (line + ": " + result.error).c_str());
            return result.command;
        }

        static GSSquaredParseResult GSSquaredParse (const std::string & line)
        {
            MockExpressionContext  context;



            return GSSquaredParser::Parse (line, context);
        }

        static DebugCommand GSSquaredOne (const std::string & line)
        {
            GSSquaredParseResult  result = GSSquaredParse (line);



            Assert::IsTrue   (result.status == ParseStatus::Ok, Widen (line + ": " + result.error).c_str());
            Assert::AreEqual (size_t (1), result.commands.size(), Widen (line).c_str());
            return result.commands.front();
        }



        TEST_METHOD (AppleWin_ExpressionReads0nAsDecimal)
        {
            MockExpressionContext  context;
            std::string            error;
            int32_t                value = 0;
            HRESULT                hr    = S_OK;



            Assert::AreEqual (10,    Evaluate ("0n10"));
            Assert::AreEqual (26,    Evaluate ("0n10+$10"));
            Assert::AreEqual (0x300, Evaluate ("0N768"));

            hr = DebugExpressionEvaluator::ParseAndEvaluate ("0n1A", context, value, error);
            Assert::IsTrue (FAILED (hr));
            Assert::IsTrue (error.find ("not a decimal number") != std::string::npos, Widen (error).c_str());

            //  0x stays WinDbg's alone.
            hr = DebugExpressionEvaluator::ParseAndEvaluate ("0x300", context, value, error);
            Assert::IsTrue (FAILED (hr));
        }



        TEST_METHOD (AppleWin_CommandsRead0n)
        {
            DebugCommand  memory  = AppleWinOne ("D 0n768");
            DebugCommand  clear   = AppleWinOne ("BPC 0n3");
            DebugCommand  history = AppleWinOne ("HISTORY 0n100 0n20");



            Assert::AreEqual ((int) 0x300, (int) memory.a1);
            Assert::AreEqual ((uint32_t) 3, clear.count);
            Assert::AreEqual ((uint64_t) 100, history.first.value_or (0));
            Assert::AreEqual ((uint32_t) 20,  history.count);
        }



        TEST_METHOD (GSSquared_AddressesBytesAndIdsRead0n)
        {
            DebugCommand  examine = GSSquaredOne ("0n768");
            DebugCommand  deposit = GSSquaredOne ("300: 0n65 41");
            DebugCommand  range   = GSSquaredOne ("0n768.0n783");
            DebugCommand  clear   = GSSquaredOne ("nobp 0n3");
            DebugCommand  banked  = GSSquaredOne ("0n0/0n768");



            Assert::AreEqual ((int) 0x300, (int) examine.a1);
            Assert::AreEqual (size_t (2),  deposit.values.size());
            Assert::AreEqual ((int) 0x41,  (int) deposit.values[0]);
            Assert::AreEqual ((int) 0x300, (int) range.a1);
            Assert::AreEqual ((int) 0x30F, (int) range.a2);
            Assert::AreEqual ((uint32_t) 3, clear.count);
            Assert::AreEqual ((int) 0x300, (int) banked.a1);
        }



        TEST_METHOD (GSSquared_0nOutOfRangeIsAnError)
        {
            Assert::IsTrue (GSSquaredParse ("300: 0n256").status != ParseStatus::Ok);
            Assert::IsTrue (GSSquaredParse ("0n65536").status    != ParseStatus::Ok);
            Assert::IsTrue (GSSquaredParse ("0n12x").status      != ParseStatus::Ok);
        }



        TEST_METHOD (Monitor_StaysHexOnly)
        {
            MonitorState        state;
            MonitorParseResult  result = MonitorParser::Parse ("0n768", state);



            Assert::IsTrue (result.status != ParseStatus::Ok || result.commands.empty() || result.commands.front().a1 != 0x300);
        }



        TEST_METHOD (Help_SaysWhichModesRead0n)
        {
            auto  hasNote = [] (CommandMode mode)
            {
                std::vector<std::string>  lines = CommandModeHelp::BuildSectionIndex (mode);



                return std::any_of (lines.begin(), lines.end(), [] (const std::string & line) { return line.find ("0n") != std::string::npos; });
            };



            Assert::IsTrue  (hasNote (CommandMode::AppleWin));
            Assert::IsTrue  (hasNote (CommandMode::Casso));
            Assert::IsTrue  (hasNote (CommandMode::GSSquared));
            Assert::IsTrue  (hasNote (CommandMode::WinDbg));
            Assert::IsFalse (hasNote (CommandMode::Monitor));
            Assert::IsTrue  (CommandModeHelp::BuildReference().find ("0n marks decimal") != std::string::npos);
        }
    };
}
