#include "Pch.h"

#include "Ui/Debugger/OpeningFocusDeferral.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  OpeningFocusDeferralTests
//
//  A saved focused pane that opens only after the first snapshot takes the
//  focus when it shows, once, unless the user clicked or typed first or the
//  reopened views settled without it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (OpeningFocusDeferralTests)
    {
    public:
        using Action = OpeningFocusDeferral::Action;



        TEST_METHOD (NothingDeferred_NeverPlaces)
        {
            OpeningFocusDeferral  deferral;



            Assert::IsTrue (deferral.OnSnapshot (true, true) == Action::None);
        }



        TEST_METHOD (PaneShowsLater_PlacesOnce)
        {
            OpeningFocusDeferral  deferral;



            deferral.Defer();

            Assert::IsTrue  (deferral.OnSnapshot (false, true) == Action::None,  L"not ready yet: wait");
            Assert::IsTrue  (deferral.IsPending(),                               L"still waiting");
            Assert::IsTrue  (deferral.OnSnapshot (true,  true) == Action::Place, L"ready: place");
            Assert::IsTrue  (deferral.OnSnapshot (true,  true) == Action::None,  L"placed only once");
            Assert::IsFalse (deferral.IsPending());
        }



        TEST_METHOD (UserClickedFirst_DoesNotPlace)
        {
            OpeningFocusDeferral  deferral;



            deferral.Defer();
            deferral.OnUserInput();

            Assert::IsTrue (deferral.OnSnapshot (true, true) == Action::None);
        }



        TEST_METHOD (UserInputBeforeDefer_StillCounts)
        {
            OpeningFocusDeferral  deferral;



            deferral.OnUserInput();
            deferral.Defer();

            Assert::IsFalse (deferral.IsPending());
            Assert::IsTrue  (deferral.OnSnapshot (true, true) == Action::None);
        }



        TEST_METHOD (ViewsSettledWithoutPane_GivesUp)
        {
            OpeningFocusDeferral  deferral;



            deferral.Defer();

            Assert::IsTrue  (deferral.OnSnapshot (false, false) == Action::None);
            Assert::IsFalse (deferral.IsPending(),                              L"settled without it: dropped");
            Assert::IsTrue  (deferral.OnSnapshot (true,  false) == Action::None, L"a pane opened later by hand does not take the focus");
        }



        TEST_METHOD (ReadyOnTheSettlingSnapshot_StillPlaces)
        {
            OpeningFocusDeferral  deferral;



            deferral.Defer();

            Assert::IsTrue (deferral.OnSnapshot (true, false) == Action::Place);
        }
    };
}
