#include "Pch.h"

#include "Widgets/DxuiTooltip.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipReadTimeTests
//
//  A tip stays up long enough to read: the system's tip lifetime, longer
//  for a long text.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTooltipReadTimeTests)
{
public:

    TEST_METHOD (ALongTipStaysUpLongEnoughToRead)
    {
        DxuiTooltip   tip;
        std::wstring  text (300, L'x');
        int64_t       shownMs = 0;



        tip.RequestShow (RECT { 0, 0, 10, 10 }, text, 0);
        tip.Tick        (1000);
        shownMs = 1000;

        Assert::IsTrue (tip.IsVisible(), L"the tip shows after its dwell");

        tip.Tick (shownMs + DxuiTooltip::kMaxVisibleMs + 1000);
        Assert::IsTrue (tip.IsVisible(), L"a long tip outlasts the short-tip lifetime");

        tip.Tick (shownMs + DxuiTooltip::kMaxReadMs + 1);
        Assert::IsFalse (tip.IsVisible(), L"but not forever");
    }



    TEST_METHOD (AShortTipKeepsTheSystemLifetime)
    {
        Assert::AreEqual (5000,  DxuiTooltip::ComputeVisibleMs (10, 5000));
        Assert::AreEqual (12000, DxuiTooltip::ComputeVisibleMs (200, 5000));
        Assert::AreEqual (DxuiTooltip::kMaxReadMs, DxuiTooltip::ComputeVisibleMs (100000, 5000));
    }
};