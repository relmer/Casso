#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/WinDbgParser.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"
#include "Core/TextEncoding.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CommandTextAsTypedTests
    //
    //  Each mode's parser builds its command from the line as typed: no
    //  command text is rewritten into another mode's form and parsed again,
    //  so every reply quotes the user's own words.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CommandTextAsTypedTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return TextEncoding::NarrowToWide (text);
        }

        static WinDbgParseResult ParseWinDbg (const std::string & line)
        {
            MockExpressionContext  context;



            return WinDbgParser::Parse (line, context);
        }

        static std::string Joined (const Reply & reply)
        {
            std::string  all = reply.error.detail;



            for (const std::string & line : reply.text)
            {
                all += "\n" + line;
            }

            return all;
        }



        //  A breakpoint id after `!` takes WinDbg's prefixes, as the same id
        //  without the `!` does.
        TEST_METHOD (WinDbg_BreakpointIdAfterBang_Reads0n)
        {
            for (const char * line : { "!bpc 0n3", "!bpd 0n3", "!bpe 0n3", "!bpchange 0n3 E", "bc 0n3" })
            {
                WinDbgParseResult  result = ParseWinDbg (line);



                Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status, Widen (std::string (line) + ": " + result.error).c_str());
                Assert::AreEqual ((uint32_t) 3, result.command.count, Widen (line).c_str());
            }
        }

        //  A condition after `!` reads WinDbg's prefixes too.
        TEST_METHOD (WinDbg_ConditionAfterBang_Reads0x)
        {
            WinDbgParseResult  result = ParseWinDbg ("!bp 300 IF A == 0x41");



            Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status, Widen (result.error).c_str());
            Assert::AreEqual (std::string ("A == 0x41"), result.command.expression.text);
        }

        //  A number joined to an operator by a slash was taken for a path and
        //  left unread; the expression reads it now.
        TEST_METHOD (WinDbg_PrefixedNumbersBesideSlash_AreRead)
        {
            WinDbgParseResult  result = ParseWinDbg ("? 0n10/0n2");



            Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status, Widen (result.error).c_str());
            Assert::AreEqual ((Word) 5, result.command.a1);
        }

        //  A condition keeps the registers and numbers as typed.
        TEST_METHOD (WinDbg_ConditionText_IsAsTyped)
        {
            WinDbgParseResult  result = ParseWinDbg ("bp 300 IF @a == 0n10");



            Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status, Widen (result.error).c_str());
            Assert::AreEqual (std::string ("@a == 0n10"), result.command.expression.text);
        }

        //  WinDbg reads a bare a as a number and @a as the register.
        TEST_METHOD (WinDbg_BareA_IsANumber_AtA_IsTheRegister)
        {
            Assert::AreEqual ((Word) 0x0A, ParseWinDbg ("? a").command.a1);
            Assert::AreEqual ((Word) 0x0B, ParseWinDbg ("? a+1").command.a1);
        }

        TEST_METHOD (WinDbg_ErrorQuotesNumberAsTyped)
        {
            WinDbgParseResult  result = ParseWinDbg ("db 0x10000");



            Assert::AreEqual ((int) ParseStatus::Invalid, (int) result.status);
            Assert::AreEqual (std::string ("0x10000 is outside $0000-$FFFF."), result.error);
        }

        TEST_METHOD (WinDbg_ReplyQuotesLineAndNumbersAsTyped)
        {
            ControllerRig  rig;
            Reply          reply = rig.Run ("db 0x10000", CommandMode::WinDbg);



            Assert::AreEqual (std::string ("db 0x10000"), reply.command);
            Assert::IsTrue   (Joined (reply).find ("0x10000") != std::string::npos, Widen (Joined (reply)).c_str());
            Assert::IsTrue   (Joined (reply).find ("$10000")  == std::string::npos, Widen (Joined (reply)).c_str());
        }

        TEST_METHOD (WinDbg_BreakpointList_QuotesConditionAsTyped)
        {
            ControllerRig  rig;
            Reply          reply;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Run ("bp 300 IF @a == 0n10", CommandMode::WinDbg).status);
            reply = rig.Run ("bl", CommandMode::WinDbg);

            Assert::IsTrue (Joined (reply).find ("@a == 0n10") != std::string::npos, Widen (Joined (reply)).c_str());
        }

        TEST_METHOD (AppleWin_ReplyQuotesLineAndNumbersAsTyped)
        {
            ControllerRig  rig;
            Reply          reply = rig.Run ("BPX #70000", CommandMode::AppleWin);



            Assert::AreEqual (std::string ("BPX #70000"), reply.command);
            Assert::IsTrue   (Joined (reply).find ("#70000") != std::string::npos, Widen (Joined (reply)).c_str());
        }

        TEST_METHOD (GSSquared_ReplyQuotesLineAndNumbersAsTyped)
        {
            ControllerRig  rig;
            Reply          reply = rig.Run ("bp 1FFFF", CommandMode::GSSquared);



            Assert::AreEqual (std::string ("bp 1FFFF"), reply.command);
            Assert::IsTrue   (Joined (reply).find ("1FFFF") != std::string::npos, Widen (Joined (reply)).c_str());
        }
    };
}
