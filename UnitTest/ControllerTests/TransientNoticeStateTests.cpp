#include "Pch.h"

#include "Controllers/TransientNoticeState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TransientNoticeStateTests
//
//  The clock is passed in, so expiry is asserted at an exact millisecond
//  rather than by sleeping and hoping.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (TransientNoticeStateTests)
    {
    public:

        TEST_METHOD (Shown_IsVisibleUntilItExpires)
        {
            TransientNoticeState  notice;

            notice.Show (L"Saved capture.png", 1000, 4000);

            Assert::IsTrue  (notice.IsShowing (1000), L"showing from the moment it is raised");
            Assert::IsTrue  (notice.IsShowing (4999), L"still showing one millisecond before it is due to go");
            Assert::IsFalse (notice.IsShowing (5000), L"and gone exactly when it is due, not a frame later");
        }


        TEST_METHOD (SecondNotice_ReplacesTheFirst)
        {
            TransientNoticeState  notice;

            notice.Show (L"Saved capture.png", 1000, 4000);
            notice.Show (L"Controller selected: Xbox Controller", 2000, 4000);

            Assert::AreEqual (std::wstring (L"Controller selected: Xbox Controller"), notice.GetText(),
                L"the newest notice is the one the user is looking for");
            Assert::IsTrue (notice.IsShowing (5500), L"and it carries its own full duration, not the first one's remainder");
        }


        TEST_METHOD (Cleared_StopsShowing)
        {
            TransientNoticeState  notice;

            notice.Show (L"Saved capture.png", 1000, 4000);
            notice.Clear();

            Assert::IsFalse (notice.IsShowing (1001), L"a cleared notice is gone at once");
            Assert::IsTrue  (notice.GetText().empty());
        }


        TEST_METHOD (EmptyText_NeverShows)
        {
            TransientNoticeState  notice;

            notice.Show (L"", 1000, 4000);

            Assert::IsFalse (notice.IsShowing (1001), L"nothing to say is not a notice with nothing in it");
        }


        TEST_METHOD (Default_IsFourSeconds)
        {
            TransientNoticeState  notice;

            notice.Show (L"Saved capture.png", 0);

            Assert::IsTrue  (notice.IsShowing (3999), L"the screenshot notice keeps the duration it had");
            Assert::IsFalse (notice.IsShowing (4000));
        }
    };
}
