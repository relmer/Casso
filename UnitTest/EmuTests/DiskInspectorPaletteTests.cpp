#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPaletteTests
//
//  The numeric part of SC-008, in every theme either host offers and in both
//  fallbacks: text at 4.5:1 against what it is drawn on, every pair of
//  platter kinds and every pair of map roles at Delta E 2000 10 or more, and good
//  and bad sectors apart in grayscale as well as by symbol (FR-077, FR-079).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskInspectorPaletteTests)
{
public:

    static constexpr float   kTextContrast    = 4.5f;
    static constexpr double  kMinDeltaE       = 10.0;
    static constexpr float   kGrayscaleApart  = 1.3f;



    struct NamedPalette
    {
        const wchar_t *       name;
        DiskInspectorPalette  palette;
    };



    static vector<NamedPalette> MakeAll()
    {
        return {
            { L"Skeuomorphic",   DiskInspectorPalette::Resolve (CassoTheme::MakeSkeuomorphic())  },
            { L"Dark modern",    DiskInspectorPalette::Resolve (CassoTheme::MakeDarkModern())    },
            { L"Retro terminal", DiskInspectorPalette::Resolve (CassoTheme::MakeRetroTerminal()) },
            { L"Dxui dark",      DiskInspectorPalette::Resolve (DxuiTheme::Dark())               },
            { L"Dxui light",     DiskInspectorPalette::Resolve (DxuiTheme::Light())              },
            { L"Fallback dark",  DiskInspectorPalette::MakeFallback (true)                       },
            { L"Fallback light", DiskInspectorPalette::MakeFallback (false)                      },
        };
    }



    static std::array<double, 3> ToLab (uint32_t argb)
    {
        static constexpr double  kWhiteX = 0.95047;
        static constexpr double  kWhiteZ = 1.08883;

        std::array<double, 3>  rgb = {};
        double                 x   = 0;
        double                 y   = 0;
        double                 z   = 0;
        int                    i   = 0;



        for (i = 0; i < 3; i++)
        {
            double  v = ((argb >> (16 - 8 * i)) & 0xFF) / 255.0;

            rgb[i] = v <= 0.04045 ? v / 12.92 : std::pow ((v + 0.055) / 1.055, 2.4);
        }

        x = (0.4124564 * rgb[0] + 0.3575761 * rgb[1] + 0.1804375 * rgb[2]) / kWhiteX;
        y = (0.2126729 * rgb[0] + 0.7151522 * rgb[1] + 0.0721750 * rgb[2]);
        z = (0.0193339 * rgb[0] + 0.1191920 * rgb[1] + 0.9503041 * rgb[2]) / kWhiteZ;

        auto  f = [] (double t) { return t > 0.008856 ? std::cbrt (t) : 7.787 * t + 16.0 / 116.0; };

        return { 116.0 * f (y) - 16.0, 500.0 * (f (x) - f (y)), 200.0 * (f (y) - f (z)) };
    }



    //  CIEDE2000, from Sharma, Wu and Dalal's formulation.
    static double DeltaE2000 (uint32_t first, uint32_t second)
    {
        static constexpr double  kPi       = 3.14159265358979323846;
        static constexpr double  kDegrees  = 180.0 / kPi;
        static constexpr double  k25Pow7   = 6103515625.0;

        auto    lab1 = ToLab (first);
        auto    lab2 = ToLab (second);
        double  c1   = std::hypot (lab1[1], lab1[2]);
        double  c2   = std::hypot (lab2[1], lab2[2]);
        double  cBar = (c1 + c2) / 2;
        double  g    = 0.5 * (1 - std::sqrt (std::pow (cBar, 7) / (std::pow (cBar, 7) + k25Pow7)));
        double  a1   = (1 + g) * lab1[1];
        double  a2   = (1 + g) * lab2[1];
        double  c1p  = std::hypot (a1, lab1[2]);
        double  c2p  = std::hypot (a2, lab2[2]);
        double  h1p  = c1p == 0 ? 0 : std::fmod (std::atan2 (lab1[2], a1) * kDegrees + 360, 360);
        double  h2p  = c2p == 0 ? 0 : std::fmod (std::atan2 (lab2[2], a2) * kDegrees + 360, 360);
        double  dh   = 0;
        double  hBar = h1p + h2p;
        double  lBar = (lab1[0] + lab2[0]) / 2;
        double  cBp  = (c1p + c2p) / 2;



        if (c1p * c2p != 0)
        {
            dh = h2p - h1p;
            dh = dh > 180 ? dh - 360 : (dh < -180 ? dh + 360 : dh);

            if (std::abs (h1p - h2p) > 180)
            {
                hBar += hBar < 360 ? 360 : -360;
            }

            hBar /= 2;
        }

        double  dL  = lab2[0] - lab1[0];
        double  dC  = c2p - c1p;
        double  dH  = 2 * std::sqrt (c1p * c2p) * std::sin (dh / kDegrees / 2);
        double  t   = 1 - 0.17 * std::cos ((hBar - 30) / kDegrees) + 0.24 * std::cos (2 * hBar / kDegrees)
                        + 0.32 * std::cos ((3 * hBar + 6) / kDegrees) - 0.20 * std::cos ((4 * hBar - 63) / kDegrees);
        double  dTh = 30 * std::exp (-std::pow ((hBar - 275) / 25, 2));
        double  rC  = 2 * std::sqrt (std::pow (cBp, 7) / (std::pow (cBp, 7) + k25Pow7));
        double  sL  = 1 + 0.015 * std::pow (lBar - 50, 2) / std::sqrt (20 + std::pow (lBar - 50, 2));
        double  sC  = 1 + 0.045 * cBp;
        double  sH  = 1 + 0.015 * cBp * t;
        double  rT  = -std::sin (2 * dTh / kDegrees) * rC;

        return std::sqrt (std::pow (dL / sL, 2) + std::pow (dC / sC, 2) + std::pow (dH / sH, 2) + rT * (dC / sC) * (dH / sH));
    }



    template <size_t N>
    static void AssertAllApart (const wchar_t * theme, const wchar_t * what, const std::array<uint32_t, N> & colors)
    {
        size_t  i = 0;
        size_t  j = 0;



        for (i = 0; i < N; i++)
        {
            for (j = i + 1; j < N; j++)
            {
                double  de = DeltaE2000 (colors[i], colors[j]);

                Assert::IsTrue (de >= kMinDeltaE, std::format (L"{}: {} {} and {} are Delta E 2000 {:.1f} apart", theme, what, i, j, de).c_str());
            }
        }
    }



    TEST_METHOD (TextReachesFourAndAHalfToOne)
    {
        for (const NamedPalette & p : MakeAll())
        {
            Assert::IsTrue (DxuiColor::ComputeContrastRatio (p.palette.text, p.palette.background) >= kTextContrast, p.name);

            for (SectorState state : { SectorState::Good, SectorState::BadData, SectorState::NotChecked, SectorState::NoDataField })
            {
                Assert::IsTrue (DxuiColor::ComputeContrastRatio (p.palette.GetStateColor (state), p.palette.background) >= kTextContrast,
                                std::format (L"{}: sector state {}", p.name, static_cast<int> (state)).c_str());
            }

            for (uint32_t color : p.palette.kinds)
            {
                Assert::IsTrue (DxuiColor::ComputeContrastRatio (DiskInspectorPalette::GetTextColorOn (color), color) >= kTextContrast, p.name);
            }

            for (uint32_t color : p.palette.roles)
            {
                Assert::IsTrue (DxuiColor::ComputeContrastRatio (DiskInspectorPalette::GetTextColorOn (color), color) >= kTextContrast, p.name);
            }
        }
    }



    TEST_METHOD (KindsAndRolesAreApartByDeltaE2000)
    {
        for (const NamedPalette & p : MakeAll())
        {
            AssertAllApart (p.name, L"platter kinds", p.palette.kinds);
            AssertAllApart (p.name, L"map roles",     p.palette.roles);
        }
    }



    TEST_METHOD (GoodAndBadAreApartInGrayscaleAndBySymbol)
    {
        for (const NamedPalette & p : MakeAll())
        {
            Assert::IsTrue (DxuiColor::ComputeContrastRatio (p.palette.GetStateColor (SectorState::Good), p.palette.GetStateColor (SectorState::BadData)) >= kGrayscaleApart, p.name);
        }

        Assert::AreNotEqual (std::wstring (DiskInspectorPalette::GetStateSymbol (SectorState::Good)), std::wstring (DiskInspectorPalette::GetStateSymbol (SectorState::BadData)));
        Assert::AreNotEqual (std::wstring (DiskInspectorPalette::GetMarkSymbol (true)), std::wstring (DiskInspectorPalette::GetMarkSymbol (false)));
    }



    TEST_METHOD (AThemeThatSetsNoColorsTakesTheFallbackForItsSurface)
    {
        DxuiTheme             light = DxuiTheme::Light();
        DiskInspectorPalette  palette;



        light.diskInspector             = DiskInspectorColors();
        light.diskInspector.dataField   = 0xFF123456;
        palette                         = DiskInspectorPalette::Resolve (light);

        Assert::IsFalse (palette.isDarkSurface);
        Assert::AreEqual (0xFF123456u, palette.GetKindColor (PlatterKind::DataField));
        Assert::AreEqual (DiskInspectorColors::MakeLight().sync, palette.GetKindColor (PlatterKind::Sync));
    }
};
