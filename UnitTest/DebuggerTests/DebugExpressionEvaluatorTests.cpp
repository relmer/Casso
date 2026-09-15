#include "Pch.h"

#include "Debugger/DebugExpressionEvaluator.h"
#include "MockExpressionContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluatorTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebugExpressionEvaluatorTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////
        //
        //  Expect
        //
        ////////////////////////////////////////////////////////////////////////

        static void Expect (const MockExpressionContext & context, const std::string & text, int32_t expected)
        {
            std::string  error;
            int32_t      value = 0;
            HRESULT      hr    = S_OK;



            hr = DebugExpressionEvaluator::ParseAndEvaluate (text, context, value, error);

            Assert::AreEqual (S_OK,     hr,    std::wstring (text.begin(), text.end()).c_str());
            Assert::AreEqual (expected, value, std::wstring (text.begin(), text.end()).c_str());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ExpectError
        //
        //  Asserts failure, and that the error text contains fragment.
        //
        ////////////////////////////////////////////////////////////////////////

        static void ExpectError (const MockExpressionContext & context, const std::string & text, const std::string & fragment)
        {
            std::string  error;
            int32_t      value = 0;
            HRESULT      hr    = S_OK;



            hr = DebugExpressionEvaluator::ParseAndEvaluate (text, context, value, error);

            Assert::IsTrue (FAILED (hr),                             std::wstring (text.begin(), text.end()).c_str());
            Assert::IsTrue (error.find (fragment) != std::string::npos, std::wstring (error.begin(), error.end()).c_str());
        }



        TEST_METHOD (Numbers)
        {
            MockExpressionContext context;



            Expect (context, "300",       0x300);
            Expect (context, "$300",      0x300);
            Expect (context, "$a",        0xA);
            Expect (context, "0A",        0xA);
            Expect (context, "#10",       10);
            Expect (context, "'A'",       0x41);
            Expect (context, "  1FF  ",   0x1FF);
            Expect (context, "FFFFFFFF",  -1);
        }



        TEST_METHOD (Operators)
        {
            MockExpressionContext context;



            Expect (context, "300+10",    0x310);
            Expect (context, "10-20",     -0x10);
            Expect (context, "FF/10",     0xF);
            Expect (context, "FF//10",    0xF);
            Expect (context, "FF%10",     0xF);
            Expect (context, "F0&3C",     0x30);
            Expect (context, "F0|0F",     0xFF);
            Expect (context, "FF^0F",     0xF0);
            Expect (context, "!0",        0xFFFF);
            Expect (context, "-1",        -1);
            Expect (context, "<1234",     0x34);
            Expect (context, ">1234",     0x12);
            Expect (context, "3<4",       1);
            Expect (context, "4<=3",      0);
            Expect (context, "4>=4",      1);
            Expect (context, "5=5",       1);
            Expect (context, "5==6",      0);
            Expect (context, "5!=6",      1);
        }



        TEST_METHOD (PrecedenceAndParentheses)
        {
            MockExpressionContext context;



            Expect (context, "2*3+4",     0xA);
            Expect (context, "2+3*4",     0xE);
            Expect (context, "(2+3)*4",   0x14);
            Expect (context, "10-4-2",    0xA);
            Expect (context, "-(2+3)",    -5);
            Expect (context, "!-1",       0);
            Expect (context, "1|2&3",     3);
            Expect (context, "<(1234+1)", 0x35);
        }



        TEST_METHOD (Registers)
        {
            MockExpressionContext context;



            Expect (context, "A",         0x41);
            Expect (context, "a",         0x41);
            Expect (context, "PC+2",      0x302);
            Expect (context, "S",         0xFF);
            Expect (context, "X+Y",       0x3);
            Expect (context, "A=41",      1);
        }



        TEST_METHOD (MemoryDereference)
        {
            MockExpressionContext context;



            context.memory[0x0300] = 0xA9;
            context.memory[0x0301] = 0x42;

            Expect      (context, "*300",     0xA9);
            Expect      (context, "*PC",      0xA9);
            Expect      (context, "*(PC+1)",  0x42);
            Expect      (context, "2**300",   0x152);
            ExpectError (context, "*C000",    "$C000");
        }



        TEST_METHOD (Symbols)
        {
            MockExpressionContext context;



            Expect      (context, "HOME",     0xFC58);
            Expect      (context, "home+1",   0xFC59);
            Expect      (context, "COUT-HOME", 0x195);
            ExpectError (context, "NOSUCH",   "NOSUCH");
        }



        TEST_METHOD (Errors)
        {
            MockExpressionContext context;



            ExpectError (context, "",         "incomplete");
            ExpectError (context, "3+",       "incomplete");
            ExpectError (context, "(3",       "parentheses");
            ExpectError (context, "3)",       "parentheses");
            ExpectError (context, "10/0",     "divides by zero");
            ExpectError (context, "10%0",     "divides by zero");
            ExpectError (context, "$",        "column");
            ExpectError (context, "#12A",     "decimal");
            ExpectError (context, "3G",       "hex");
            ExpectError (context, "3 4",      "operator");
            ExpectError (context, "'A",       "'c'");
            ExpectError (context, "123456789", "hex");
        }



        TEST_METHOD (ParsedOnce_EvaluatedAgainstCurrentState)
        {
            MockExpressionContext context;
            Expression            expression;
            std::string           error;
            int32_t               value = 0;
            HRESULT               hr    = S_OK;



            hr = DebugExpressionEvaluator::Parse ("A=0", expression, error);
            Assert::AreEqual (S_OK, hr);
            Assert::IsFalse  (expression.postfix.empty());

            hr = DebugExpressionEvaluator::Evaluate (expression, context, value, error);
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (0,    value);

            context.registers["A"] = 0;

            hr = DebugExpressionEvaluator::Evaluate (expression, context, value, error);
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (1,    value);
        }
    };
}
