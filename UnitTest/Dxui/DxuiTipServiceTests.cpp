#include "Pch.h"

#include "MockDxuiControl.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTipServiceTests
//
//  The window's tip service: which control's tip is found under a point, and
//  how each kind of tip opens and closes. The time is passed in, as in
//  DxuiTooltip's tests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTipServiceTests)
{
public:

    static constexpr int  kOpenMs = 100;

    static DxuiTip  MakeTip (const std::wstring & text, RECT anchor)
    {
        DxuiTip  tip;

        tip.text   = text;
        tip.anchor = anchor;
        return tip;
    }


    //  As the system's tips do, the tip opens once the pointer has rested
    //  over its control for the open delay: a move starts the delay again.
    TEST_METHOD (ATipOpensOnceThePointerRestsForItsDelay)
    {
        DxuiTooltip     tooltip;
        DxuiTipService  tips (tooltip);
        DxuiTip         tip = MakeTip (L"Run", RECT { 0, 0, 20, 20 });



        tooltip.SetDwellOpenMs (kOpenMs);

        tips.Show (tip, POINT { 5, 5 }, 0);
        tips.Show (tip, POINT { 6, 5 }, kOpenMs - 10);
        tooltip.Tick (kOpenMs);
        Assert::IsFalse (tooltip.IsVisible(), L"the tip opened while the pointer moved");

        tooltip.Tick (kOpenMs * 2 - 10);
        Assert::IsTrue   (tooltip.IsVisible(), L"no tip once the pointer rested");
        Assert::AreEqual (std::wstring (L"Run"), tooltip.GetText());
    }


    //  As the system's tips do, a press hides the tip at once, and the same
    //  tip does not open again while the pointer stays over its control.
    TEST_METHOD (APressHidesTheTipUntilThePointerIsOverAnotherOrNone)
    {
        DxuiTooltip     tooltip;
        DxuiTipService  tips (tooltip);
        DxuiTip         run  = MakeTip (L"Run",  RECT { 0,  0, 20, 20 });
        DxuiTip         step = MakeTip (L"Step", RECT { 20, 0, 40, 20 });



        tooltip.SetDwellOpenMs (kOpenMs);

        tips.Show (run, POINT { 5, 5 }, 0);
        tooltip.Tick (kOpenMs);
        Assert::IsTrue (tooltip.IsVisible());

        tips.OnPress();
        Assert::IsFalse (tooltip.IsVisible(), L"the press left the tip up");

        tips.Show (run, POINT { 6, 5 }, kOpenMs * 2);
        tooltip.Tick (kOpenMs * 4);
        Assert::IsFalse (tooltip.IsVisible(), L"the tip came back over the button pressed");

        tips.Show (step, POINT { 25, 5 }, kOpenMs * 5);
        tooltip.Tick (kOpenMs * 7);
        Assert::IsTrue   (tooltip.IsVisible(), L"no tip over the next button");
        Assert::AreEqual (std::wstring (L"Step"), tooltip.GetText());

        tips.OnPress();
        tips.Hide (kOpenMs * 8);
        tips.Show (step, POINT { 25, 5 }, kOpenMs * 9);
        tooltip.Tick (kOpenMs * 11);
        Assert::IsTrue (tooltip.IsVisible(), L"the pointer left the button and came back, and no tip");
    }


    //  While a menu is open there is no tip, and one waiting to open never
    //  does.
    TEST_METHOD (WhileSuppressedNoTipOpens)
    {
        DxuiTooltip     tooltip;
        DxuiTipService  tips (tooltip);
        DxuiTip         tip = MakeTip (L"Run", RECT { 0, 0, 20, 20 });



        tooltip.SetDwellOpenMs (kOpenMs);

        tips.Show (tip, POINT { 5, 5 }, 0);
        tips.SetSuppressed (true);
        Assert::IsFalse (tips.WantsTick(), L"the tip is on its way over the menu");

        tips.Show (tip, POINT { 5, 5 }, 10);
        Assert::IsFalse (tips.Tick (kOpenMs * 2));
        Assert::IsFalse (tooltip.IsVisible());

        tips.SetSuppressed (false);
        tips.Show (tip, POINT { 5, 5 }, kOpenMs * 3);
        Assert::IsTrue (tips.Tick (kOpenMs * 5));
        Assert::IsTrue (tooltip.IsVisible(), L"no tip once the menu closed");
    }


    //  A tip that follows the pointer is up at once, against the pointer's
    //  pixel rather than the part's anchor.
    TEST_METHOD (APointerTipIsUpAtOnceAtThePointer)
    {
        DxuiTooltip     tooltip;
        DxuiTipService  tips (tooltip);
        DxuiTip         tip = MakeTip (L"$0400", RECT { 0, 0, 8, 16 });



        tip.placement = DxuiTip::Placement::FollowPointer;
        tooltip.SetDwellOpenMs (kOpenMs);

        tips.Show (tip, POINT { 3, 7 }, 0);

        Assert::IsTrue   (tooltip.IsVisible(), L"it waited out a delay");
        Assert::AreEqual (3L, tooltip.GetAnchor().left);
        Assert::AreEqual (7L, tooltip.GetAnchor().top);
    }


    //  A timed notice is up at once and closes at its time: a pointer move
    //  that repeats it does not push the time back.
    TEST_METHOD (ATimedTipClosesAtItsTimeWhateverThePointerDoes)
    {
        constexpr int   kShowMs = 1000;
        DxuiTooltip     tooltip;
        DxuiTipService  tips (tooltip);
        DxuiTip         tip = MakeTip (L"Paddle mode", RECT { 0, 0, 20, 20 });



        tip.lifetime = DxuiTip::Lifetime::Timed;
        tip.timedMs  = kShowMs;

        tips.Show (tip, POINT { 5, 5 }, 0);
        Assert::IsTrue (tooltip.IsVisible(), L"it waited out a delay");

        tips.Show (tip, POINT { 6, 5 }, kShowMs - 10);
        tooltip.Tick (kShowMs + 1);
        Assert::IsFalse (tooltip.IsVisible(), L"the move pushed the closing time back");
    }


    //  After a pending tip the tip that is up stays up, rather than closing
    //  while the next tip's text is read.
    TEST_METHOD (APendingTipLeavesTheTipUp)
    {
        DxuiTooltip     tooltip;
        DxuiTipService  tips (tooltip);
        DxuiTip         tip     = MakeTip (L"Run", RECT { 0, 0, 20, 20 });
        DxuiTip         pending;



        pending.isPending = true;
        tooltip.SetDwellOpenMs (kOpenMs);

        tips.Show (tip, POINT { 5, 5 }, 0);
        tooltip.Tick (kOpenMs);

        tips.Show (pending, POINT { 50, 5 }, kOpenMs * 2);
        tooltip.Tick (kOpenMs * 3);

        Assert::IsTrue   (tooltip.IsVisible(), L"the pending tip closed the one up");
        Assert::AreEqual (std::wstring (L"Run"), tooltip.GetText());
    }


    //  A container's tip is the tip of its front child under the point, or
    //  else the container's tip text.
    //  A child that takes local points gets the point relative to itself, and
    //  its anchor comes back in the container's pixels.
    TEST_METHOD (TheFrontChildsTipWinsAndALocalChildsAnchorIsMovedBack)
    {
        DxuiPanel          root;
        MockDxuiControl  * back  = root.CreateChild<MockDxuiControl>();
        MockDxuiControl  * front = root.CreateChild<MockDxuiControl>();
        DxuiTip            tip;



        root.SetBounds   (RECT { 0,  0,  200, 100 });
        back->SetBounds  (RECT { 10, 10, 110, 60 });
        front->SetBounds (RECT { 50, 20, 90,  40 });

        root.SetTip   (L"Window");
        back->SetTip  (L"Back");
        front->SetTip (L"Front");
        front->usesLocalPoints = true;

        Assert::IsTrue   (DxuiTipService::FindTip (root, POINT { 60, 30 }, tip));
        Assert::AreEqual (std::wstring (L"Front"), tip.text, L"the control under the front one");
        Assert::AreEqual (50L, tip.anchor.left, L"the local anchor was not moved back");
        Assert::AreEqual (20L, tip.anchor.top);

        Assert::IsTrue   (DxuiTipService::FindTip (root, POINT { 20, 50 }, tip));
        Assert::AreEqual (std::wstring (L"Back"), tip.text);

        Assert::IsTrue   (DxuiTipService::FindTip (root, POINT { 150, 80 }, tip));
        Assert::AreEqual (std::wstring (L"Window"), tip.text, L"the container's tip");

        front->SetVisible (false);
        Assert::IsTrue   (DxuiTipService::FindTip (root, POINT { 60, 30 }, tip));
        Assert::AreEqual (std::wstring (L"Back"), tip.text, L"a hidden control's tip");
    }


    //  Every part of a tip's look is set for each tip, so a tip of a sentence
    //  after a tip of code is in the body face.
    TEST_METHOD (ATipHasNoneOfTheLastTipsLook)
    {
        DxuiTooltip     tooltip;
        DxuiTipService  tips (tooltip);
        DxuiTip         code  = MakeTip (L"LDA $C000", RECT { 0, 0, 20, 20 });
        DxuiTip         plain = MakeTip (L"Run",       RECT { 20, 0, 40, 20 });



        code.isMonospace = true;

        tips.Show (code, POINT { 5, 5 }, 0);
        Assert::AreEqual (std::wstring (DxuiTheme::kMonoFace), std::wstring (tooltip.GetFace()));

        tips.Show (plain, POINT { 25, 5 }, 10);
        Assert::AreEqual (std::wstring (DxuiTheme::kBodyFace), std::wstring (tooltip.GetFace()), L"the face of the last tip");
    }
};
