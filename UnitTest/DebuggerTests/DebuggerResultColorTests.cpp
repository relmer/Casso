#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerResultColorTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerResultColorTests
    //
    //  An operand's result is drawn in a color the theme gives, and every theme
    //  the debugger offers gives one that reads against its code listing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerResultColorTests)
    {
    public:

        static void  AssertLegible (const DxuiTheme & theme, const wchar_t * which)
        {
            static constexpr float  kMinContrast = 4.5f;
            float                   contrast     = 0.0f;



            Assert::AreNotEqual (0u, theme.resultText, which);

            contrast = DxuiColor::ComputeContrastRatio (theme.resultText, theme.ContentBackground());

            Assert::IsTrue (contrast >= kMinContrast, which);
        }


        TEST_METHOD (EveryCassoThemeGivesALegibleResultColor)
        {
            AssertLegible (CassoTheme::MakeSkeuomorphic(),  L"Skeuomorphic");
            AssertLegible (CassoTheme::MakeDarkModern(),    L"Dark modern");
            AssertLegible (CassoTheme::MakeRetroTerminal(), L"Retro terminal");
        }


        TEST_METHOD (TheSystemThemesGiveALegibleResultColor)
        {
            DxuiLightTheme  light;
            DxuiDarkTheme   dark;



            AssertLegible (light,              L"System light");
            AssertLegible (dark,               L"System dark");
            AssertLegible (DxuiTheme::Light(), L"Dxui light");
            AssertLegible (DxuiTheme::Dark(),  L"Dxui dark");
        }
    };
}
