#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoThemeTests
//
//  The desk-scene fallback in MakeByName. The skeuomorphic theme's drives
//  exist only as 3D objects in the desk scene, and the flat drive widget has
//  no skeuomorphic form, so a skeuomorphic theme shown without the scene
//  would show no drives at all. The two-argument MakeByName swaps in a
//  compact theme for that case and leaves every other case alone.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoThemeTests)
{
public:

    TEST_METHOD (Skeuomorphic_WithoutDeskScene_FallsBackToCompact)
    {
        CassoTheme  theme = CassoTheme::MakeByName ("Skeuomorphic", false);

        Assert::IsTrue (theme.compactDrives,
                        L"a theme whose drives need the scene is replaced when the scene is gone");
        Assert::AreEqual (CassoTheme::MakeDarkModern().navStrip, theme.navStrip,
                          L"and the stand-in is Dark Modern");
    }


    TEST_METHOD (Skeuomorphic_WithDeskScene_IsUnchanged)
    {
        CassoTheme  theme = CassoTheme::MakeByName ("Skeuomorphic", true);

        Assert::IsFalse (theme.compactDrives, L"with the scene available the chosen theme stands");
        Assert::AreEqual (CassoTheme::MakeSkeuomorphic().navStrip, theme.navStrip);
    }


    TEST_METHOD (CompactThemes_WithoutDeskScene_AreUnchanged)
    {
        CassoTheme  retro  = CassoTheme::MakeByName ("RetroTerminal", false);
        CassoTheme  modern = CassoTheme::MakeByName ("DarkModern",    false);

        // A compact theme never needed the scene, so losing it changes nothing.
        Assert::AreEqual (CassoTheme::MakeRetroTerminal().navStrip, retro.navStrip,
                          L"Retro Terminal is not replaced by Dark Modern");
        Assert::AreEqual (CassoTheme::MakeDarkModern().navStrip, modern.navStrip);
    }


    TEST_METHOD (Phosphor_IsCompactAndDrawsInVt323)
    {
        CassoTheme  theme = CassoTheme::MakeByName ("Phosphor", false);

        Assert::IsTrue (theme.compactDrives, L"Phosphor uses the flat drive widgets");
        Assert::IsTrue (theme.bodyFace == CassoTheme::BodyFace::Vt323);
        Assert::AreEqual (CassoTheme::MakePhosphor().navStrip, theme.navStrip,
                          L"Phosphor is not replaced by Dark Modern");
    }


    TEST_METHOD (Phosphor_MenuHighlightIsInverseVideo)
    {
        CassoTheme  theme = CassoTheme::MakePhosphor();

        Assert::AreEqual (theme.navItemText,      theme.navStripHover,     L"a lit menu title is filled with the text color");
        Assert::AreEqual (theme.navStrip,         theme.navHoverText,      L"and its text takes the strip color");
        Assert::AreEqual (theme.dropdownItemText, theme.dropdownHover,     L"a hovered menu row is filled with the text color");
        Assert::AreEqual (theme.dropdownBg,       theme.dropdownHoverText, L"and its text takes the menu background");

        // The generic hover fill sits under ordinary body text in buttons,
        // lists and tabs, so it must not be the text color.
        Assert::AreNotEqual (theme.bodyText, theme.navHover);
    }


    TEST_METHOD (OtherThemes_KeepTheirTextColorOnHover)
    {
        CassoTheme  themes[] = { CassoTheme::MakeSkeuomorphic(),
                                 CassoTheme::MakeDarkModern(),
                                 CassoTheme::MakeRetroTerminal() };

        for (const CassoTheme & theme : themes)
        {
            Assert::AreEqual (theme.navItemText,      theme.navHoverText);
            Assert::AreEqual (theme.dropdownItemText, theme.dropdownHoverText);
            Assert::AreEqual (theme.dropdownAccel,    theme.dropdownHoverAccel);
            Assert::AreEqual (theme.navHover,         theme.navStripHover);
            Assert::IsTrue   (theme.bodyFace == CassoTheme::BodyFace::System);
            Assert::AreEqual ((int) 0, (int) theme.textGlowAlpha);
            Assert::IsFalse  (theme.monochromeGlyphs);
        }
    }
};
