#include "Pch.h"

#include "Widgets/DxuiActionBanner.h"
#include "Core/DxuiDpiScaler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBannerTests
//
//  The notice strip that carries actions: where its buttons land, and that
//  pressing one reports which.
//
//  DxuiInfoBanner CANNOT DO THIS, and its own header says so: "not clickable,
//  no raised surface". These cover what composing it adds -- the reserved
//  action column, the geometry beside wrapped text, and the report back.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiActionBannerTests
{
    static DxuiDpiScaler Scaler96()
    {
        DxuiDpiScaler  s;

        s.SetDpi (96);   // 1:1 DIP == px
        return s;
    }


    static RECT MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  rect = { l, t, r, b };

        return rect;
    }


    TEST_CLASS (DxuiActionBannerTests)
    {
    public:

        TEST_METHOD (ABannerWithNoActionsHasNone)
        {
            DxuiActionBanner  banner;



            banner.SetText (L"A disk changed.");

            Assert::AreEqual ((size_t) 0, banner.GetActionCount());
            Assert::IsNull (banner.GetAction (0));
        }



        TEST_METHOD (SettingActionsReplacesThemRatherThanAddingToThem)
        {
            DxuiActionBanner  banner;



            banner.SetActions ({ L"Restart" });
            Assert::AreEqual ((size_t) 1, banner.GetActionCount());

            //  A standing report absorbs later changes instead of stacking, so
            //  re-wording it must leave one Restart button and not three.
            banner.SetActions ({ L"Restart" });
            banner.SetActions ({ L"Restart" });

            Assert::AreEqual ((size_t) 1, banner.GetActionCount());
        }



        TEST_METHOD (TheActionReportsWhichOneWasInvoked)
        {
            DxuiActionBanner  banner;
            int               invoked = -1;



            banner.SetActions ({ L"Take it up", L"Restart" });
            banner.SetOnAction ([&invoked] (size_t index) { invoked = (int) index; });

            banner.GetAction (1)->Click();

            Assert::AreEqual (1, invoked, L"the index identifies the answer, and the "
                                          L"answer's meaning came from core with its label");

            banner.GetAction (0)->Click();
            Assert::AreEqual (0, invoked);
        }



        TEST_METHOD (TheActionsLandInsideTheBannerAgainstItsTrailingEdge)
        {
            DxuiActionBanner  banner;
            DxuiDpiScaler     scaler = Scaler96();
            RECT              bounds = MakeRect (0, 0, 600, 60);
            RECT              first  = {};
            RECT              second = {};



            banner.SetText    (L"Loader.dsk changed and the new contents were reloaded.");
            banner.SetActions ({ L"Take it up", L"Restart" });
            banner.Layout     (bounds, scaler);

            first  = banner.GetAction (0)->GetBounds();
            second = banner.GetAction (1)->GetBounds();

            Assert::IsTrue (second.right <= bounds.right, L"inside the strip");
            Assert::IsTrue (first.right  <= second.left,  L"laid out in the order given");
            Assert::IsTrue (first.left   >  bounds.left,  L"against the trailing edge, "
                                                          L"not over the message");
            Assert::IsTrue (first.top    >= bounds.top);
            Assert::IsTrue (first.bottom <= bounds.bottom);
        }



        TEST_METHOD (AnActionIsNeverClippedByAShortMessage)
        {
            DxuiActionBanner  banner;
            DxuiDpiScaler     scaler = Scaler96();
            float             height = 0.0f;



            //  One short line of text would fit in less height than a button
            //  needs, and a report whose action is clipped offers nothing.
            banner.SetText    (L"Ok.");
            banner.SetActions ({ L"Restart" });

            height = banner.GetPreferredHeightPx (600.0f, scaler);

            Assert::IsTrue (height >= 26.0f, L"at least as tall as its action");
        }



        TEST_METHOD (TheActionColumnComesOutOfTheWidthBeforeTheTextWraps)
        {
            DxuiActionBanner  bare;
            DxuiActionBanner  withAction;
            DxuiDpiScaler     scaler = Scaler96();
            const wchar_t *   text   =
                L"Loader.dsk was changed by something else while it was mounted, and the "
                L"new contents have been reloaded. The machine is still running.";



            bare.SetText       (text);
            withAction.SetText (text);
            withAction.SetActions ({ L"Restart" });

            //  Measuring the text across the full width and then putting a
            //  button over it is how a notice ends up with its message
            //  underneath its own action.
            Assert::IsTrue (withAction.GetPreferredHeightPx (400.0f, scaler)
                          > bare.GetPreferredHeightPx (400.0f, scaler),
                            L"less room for the text means more lines of it");
        }



        TEST_METHOD (ABannerReportsItsTextAsItsAccessibleName)
        {
            DxuiActionBanner  banner;



            banner.SetText (L"Loader.dsk changed.");

            Assert::IsTrue (banner.GetAccessibleName() == std::wstring (L"Loader.dsk changed."));
            Assert::IsTrue (banner.GetAccessibleRole() == DxuiAccessibleRole::Label);
        }



        TEST_METHOD (TheNoticeIsLaidOutClearOfItsOwnActions)
        {
            DxuiActionBanner  banner;
            DxuiDpiScaler     scaler = Scaler96();
            RECT              bounds = MakeRect (0, 0, 600, 60);
            RECT              action = {};
            RECT              notice = {};



            banner.SetText    (L"Loader.dsk in Drive 1 was modified externally and mounted.");
            banner.SetActions ({ L"Dismiss" });
            banner.Layout     (bounds, scaler);

            action = banner.GetAction (0)->GetBounds();
            notice = banner.GetNoticeBounds();

            //  IT IS A MESSAGE BAR CONTAINING A BUTTON. The bordered strip runs
            //  the full width and the action sits inside it.
            Assert::IsTrue (notice.right >= action.right,
                            L"the action must be inside the bar, not beside it");
            Assert::IsTrue (notice.left  <= action.left);
            Assert::IsTrue (notice.top   <= action.top);
            Assert::IsTrue (notice.bottom >= action.bottom);

            //  And what keeps the text off it is the reserve, not a narrower
            //  box: measured, handing the notice the whole width with no
            //  reserve wrapped the last line underneath the button.
            Assert::IsTrue (banner.GetActionReservePx (scaler) > 0.0f);
        }
    };
}
