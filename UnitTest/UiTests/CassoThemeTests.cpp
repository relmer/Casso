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
};
