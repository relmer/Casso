#include "Pch.h"

#include "Shell/CpuCommandDispatcher.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsPayloadTests
//
//  The text an IDM_DEBUG_REVERSE_OPTIONS command carries from Tools > Options
//  on the UI thread to the CPU thread, which owns the reverse host: the
//  recording switch and the budget survive the trip, and text that does not
//  parse asks for nothing.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseOptionsPayloadTests)
{
public:

    TEST_METHOD (TheSwitchAndTheBudgetSurviveTheTrip)
    {
        for (bool isOn : { true, false })
        {
            for (int budget : { 4, 64, 4096 })
            {
                std::string  payload     = CpuCommandDispatcher::FormatReverseOptionsPayload (isOn, budget);
                bool         isRecording = !isOn;
                int          budgetMb    = 0;
                bool         isParsed    = CpuCommandDispatcher::TryParseReverseOptionsPayload (payload, isRecording, budgetMb);



                Assert::IsTrue   (isParsed, TextEncoding::NarrowToWide (payload).c_str());
                Assert::AreEqual (isOn,   isRecording);
                Assert::AreEqual (budget, budgetMb);
            }
        }
    }


    TEST_METHOD (TextThatDoesNotParseAsksForNothing)
    {
        for (const char * text : { "", "on", "off", "on ", "maybe 64", "on 64 1", "on -4", "on 0", "on 64MB", "on  64" })
        {
            bool  isRecording = false;
            int   budgetMb    = 0;



            Assert::IsFalse (CpuCommandDispatcher::TryParseReverseOptionsPayload (text, isRecording, budgetMb),
                             std::wstring (text, text + strlen (text)).c_str());
        }
    }
};
