#include "Pch.h"

#include "Theme/DxuiColor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiColorTests
//
//  Packed-ARGB blending. Every channel, alpha included, moves independently,
//  so a test color puts a different value in each one: a blend that mixed up
//  two shifts would still pass on a gray.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiColorTests)
{
public:

    static constexpr uint32_t  s_kFrom = 0x10204080u;
    static constexpr uint32_t  s_kTo   = 0x90A0C0F0u;


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
};





