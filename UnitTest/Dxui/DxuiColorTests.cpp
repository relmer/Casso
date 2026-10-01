#include "Pch.h"

#include "Theme/DxuiColor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiColorTests
//
//  The focus accent: the theme's accent where it stands apart from the
//  background, and magenta where the background shares its hue.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiColorTests)
{
public:

    static int  Red   (uint32_t argb) { return (int) ((argb >> 16) & 0xFFu); }
    static int  Green (uint32_t argb) { return (int) ((argb >>  8) & 0xFFu); }
    static int  Blue  (uint32_t argb) { return (int) ( argb        & 0xFFu); }


    TEST_METHOD (ABlueAccentOnABlueBackgroundTurnsMagenta)
    {
        uint32_t  focus = DxuiColor::ComputeFocusAccent (0xFF4EA8FFu, 0xFF1B2433u);

        Assert::IsTrue (Red (focus) >= 0xF0 && Blue (focus) >= 0xF0, L"magenta, not orange");
        Assert::IsTrue (Green (focus) <= 0x40, L"saturated, not pastel");
        Assert::AreEqual (0xFF000000u, focus & 0xFF000000u, L"opaque as the accent is");
    }


    //  Magenta would not stand out on a magenta background, so the accent's
    //  complement is used there instead.
    TEST_METHOD (AMagentaAccentOnAMagentaBackgroundTurnsToItsComplement)
    {
        uint32_t  focus = DxuiColor::ComputeFocusAccent (0xFFFF4EFFu, 0xFF331B33u);

        Assert::IsTrue (Green (focus) >= 0xF0, L"green");
        Assert::IsTrue (Red (focus) <= 0x40 && Blue (focus) <= 0x40, L"saturated, not pastel");
    }


    //  Retro's pale phosphor green on its green panels: the complement is a
    //  vivid magenta, not the pastel pink a pale accent's complement would be.
    TEST_METHOD (APaleAccentsComplementIsVivid)
    {
        uint32_t  focus = DxuiColor::ComputeFocusAccent (0xFF8AFF8Au, 0xFF0E2612u);

        Assert::IsTrue (Red (focus) >= 0xF0 && Blue (focus) >= 0xF0, L"magenta");
        Assert::IsTrue (Green (focus) <= 0x40, L"saturated, not pastel");
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
