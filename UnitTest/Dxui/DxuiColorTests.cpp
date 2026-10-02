#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiColorTests
//
//  Packed-ARGB blending, and the focus accent. Every channel of a blend,
//  alpha included, moves independently, so a test color puts a different
//  value in each one: a blend that mixed up two shifts would still pass on a
//  gray. The focus accent is the theme's accent where it stands apart from
//  the background, and lavender where the background shares its hue.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiColorTests)
{
public:

    static constexpr uint32_t  s_kFrom = 0x10204080u;
    static constexpr uint32_t  s_kTo   = 0x90A0C0F0u;


    static int  Red   (uint32_t argb) { return (int) ((argb >> 16) & 0xFFu); }
    static int  Green (uint32_t argb) { return (int) ((argb >>  8) & 0xFFu); }
    static int  Blue  (uint32_t argb) { return (int) ( argb        & 0xFFu); }


    TEST_METHOD (Lerp_Endpoints_ReturnTheInputs)
    {
        Assert::AreEqual (s_kFrom, DxuiColor::Lerp (s_kFrom, s_kTo, 0.0f));
        Assert::AreEqual (s_kTo,   DxuiColor::Lerp (s_kFrom, s_kTo, 1.0f));
    }


    TEST_METHOD (Lerp_Midpoint_BlendsEachChannel)
    {
        Assert::AreEqual (0x506080B8u, DxuiColor::Lerp (s_kFrom, s_kTo, 0.5f));
    }


    TEST_METHOD (Lerp_RoundsToNearest)
    {
        // Halfway from 0 to 3 is 1.5, which rounds up to 2.
        Assert::AreEqual (0x00000002u, DxuiColor::Lerp (0x00000000u, 0x00000003u, 0.5f));
    }


    TEST_METHOD (Lerp_OutOfRange_ClampsEachChannel)
    {
        Assert::AreEqual (0xFFFFFFFFu, DxuiColor::Lerp (0x80808080u, 0xFFFFFFFFu, 2.0f));
        Assert::AreEqual (0x00000000u, DxuiColor::Lerp (0x80808080u, 0xFFFFFFFFu, -2.0f));
    }


    //  Visual Studio's dark theme focus outline, sampled from a screenshot.
    TEST_METHOD (ABlueAccentOnABlueBackgroundTurnsLavender)
    {
        Assert::AreEqual (0xFF9184EEu, DxuiColor::ComputeFocusAccent (0xFF4EA8FFu, 0xFF1B2433u));
    }


    //  Magenta would not stand out on a magenta background, so the accent's
    //  complement is used there instead.
    TEST_METHOD (AMagentaAccentOnAMagentaBackgroundTurnsToItsComplement)
    {
        uint32_t  focus = DxuiColor::ComputeFocusAccent (0xFFFF4EFFu, 0xFF331B33u);

        Assert::IsTrue (Green (focus) >= 0xF0, L"green");
        Assert::IsTrue (Red (focus) <= 0x40 && Blue (focus) <= 0x40, L"saturated, not pastel");
    }


    //  Retro's pale phosphor green on its green panels.
    TEST_METHOD (APaleGreenAccentOnAGreenBackgroundTurnsLavender)
    {
        Assert::AreEqual (0xFF9184EEu, DxuiColor::ComputeFocusAccent (0xFF8AFF8Au, 0xFF0E2612u));
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