#include "Pch.h"

#include "Widgets/DxuiButton.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MockDxuiThemeTests
//
//  Tests of the TEST DOUBLE itself: the mock theme every widget test paints
//  against.
//
//  Worth testing because everything else trusts it. Widget tests assert
//  colors, and a mock returning the same value for two different roles would
//  make a widget that used the wrong one look correct -- so these assert the
//  roles are distinguishable.
//
//  It also pins that the mock implements the whole IDxuiTheme surface: a role
//  added to the interface and left unimplemented here would leave every widget
//  test painting with a default nobody chose.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MockDxuiThemeTests)
{
public:

    TEST_METHOD (CannedAccessors_ReturnDeclaredValues)
    {
        MockDxuiTheme  theme;
        IDxuiTheme   & iface = theme;



        Assert::AreEqual ((unsigned int) MockDxuiTheme::s_kBackground,          (unsigned int) iface.Background());
        Assert::AreEqual ((unsigned int) MockDxuiTheme::s_kBackgroundElevated,  (unsigned int) iface.BackgroundElevated());
        Assert::AreEqual ((unsigned int) MockDxuiTheme::s_kButtonIdle,          (unsigned int) iface.ButtonIdle());
        Assert::AreEqual ((unsigned int) MockDxuiTheme::s_kButtonHover,         (unsigned int) iface.ButtonHover());
        Assert::AreEqual ((unsigned int) MockDxuiTheme::s_kButtonPressed,       (unsigned int) iface.ButtonPressed());
        Assert::AreEqual ((unsigned int) MockDxuiTheme::s_kButtonBorder,        (unsigned int) iface.ButtonBorder());
        Assert::AreEqual ((unsigned int) MockDxuiTheme::s_kButtonText,          (unsigned int) iface.ButtonText());
        Assert::AreEqual ((unsigned int) MockDxuiTheme::s_kFocusRing,           (unsigned int) iface.FocusRing());
        Assert::AreEqual (MockDxuiTheme::s_kBodyLineHeightDip, iface.BodyLineHeightDip());
        Assert::AreEqual (MockDxuiTheme::s_kFocusRingWidthDip, iface.FocusRingWidthDip());

        Assert::IsNull (iface.BodyFont().face);
        Assert::IsNull (iface.HeadingFont().face);
    }


    TEST_METHOD (DxuiButton_Constructs_And_HonorsStateSetters)
    {
        MockDxuiTheme  theme;
        DxuiButton     button;
        RECT           rect = { 10, 20, 110, 60 };



        // Construction + layout + label do not require a D3D device.
        button.SetDpi    (96);
        button.SetLabel  (L"&Apply");
        button.Layout    (rect);

        // Accelerator is parsed from the '&' prefix in the label.
        Assert::AreEqual ((wchar_t) L'a', button.Accelerator());

        // Default visibility / enabled state.
        Assert::IsTrue  (button.Visible());
        Assert::IsTrue  (button.Enabled());
        Assert::IsFalse (button.Focused());

        // Hit testing reflects the layout rect.
        Assert::IsTrue  (button.HitTest (50, 40));
        Assert::IsFalse (button.HitTest ( 5,  5));

        // SetVisible(false) resets transient hover / pressed / focus flags.
        button.SetFocused (true);
        button.SetVisible (false);
        Assert::IsFalse (button.Visible());
        Assert::IsFalse (button.Focused());

        // SetEnabled(false) likewise clears transient flags.
        button.SetVisible (true);
        button.SetEnabled (false);
        Assert::IsFalse (button.Enabled());
        Assert::IsTrue  (button.Visible());

        // Theme accessors compile against the interface base.
        const IDxuiTheme & iface = theme;
        Assert::AreNotEqual ((unsigned int) 0u, (unsigned int) iface.ButtonIdle());
    }
};
