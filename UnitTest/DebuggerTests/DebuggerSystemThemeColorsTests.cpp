#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerSystemThemeColorsTests
//
//  The debugger's own system themes, after DebuggerThemes::ApplyOwnColors:
//  Visual Studio's light pane colors, measured at 125%, in place of the
//  system's content color, and Casso Explorer's system themes left as they
//  are.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebuggerSystemThemeColorsTests)
    {
    public:

        //  Lists and titles #F9F9F9, text views #FFFFFF, the band #F7F7F7 and
        //  the gap #EEEEEE, an unfocused outline #ADADAD and a focused one
        //  #5649B0. A list's own lines keep the light theme's #E5E5E5.
        TEST_METHOD (SystemLightTakesVisualStudiosPaneColors)
        {
            DxuiLightTheme  light;
            DxuiDarkTheme   dark;



            DebuggerThemes::ApplyOwnColors (light, dark);

            Assert::AreEqual (0xFFF9F9F9u, light.ContentBackground(),  L"lists and titles");
            Assert::AreEqual (0xFFFFFFFFu, light.TextViewBackground(), L"text views");
            Assert::AreEqual (0xFFF7F7F7u, light.PaneBand(),           L"the band behind the tabs");
            Assert::AreEqual (0xFFEEEEEEu, light.DockGap(),            L"the gap between panes");
            Assert::AreEqual (0xFFADADADu, light.Border(),             L"an unfocused pane's outline");
            Assert::AreEqual (0xFF5649B0u, light.FocusAccent(),        L"the focused pane's outline");
            Assert::AreEqual (0xFFE5E5E5u, light.ContentEdge(),        L"a list's lines");
        }


        //  The system's light content color gives way to Visual Studio's, set
        //  after it; the dark one stays, as Casso Explorer's does.
        TEST_METHOD (OnlyTheLightContentColorIsReplaced)
        {
            DxuiLightTheme                         light;
            DxuiDarkTheme                          dark;
            DxuiWindowsThemeColors::SystemColors   system;



            system.hasSurfaces  = true;
            system.contentLight = 0xFFFFFFFF;
            system.contentDark  = 0xFF1A1A1A;

            light.ApplySystemColors (system);
            dark.ApplySystemColors  (system);

            DebuggerThemes::ApplyOwnColors (light, dark);

            Assert::AreEqual (0xFFF9F9F9u, light.ContentBackground(), L"light lists are Visual Studio's, not the system's");
            Assert::AreEqual (0xFF1A1A1Au, dark.ContentBackground(),  L"dark lists keep the system's");
            Assert::AreEqual (0xFF1A1A1Au, dark.TextViewBackground(), L"and dark text views with them");
        }


        //  Casso Explorer's own system themes keep their pane colors.
        TEST_METHOD (ExplorersSystemThemesAreLeftAlone)
        {
            DxuiLightTheme  light;



            Assert::AreEqual (0xFFE5E5E5u,               light.Border(),             L"the light outline");
            Assert::AreEqual (light.ContentBackground(), light.TextViewBackground(), L"text views on the content color");
            Assert::AreEqual (light.Accent(),            light.FocusAccent(),        L"the accent marks focus");
        }
    };
}
