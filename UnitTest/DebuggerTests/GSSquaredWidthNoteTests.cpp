#include "Pch.h"

#include "Debugger/GSSquaredParser.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GSSquaredWidthNoteTests
    //
    //  FR-134: `m` and `x` say that they set 65816 register widths and do not
    //  apply to this machine's CPU.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (GSSquaredWidthNoteTests)
    {
    public:
        TEST_METHOD (MAndX_SayTheySet65816RegisterWidths)
        {
            MockExpressionContext  context;



            for (const char * line : { "m", "m 8", "x", "x 16" })
            {
                GSSquaredParseResult  result = GSSquaredParser::Parse (line, context);
                std::wstring          where  (result.error.begin(), result.error.end());



                Assert::AreEqual ((int) ParseStatus::NotAvailable, (int) result.status, where.c_str());
                Assert::IsTrue   (result.error.find ("65816 register width") != std::string::npos, where.c_str());
                Assert::IsTrue   (result.error.find ("does not apply to this machine's CPU") != std::string::npos, where.c_str());
            }
        }
    };
}
