#include "Pch.h"

#include "Ui/ColorUtil.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  ColorUtilTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ColorUtilTests)
{
public:

    TEST_METHOD (HsvToArgb_PrimariesAndGrays)
    {
        Assert::AreEqual (0xFFFF0000u, ColorUtil::HsvToArgb (0.0f,   1.0f, 1.0f));   // red
        Assert::AreEqual (0xFF00FF00u, ColorUtil::HsvToArgb (120.0f, 1.0f, 1.0f));   // green
        Assert::AreEqual (0xFF0000FFu, ColorUtil::HsvToArgb (240.0f, 1.0f, 1.0f));   // blue
        Assert::AreEqual (0xFFFFFFFFu, ColorUtil::HsvToArgb (0.0f,   0.0f, 1.0f));   // white
        Assert::AreEqual (0xFF000000u, ColorUtil::HsvToArgb (0.0f,   0.0f, 0.0f));   // black
    }

    TEST_METHOD (HsvToArgb_HueWrapsAndClamps)
    {
        // 360 wraps to 0 (red); negative hue wraps to the same.
        Assert::AreEqual (ColorUtil::HsvToArgb (0.0f, 1.0f, 1.0f),
                          ColorUtil::HsvToArgb (360.0f, 1.0f, 1.0f));
        Assert::AreEqual (ColorUtil::HsvToArgb (0.0f, 1.0f, 1.0f),
                          ColorUtil::HsvToArgb (-360.0f, 1.0f, 1.0f));

        // Out-of-range saturation / value clamp: s<=0 is white, v over 1 stays full.
        Assert::AreEqual (0xFFFFFFFFu, ColorUtil::HsvToArgb (0.0f, -1.0f, 5.0f));
    }

    TEST_METHOD (RoundTrip_ArgbHsvArgb)
    {
        const uint32_t  samples[] = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu,
                                      0xFFFFB000u, 0xFF123456u, 0xFFABCDEFu,
                                      0xFFFFFFFFu, 0xFF000000u };

        for (uint32_t argb : samples)
        {
            float     h   = 0.0f;
            float     s   = 0.0f;
            float     v   = 0.0f;
            uint32_t  rt  = 0;

            ColorUtil::ArgbToHsv (argb, h, s, v);
            rt = ColorUtil::HsvToArgb (h, s, v);

            // Allow +/- 1 per channel for float rounding on the round-trip.
            for (int shift = 0; shift <= 16; shift += 8)
            {
                int  a = (int) ((argb >> shift) & 0xFF);
                int  b = (int) ((rt   >> shift) & 0xFF);

                Assert::IsTrue (std::abs (a - b) <= 1);
            }
        }
    }

    TEST_METHOD (ParseHexColor_AcceptsValidForms)
    {
        uint32_t  argb = 0;

        Assert::IsTrue (ColorUtil::ParseHexColor (L"#FFB000", argb));
        Assert::AreEqual (0xFFFFB000u, argb);

        Assert::IsTrue (ColorUtil::ParseHexColor (L"00ff00", argb));
        Assert::AreEqual (0xFF00FF00u, argb);

        Assert::IsTrue (ColorUtil::ParseHexColor (L"  #123abc  ", argb));
        Assert::AreEqual (0xFF123ABCu, argb);
    }

    TEST_METHOD (ParseHexColor_RejectsMalformed)
    {
        uint32_t  argb = 0xDEADBEEF;

        Assert::IsFalse (ColorUtil::ParseHexColor (L"", argb));
        Assert::IsFalse (ColorUtil::ParseHexColor (L"#FFF", argb));        // too short
        Assert::IsFalse (ColorUtil::ParseHexColor (L"#FFGG00", argb));     // non-hex
        Assert::IsFalse (ColorUtil::ParseHexColor (L"FFB0000", argb));     // too long
        Assert::AreEqual (0xDEADBEEFu, argb);                             // untouched
    }

    TEST_METHOD (FormatHexColor_UppercaseSixDigits)
    {
        Assert::AreEqual (std::wstring (L"#FFB000"), ColorUtil::FormatHexColor (0xFFFFB000u));
        Assert::AreEqual (std::wstring (L"#000000"), ColorUtil::FormatHexColor (0xFF000000u));
        Assert::AreEqual (std::wstring (L"#ABCDEF"), ColorUtil::FormatHexColor (0x00ABCDEFu));
    }

    TEST_METHOD (ResolveColorMonitorTextArgb_MapsModes)
    {
        Assert::AreEqual (ColorUtil::kWhiteArgb,
                          ColorUtil::ResolveColorMonitorTextArgb (ColorMonitorTextMode::White, 0xFF123456u));
        Assert::AreEqual (ColorUtil::kGreenArgb,
                          ColorUtil::ResolveColorMonitorTextArgb (ColorMonitorTextMode::Green, 0xFF123456u));
        Assert::AreEqual (ColorUtil::kAmberArgb,
                          ColorUtil::ResolveColorMonitorTextArgb (ColorMonitorTextMode::Amber, 0xFF123456u));
        Assert::AreEqual (0xFF123456u,
                          ColorUtil::ResolveColorMonitorTextArgb (ColorMonitorTextMode::Custom, 0x00123456u));
    }
};
