#include "Pch.h"

#include "Theme/DxuiColor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiColorTests
//
//  The focus accent: the theme's accent where it stands apart from the
//  background, and its complement where the background shares its hue.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiColorTests)
{
public:

    static int  Red   (uint32_t argb) { return (int) ((argb >> 16) & 0xFFu); }
    static int  Green (uint32_t argb) { return (int) ((argb >>  8) & 0xFFu); }
    static int  Blue  (uint32_t argb) { return (int) ( argb        & 0xFFu); }


    TEST_METHOD (ABlueAccentOnABlueBackgroundTurnsOrange)
    {
        uint32_t  focus = DxuiColor::ComputeFocusAccent (0xFF4EA8FFu, 0xFF1B2433u);

        Assert::IsTrue (Red (focus) > Green (focus) && Green (focus) > Blue (focus), L"a warm color, red over green over blue");
        Assert::AreEqual (0xFF000000u, focus & 0xFF000000u, L"opaque as the accent is");
    }


    TEST_METHOD (AnAccentOnAGrayBackgroundIsKept)
    {
        Assert::AreEqual (0xFF4EA8FFu, DxuiColor::ComputeFocusAccent (0xFF4EA8FFu, 0xFF1F1F1Fu));
    }


    TEST_METHOD (AnAccentOnABackgroundOfAnotherHueIsKept)
    {
        Assert::AreEqual (0xFF4EA8FFu, DxuiColor::ComputeFocusAccent (0xFF4EA8FFu, 0xFF33241Bu), L"a brown background");
    }
};
