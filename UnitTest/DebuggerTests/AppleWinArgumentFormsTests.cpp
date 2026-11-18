#include "Pch.h"

#include "Debugger/AppleWinArgumentForms.h"
#include "MockExpressionContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentFormsTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (AppleWinArgumentFormsTests)
    {
    public:

        //  A decimal takes # and 0n, and hex only in WinDbg's syntax.
        TEST_METHOD (Decimal_PrefixesBySyntax)
        {
            uint64_t  value = 0;



            Assert::IsTrue  (AppleWinArgumentForms::TryParseDecimal ("42", value));
            Assert::AreEqual ((uint64_t) 42, value);
            Assert::IsTrue  (AppleWinArgumentForms::TryParseDecimal ("#7", value));
            Assert::AreEqual ((uint64_t) 7, value);
            Assert::IsTrue  (AppleWinArgumentForms::TryParseDecimal ("0n12", value));
            Assert::AreEqual ((uint64_t) 12, value);
            Assert::IsFalse (AppleWinArgumentForms::TryParseDecimal ("0x10", value));
            Assert::IsTrue  (AppleWinArgumentForms::TryParseDecimal ("0x10", value, NumberSyntax::WinDbg));
            Assert::AreEqual ((uint64_t) 16, value);
            Assert::IsFalse (AppleWinArgumentForms::TryParseDecimal ("", value));
            Assert::IsFalse (AppleWinArgumentForms::TryParseDecimal ("12a", value));
        }


        //  A count above 32 bits fails to parse rather than wrapping.
        TEST_METHOD (Count_RejectsAbove32Bits)
        {
            uint32_t  value = 0;



            Assert::IsTrue  (AppleWinArgumentForms::TryParseCount ("4294967295", value));
            Assert::AreEqual (UINT32_MAX, value);
            Assert::IsFalse (AppleWinArgumentForms::TryParseCount ("4294967296", value));
        }


        TEST_METHOD (NumberOrRange_TellsNumbersFromNames)
        {
            Assert::IsTrue  (AppleWinArgumentForms::IsNumberOrRange ("$300.3FF", NumberSyntax::AppleWin));
            Assert::IsTrue  (AppleWinArgumentForms::IsNumberOrRange ("0n10", NumberSyntax::AppleWin));
            Assert::IsTrue  (AppleWinArgumentForms::IsNumberOrRange ("0x300.0x3FF", NumberSyntax::WinDbg));
            Assert::IsFalse (AppleWinArgumentForms::IsNumberOrRange ("0x300", NumberSyntax::AppleWin));
            Assert::IsFalse (AppleWinArgumentForms::IsNumberOrRange ("COUT", NumberSyntax::AppleWin));
        }


        //  A length and a last address result in the same range; a range past
        //  $FFFF or backward is an error.
        TEST_METHOD (Range_LengthOrLastAddress)
        {
            MockExpressionContext  context;
            DebugCommand           byLength;
            DebugCommand           byLast;
            DebugCommand           single;
            DebugCommand           bad;
            std::string            error;



            Assert::IsTrue  (AppleWinArgumentForms::TryParseRange ("300,10", context, byLength, error));
            Assert::IsTrue  (AppleWinArgumentForms::TryParseRange ("300:30F", context, byLast, error));
            Assert::AreEqual ((int) byLast.a1, (int) byLength.a1);
            Assert::AreEqual ((int) byLast.a2, (int) byLength.a2);
            Assert::AreEqual (0x30F, (int) byLast.a2);
            Assert::IsTrue  (AppleWinArgumentForms::TryParseRange ("300", context, single, error));
            Assert::IsTrue  (single.hasA1);
            Assert::IsFalse (single.hasA2);
            Assert::IsFalse (AppleWinArgumentForms::TryParseRange ("FFF0,20", context, bad, error));
            Assert::IsFalse (AppleWinArgumentForms::TryParseRange ("300,0", context, bad, error));
            Assert::IsFalse (AppleWinArgumentForms::TryParseRange ("300:2FF", context, bad, error));
        }


        //  Words are stored low byte first; a byte list fails on a word.
        TEST_METHOD (Values_ByWidth)
        {
            using Width = AppleWinArgumentForms::ValueWidth;

            MockExpressionContext            context;
            AppleWinArgumentForms::Tokens    tokens = { "12", "3456" };
            DebugCommand                     mixed;
            DebugCommand                     words;
            DebugCommand                     bytes;
            std::string                      error;



            Assert::IsTrue  (AppleWinArgumentForms::TryParseValues (tokens, 0, Width::BytesOrWords, context, mixed, error));
            Assert::IsTrue  ((std::vector<Byte> { 0x12, 0x56, 0x34 }) == mixed.values);
            Assert::IsTrue  (AppleWinArgumentForms::TryParseValues (tokens, 0, Width::Words, context, words, error));
            Assert::IsTrue  ((std::vector<Byte> { 0x12, 0x00, 0x56, 0x34 }) == words.values);
            Assert::IsFalse (AppleWinArgumentForms::TryParseValues (tokens, 0, Width::Bytes, context, bytes, error));
            Assert::IsFalse (error.empty());
        }


        TEST_METHOD (IdOrAll_IdStarOrError)
        {
            DebugCommand  id;
            DebugCommand  all;
            DebugCommand  bad;
            std::string   error;



            Assert::IsTrue  (AppleWinArgumentForms::TryParseIdOrAll ({ "3" }, id, error));
            Assert::AreEqual ((uint32_t) 3, id.count);
            Assert::IsTrue  (AppleWinArgumentForms::TryParseIdOrAll ({ "*" }, all, error));
            Assert::AreEqual (std::string ("*"), all.text);
            Assert::IsFalse (AppleWinArgumentForms::TryParseIdOrAll ({}, bad, error));
            Assert::IsFalse (AppleWinArgumentForms::TryParseIdOrAll ({ "x" }, bad, error));
        }
    };
}
