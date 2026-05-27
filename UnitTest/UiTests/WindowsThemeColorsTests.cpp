#include "Pch.h"

#include "Ui/WindowsThemeColors.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColorsTests
//
//  Light-touch coverage of the Fluent caption-button color tokens
//  surfaced by WindowsThemeColors. Asserts the token contracts that
//  the chrome painter depends on without mutating the user's real
//  registry: the close button is the same red in hover and pressed,
//  the close glyph stays opaque white, and the active mode picks the
//  documented light/dark hover/pressed values.
//
////////////////////////////////////////////////////////////////////////////////

namespace WindowsThemeColorsTests
{
    TEST_CLASS (WindowsThemeColorsTests)
    {
    public:

        TEST_METHOD (CloseButton_Background_Same_For_Hover_And_Pressed)
        {
            WindowsThemeColors &  sys = WindowsThemeColors::Instance();


            Assert::AreEqual ((unsigned long) 0xFFC42B1Cu,
                              (unsigned long) sys.CloseButtonHoverArgb());
            Assert::AreEqual ((unsigned long) sys.CloseButtonHoverArgb(),
                              (unsigned long) sys.CloseButtonPressedArgb());
        }


        TEST_METHOD (CloseButton_Glyph_Hover_Opaque_Pressed_Slightly_Faded)
        {
            WindowsThemeColors &  sys = WindowsThemeColors::Instance();
            uint32_t              hoverAlpha   = (sys.CloseButtonGlyphHoverArgb()   >> 24) & 0xFFu;
            uint32_t              pressedAlpha = (sys.CloseButtonGlyphPressedArgb() >> 24) & 0xFFu;


            Assert::AreEqual ((unsigned long) 0xFFu, (unsigned long) hoverAlpha);
            Assert::IsTrue   (pressedAlpha < hoverAlpha);
        }


        TEST_METHOD (CloseButton_Foreground_Over_Is_Opaque_White)
        {
            WindowsThemeColors &  sys = WindowsThemeColors::Instance();


            Assert::AreEqual ((unsigned long) 0xFFFFFFFFu,
                              (unsigned long) sys.CloseButtonGlyphHoverArgb());
        }


        TEST_METHOD (Caption_Hover_Pressed_Tokens_Match_Active_Mode)
        {
            WindowsThemeColors &  sys = WindowsThemeColors::Instance();
            uint32_t              expectedHover   = sys.IsDarkMode() ? 0x0FFFFFFFu : 0x09000000u;
            uint32_t              expectedPressed = sys.IsDarkMode() ? 0x0AFFFFFFu : 0x06000000u;


            Assert::AreEqual ((unsigned long) expectedHover,
                              (unsigned long) sys.CaptionButtonHoverArgb());
            Assert::AreEqual ((unsigned long) expectedPressed,
                              (unsigned long) sys.CaptionButtonPressedArgb());
        }


        TEST_METHOD (Caption_Foreground_Is_Solid_For_Both_Modes)
        {
            WindowsThemeColors &  sys = WindowsThemeColors::Instance();


            Assert::AreEqual ((unsigned long) 0xFFu,
                              (unsigned long) ((sys.CaptionButtonForegroundArgb() >> 24) & 0xFFu));
        }


        TEST_METHOD (Refresh_Does_Not_Crash)
        {
            WindowsThemeColors &  sys = WindowsThemeColors::Instance();


            sys.Refresh();
            sys.Refresh();
            Assert::IsTrue (sys.IsDarkMode() == true || sys.IsDarkMode() == false);
        }
    };
}
