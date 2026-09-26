#include "Pch.h"

#include "Ui/Debugger/ConsoleHistory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleHistoryTests
//
//  Up and Down in the console's command box: the most recent line first, a
//  line run again moves to the most recent place, and the text being typed
//  comes back at the end of the walk.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ConsoleHistoryTests)
    {
    public:

        TEST_METHOD (Older_WalksBackFromTheMostRecent_AndStopsAtTheOldest)
        {
            ConsoleHistory  history;



            history.Add (L"R");
            history.Add (L"BPL");
            history.Add (L"T");

            Assert::AreEqual (std::wstring (L"T"),   history.GetOlder (L"").value());
            Assert::AreEqual (std::wstring (L"BPL"), history.GetOlder (L"").value());
            Assert::AreEqual (std::wstring (L"R"),   history.GetOlder (L"").value());
            Assert::AreEqual (std::wstring (L"R"),   history.GetOlder (L"").value(), L"the oldest line holds");
        }



        TEST_METHOD (Newer_EndsOnWhatWasTyped)
        {
            ConsoleHistory  history;



            history.Add (L"R");
            history.Add (L"T");

            Assert::IsFalse  (history.GetNewer().has_value(), L"nothing is newer outside a walk");
            (void) history.GetOlder (L"MD 30");
            (void) history.GetOlder (L"");
            Assert::AreEqual (std::wstring (L"T"),     history.GetNewer().value());
            Assert::AreEqual (std::wstring (L"MD 30"), history.GetNewer().value(), L"the line being typed comes back");
            Assert::IsFalse  (history.GetNewer().has_value());
        }



        TEST_METHOD (Add_MovesARepeatedLineToTheMostRecentPlace)
        {
            ConsoleHistory  history;



            history.Add (L"R");
            history.Add (L"BPL");
            history.Add (L"T");
            history.Add (L"R");
            history.Add (L"   ");

            Assert::AreEqual ((size_t) 3, history.GetLines().size(), L"no second copy, and no blank line");
            Assert::AreEqual (std::wstring (L"R"),   history.GetLines().back());
            Assert::AreEqual (std::wstring (L"BPL"), history.GetLines().front());
            Assert::AreEqual (std::wstring (L"R"),   history.GetOlder (L"").value());
        }



        TEST_METHOD (Add_EndsTheWalk_AndDropsTheOldestWhenFull)
        {
            ConsoleHistory  history;



            for (size_t i = 0; i <= ConsoleHistory::kLimit; ++i)
            {
                history.Add (std::to_wstring (i));
            }

            Assert::AreEqual (ConsoleHistory::kLimit, history.GetLines().size());
            Assert::AreEqual (std::wstring (L"1"), history.GetLines().front());

            (void) history.GetOlder (L"");
            (void) history.GetOlder (L"");
            history.Add (L"X");
            Assert::AreEqual (std::wstring (L"X"), history.GetOlder (L"").value(), L"a new walk starts at the most recent");
        }
    };
}
