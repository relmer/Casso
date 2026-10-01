#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PanelOpenWordTests
    //
    //  PANEL OPEN name and PANEL name OPEN open a panel, as PANEL name does.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PanelOpenWordTests)
    {
    public:
        TEST_METHOD (OpenWord_EitherOrder_OpensThePanel)
        {
            for (const char * line : { "PANEL disk", "PANEL OPEN disk", "PANEL disk open", "PANEL open disk" })
            {
                MockExpressionContext  context;
                AppleWinParseResult    result = AppleWinParser::Parse (line, context);
                std::wstring           label  (line, line + strlen (line));



                Assert::AreEqual ((int) ParseStatus::Ok,       (int) result.status,       label.c_str());
                Assert::AreEqual ((int) DebugVerb::OpenPanel,  (int) result.command.verb, label.c_str());
                Assert::AreEqual (std::string ("disk"),        result.command.text,       label.c_str());
            }
        }

        TEST_METHOD (OpenWord_KeepsATitleWithASpace)
        {
            MockExpressionContext  context;



            Assert::AreEqual (std::string ("Disk II"), AppleWinParser::Parse ("PANEL OPEN Disk II", context).command.text);
            Assert::AreEqual (std::string ("Disk II"), AppleWinParser::Parse ("PANEL Disk II OPEN", context).command.text);
        }

        TEST_METHOD (OpenWord_Alone_IsAnError)
        {
            MockExpressionContext  context;



            Assert::AreEqual ((int) ParseStatus::Invalid, (int) AppleWinParser::Parse ("PANEL OPEN", context).status);
        }
    };
}
