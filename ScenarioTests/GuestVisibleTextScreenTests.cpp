#include "Pch.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleTextScreenTests
//
//  What the scraper reads off a screen the guest's own firmware drew. Under
//  DOS 3.3, PR#3 routes output to the //e's 80-column firmware, which puts
//  even columns in aux and odd columns in main at the same addresses; a
//  scrape that reads one bank returns every other character.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleTextScreenTests)
{
public:

    static constexpr const char *  kPrinted = "HELLO THERE";


    TEST_METHOD (Dos33_PR3ThenPrint_ScrapesEveryColumn)
    {
        std::vector<Byte>          master = GuestSession::RequireDos33Master();
        TestMachine                machine ("Apple2e");
        std::vector<std::string>   rows;
        std::string                echo   = std::string ("]PRINT \"") + kPrinted + "\"";



        GuestSession::BootToPrompt (machine, master);
        GuestSession::TypeAndCollect (machine, "PR#3");

        Assert::IsTrue (machine.GetRefs().iieSoftSwitches->Is80ColMode(),
            L"fixture: 80COL must be on after PR#3");

        rows = GuestSession::TypeAndCollect (machine, std::string ("PRINT \"") + kPrinted + "\"");

        Assert::IsTrue (GuestSession::AnyRowIs (rows, echo),
            L"the typed command reads back whole, both banks interleaved");
        Assert::IsTrue (GuestSession::AnyRowIs (rows, kPrinted),
            L"and so does what it printed");
    }
};
