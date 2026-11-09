#include "Pch.h"

#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/DiskInspector/InspectorWalkthrough.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthroughTests
//
//  The walkthrough (FR-153): a step for each area of the window, then the
//  disk facts; Back and Next, and closing at the end; its card beside the
//  step's area; and its buttons found where they are drawn.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorWalkthroughTests)
{
public:

    TEST_METHOD (TheAreasComeFirstThenTheDiskFacts)
    {
        std::span<const InspectorWalkthrough::Step>  steps = InspectorWalkthrough::GetSteps();
        const WalkthroughArea                        areas[] = { WalkthroughArea::Platter, WalkthroughArea::Toolbar, WalkthroughArea::Track,
                                                                 WalkthroughArea::Tabs, WalkthroughArea::Strip };
        size_t                                       i       = 0;



        Assert::IsTrue (steps.size() > std::size (areas), L"disk facts follow the areas");

        for (i = 0; i < steps.size(); i++)
        {
            Assert::IsTrue (steps[i].area == (i < std::size (areas) ? areas[i] : WalkthroughArea::None));
            Assert::IsTrue (wcslen (steps[i].title) > 0 && wcslen (steps[i].body) > 0);
        }
    }



    TEST_METHOD (BackAndNextStepThroughAndTheLastNextCloses)
    {
        InspectorWalkthrough  walkthrough;
        int                   count       = static_cast<int> (InspectorWalkthrough::GetSteps().size());
        int                   i           = 0;



        Assert::IsFalse (walkthrough.IsOpen());

        walkthrough.Open();
        walkthrough.Back();
        Assert::AreEqual (0, walkthrough.GetStep(), L"Back on the first step stays there");

        for (i = 1; i < count; i++)
        {
            walkthrough.Next();
            Assert::AreEqual (i, walkthrough.GetStep());
        }

        walkthrough.Back();
        Assert::AreEqual (count - 2, walkthrough.GetStep());

        walkthrough.Next();
        walkthrough.Next();
        Assert::IsFalse (walkthrough.IsOpen(), L"Next on the last step closes it");

        walkthrough.Open();
        Assert::AreEqual (0, walkthrough.GetStep(), L"it opens again on the first step");
    }



    TEST_METHOD (TheCardGoesBesideTheAreaWhereItFits)
    {
        static constexpr RECT  kWindow = { 0, 0, 1600, 1000 };
        static constexpr int   kW      = 400;
        static constexpr int   kH      = 300;
        static constexpr int   kGap    = 10;

        RECT  card = {};



        card = InspectorWalkthrough::PlaceCard (kWindow, { 0, 100, 800, 900 }, kW, kH, kGap);
        Assert::AreEqual (810L, card.left, L"right of an area on the left");

        card = InspectorWalkthrough::PlaceCard (kWindow, { 900, 100, 1600, 900 }, kW, kH, kGap);
        Assert::AreEqual (890L, card.right, L"left of an area on the right");

        card = InspectorWalkthrough::PlaceCard (kWindow, { 0, 0, 1600, 100 }, kW, kH, kGap);
        Assert::AreEqual (110L, card.top, L"below an area as wide as the window");

        card = InspectorWalkthrough::PlaceCard (kWindow, {}, kW, kH, kGap);
        Assert::AreEqual (600L, card.left, L"centered with no area");
        Assert::AreEqual (350L, card.top);
    }



    TEST_METHOD (TheButtonsAreFoundWhereTheyAreDrawn)
    {
        InspectorWalkthrough  walkthrough;
        MockDxuiTextRenderer  text;
        DxuiTheme             theme  = DxuiTheme::Dark();
        DxuiDpiScaler         scaler;
        POINT                 inNext = {};



        walkthrough.Open();
        walkthrough.Paint (text, theme, scaler, { 0, 0, 1600, 1000 }, { 0, 100, 800, 900 });

        Assert::IsTrue (walkthrough.HitTest ({ 5, 5 }) == WalkthroughButton::None);

        //  The card is right of the area at 96 dpi, and Next ends one padding
        //  in from the card's right edge, along its bottom.
        inNext.x = static_cast<LONG> (800 + 12 + InspectorWalkthrough::kCardWidthDip - 16 - 4);

        for (inNext.y = 0; inNext.y < 1000 && walkthrough.HitTest (inNext) != WalkthroughButton::Next; inNext.y++)
        {
            Assert::IsTrue (walkthrough.HitTest (inNext) == WalkthroughButton::None);
        }

        Assert::IsTrue (walkthrough.HitTest (inNext) == WalkthroughButton::Next, L"Next is on the card");
    }
};
