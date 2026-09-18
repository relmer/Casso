#include "Pch.h"

#include "Theme/DxuiDarkTheme.h"
#include "Theme/DxuiLightTheme.h"
#include "Theme/DxuiWindowsThemeColors.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemColorsTests
//
//  Decoding the accent ramp Windows provides, and which values each Windows
//  theme uses. The bytes are the AccentPalette value from a machine with the
//  default blue accent, checked against UISettings; no test reads the machine
//  it runs on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiSystemColorsTests)
{
public:

    static constexpr BYTE  kDefaultBluePalette[32] =
    {
        0x99, 0xEB, 0xFF, 0x00,   //  Light3
        0x4C, 0xC2, 0xFF, 0x00,   //  Light2
        0x00, 0x91, 0xF8, 0x00,   //  Light1
        0x00, 0x78, 0xD4, 0x00,   //  the accent
        0x00, 0x67, 0xC0, 0x00,   //  Dark1
        0x00, 0x3E, 0x92, 0x00,   //  Dark2
        0x00, 0x1A, 0x68, 0x00,   //  Dark3
        0xF7, 0x63, 0x0C, 0x00,
    };


    TEST_METHOD (DecodeAccentPalette_MatchesUISettings)
    {
        DxuiWindowsThemeColors::SystemColors  colors;


        Assert::IsTrue (DxuiWindowsThemeColors::DecodeAccentPalette (kDefaultBluePalette, colors));

        Assert::IsTrue   (colors.hasAccent);
        Assert::AreEqual (0xFF99EBFFu, colors.accentLight3, L"UISettings AccentLight3");
        Assert::AreEqual (0xFF4CC2FFu, colors.accentLight2, L"UISettings AccentLight2");
        Assert::AreEqual (0xFF0067C0u, colors.accentDark1,  L"UISettings AccentDark1");
        Assert::AreEqual (0xFF003E92u, colors.accentDark2,  L"UISettings AccentDark2");
    }


    TEST_METHOD (DecodeAccentPalette_RefusesAShortValue)
    {
        DxuiWindowsThemeColors::SystemColors  colors;


        Assert::IsFalse (DxuiWindowsThemeColors::DecodeAccentPalette (std::span<const BYTE> (kDefaultBluePalette, 16), colors),
            L"Half a palette is not a palette");
        Assert::IsFalse (colors.hasAccent, L"and nothing is claimed from it");
    }


    TEST_METHOD (ToArgb_TurnsAColorrefAround)
    {
        Assert::AreEqual (0xFF191919u, DxuiWindowsThemeColors::ToArgb (RGB (0x19, 0x19, 0x19)));
        Assert::AreEqual (0xFF0078D4u, DxuiWindowsThemeColors::ToArgb (RGB (0x00, 0x78, 0xD4)),
            L"A COLORREF stores blue high; the painter wants it low");
    }


    TEST_METHOD (DarkTheme_TakesTheLighterAccentAndTheDarkSurface)
    {
        DxuiDarkTheme                         theme;
        DxuiWindowsThemeColors::SystemColors  colors;


        Assert::IsTrue (DxuiWindowsThemeColors::DecodeAccentPalette (kDefaultBluePalette, colors));
        colors.hasSurfaces  = true;
        colors.contentDark  = 0xFF101010u;
        colors.contentLight = 0xFFFEFEFEu;

        theme.ApplySystemColors (colors);

        Assert::AreEqual (0xFF4CC2FFu, theme.Accent(),            L"A dark surface takes Light2");
        Assert::AreEqual (0xFF99EBFFu, theme.linkHover,           L"with Light3 for hover");
        Assert::AreEqual (0xFF101010u, theme.ContentBackground(), L"and the dark list surface");
    }


    TEST_METHOD (LightTheme_TakesTheDarkerAccentAndTheLightSurface)
    {
        DxuiLightTheme                        theme;
        DxuiWindowsThemeColors::SystemColors  colors;


        Assert::IsTrue (DxuiWindowsThemeColors::DecodeAccentPalette (kDefaultBluePalette, colors));
        colors.hasSurfaces  = true;
        colors.contentDark  = 0xFF101010u;
        colors.contentLight = 0xFFFEFEFEu;

        theme.ApplySystemColors (colors);

        Assert::AreEqual (0xFF0067C0u, theme.Accent(),            L"A light surface takes Dark1");
        Assert::AreEqual (0xFF003E92u, theme.linkHover,           L"with Dark2 for hover");
        Assert::AreEqual (0xFFFEFEFEu, theme.ContentBackground(), L"and the light list surface");
    }


    TEST_METHOD (MissingHalvesKeepTheBuiltInValues)
    {
        DxuiDarkTheme                         builtIn;
        DxuiDarkTheme                         theme;
        DxuiWindowsThemeColors::SystemColors  nothing;


        theme.ApplySystemColors (nothing);

        Assert::AreEqual (builtIn.Accent(),            theme.Accent(),
            L"No accent published: the theme keeps its own");
        Assert::AreEqual (builtIn.ContentBackground(), theme.ContentBackground(),
            L"No visual style: the measured surface stays");
        Assert::AreEqual (builtIn.bodyText,            theme.bodyText,
            L"and body text is never taken from the system at all");
    }
};
