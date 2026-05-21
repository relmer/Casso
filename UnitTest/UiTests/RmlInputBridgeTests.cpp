#include "Pch.h"

#include "Ui/RmlInputBridge.h"

#include <RmlUi/Core/Input.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RmlInputBridgeTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RmlInputBridgeTests)
{
public:

    TEST_METHOD (VK_A_Maps_To_KI_A)
    {
        Assert::AreEqual ((int) Rml::Input::KI_A,
                          RmlInputBridge::TranslateVirtualKey ('A'));
    }


    TEST_METHOD (VK_Z_Maps_To_KI_Z)
    {
        Assert::AreEqual ((int) Rml::Input::KI_Z,
                          RmlInputBridge::TranslateVirtualKey ('Z'));
    }


    TEST_METHOD (VK_0_Maps_To_KI_0)
    {
        Assert::AreEqual ((int) Rml::Input::KI_0,
                          RmlInputBridge::TranslateVirtualKey ('0'));
    }


    TEST_METHOD (VK_RETURN_Maps_To_KI_RETURN)
    {
        Assert::AreEqual ((int) Rml::Input::KI_RETURN,
                          RmlInputBridge::TranslateVirtualKey (VK_RETURN));
    }


    TEST_METHOD (VK_ESCAPE_Maps_To_KI_ESCAPE)
    {
        Assert::AreEqual ((int) Rml::Input::KI_ESCAPE,
                          RmlInputBridge::TranslateVirtualKey (VK_ESCAPE));
    }


    TEST_METHOD (Arrow_Keys_Map_Correctly)
    {
        Assert::AreEqual ((int) Rml::Input::KI_LEFT,
                          RmlInputBridge::TranslateVirtualKey (VK_LEFT));
        Assert::AreEqual ((int) Rml::Input::KI_RIGHT,
                          RmlInputBridge::TranslateVirtualKey (VK_RIGHT));
        Assert::AreEqual ((int) Rml::Input::KI_UP,
                          RmlInputBridge::TranslateVirtualKey (VK_UP));
        Assert::AreEqual ((int) Rml::Input::KI_DOWN,
                          RmlInputBridge::TranslateVirtualKey (VK_DOWN));
    }


    TEST_METHOD (FKeys_F1_Through_F12_Map_Correctly)
    {
        Assert::AreEqual ((int) Rml::Input::KI_F1,
                          RmlInputBridge::TranslateVirtualKey (VK_F1));
        Assert::AreEqual ((int) Rml::Input::KI_F10,
                          RmlInputBridge::TranslateVirtualKey (VK_F10));
        Assert::AreEqual ((int) Rml::Input::KI_F12,
                          RmlInputBridge::TranslateVirtualKey (VK_F12));
    }


    TEST_METHOD (Unknown_VK_Returns_Unknown)
    {
        Assert::AreEqual (RmlInputBridge::Key::Unknown,
                          RmlInputBridge::TranslateVirtualKey (0xFE));
    }


    TEST_METHOD (Modifiers_None)
    {
        Assert::AreEqual (0, RmlInputBridge::SynthesizeModifiers (false, false, false, false, false, false));
    }


    TEST_METHOD (Modifiers_CtrlShift)
    {
        int  m = RmlInputBridge::SynthesizeModifiers (true, true, false, false, false, false);
        Assert::IsTrue ((m & RmlInputBridge::Mod::Ctrl)  != 0);
        Assert::IsTrue ((m & RmlInputBridge::Mod::Shift) != 0);
        Assert::IsTrue ((m & RmlInputBridge::Mod::Alt)   == 0);
    }


    TEST_METHOD (Modifiers_AllLocks)
    {
        int  m = RmlInputBridge::SynthesizeModifiers (false, false, false, true, true, true);
        Assert::AreEqual (RmlInputBridge::Mod::CapsLock | RmlInputBridge::Mod::NumLock | RmlInputBridge::Mod::ScrollLock,
                          m);
    }


    TEST_METHOD (MouseButton_Left)
    {
        Assert::AreEqual (RmlInputBridge::MouseButton::Left,
                          RmlInputBridge::TranslateMouseButtonMessage (WM_LBUTTONDOWN, 0));
        Assert::AreEqual (RmlInputBridge::MouseButton::Left,
                          RmlInputBridge::TranslateMouseButtonMessage (WM_LBUTTONUP,   0));
    }


    TEST_METHOD (MouseButton_Right)
    {
        Assert::AreEqual (RmlInputBridge::MouseButton::Right,
                          RmlInputBridge::TranslateMouseButtonMessage (WM_RBUTTONDOWN, 0));
    }


    TEST_METHOD (MouseButton_Middle)
    {
        Assert::AreEqual (RmlInputBridge::MouseButton::Middle,
                          RmlInputBridge::TranslateMouseButtonMessage (WM_MBUTTONDOWN, 0));
    }


    TEST_METHOD (MouseButton_X1_X2)
    {
        Assert::AreEqual (RmlInputBridge::MouseButton::X1,
                          RmlInputBridge::TranslateMouseButtonMessage (WM_XBUTTONDOWN, XBUTTON1));
        Assert::AreEqual (RmlInputBridge::MouseButton::X2,
                          RmlInputBridge::TranslateMouseButtonMessage (WM_XBUTTONUP,   XBUTTON2));
    }


    TEST_METHOD (IsMouseDown_True_For_Down_Messages)
    {
        Assert::IsTrue  (RmlInputBridge::IsMouseDownMessage (WM_LBUTTONDOWN));
        Assert::IsTrue  (RmlInputBridge::IsMouseDownMessage (WM_LBUTTONDBLCLK));
        Assert::IsFalse (RmlInputBridge::IsMouseDownMessage (WM_LBUTTONUP));
        Assert::IsFalse (RmlInputBridge::IsMouseDownMessage (WM_MOUSEMOVE));
    }


    TEST_METHOD (IsMouseUp_True_For_Up_Messages)
    {
        Assert::IsTrue  (RmlInputBridge::IsMouseUpMessage (WM_RBUTTONUP));
        Assert::IsFalse (RmlInputBridge::IsMouseUpMessage (WM_RBUTTONDOWN));
    }


    TEST_METHOD (WheelDelta_Up_Tick)
    {
        Assert::AreEqual (-1.0f, RmlInputBridge::NormalizeWheelDelta (120));
    }


    TEST_METHOD (WheelDelta_Down_Tick)
    {
        Assert::AreEqual (1.0f, RmlInputBridge::NormalizeWheelDelta (-120));
    }


    TEST_METHOD (CoalesceUtf16_Bmp_Char)
    {
        wchar_t   pending = 0;
        char32_t  cp      = 0;

        bool ready = RmlInputBridge::CoalesceUtf16Char (L'A', pending, cp);
        Assert::IsTrue (ready);
        Assert::AreEqual ((unsigned int) (char32_t) 'A', (unsigned int) cp);
        Assert::AreEqual ((wchar_t) 0, pending);
    }


    TEST_METHOD (CoalesceUtf16_SurrogatePair_Emoji)
    {
        wchar_t   pending = 0;
        char32_t  cp      = 0;
        bool      ready   = false;

        // U+1F600 == surrogate pair (0xD83D, 0xDE00) -> 😀
        ready = RmlInputBridge::CoalesceUtf16Char ((wchar_t) 0xD83D, pending, cp);
        Assert::IsFalse (ready);
        Assert::AreEqual ((wchar_t) 0xD83D, pending);

        ready = RmlInputBridge::CoalesceUtf16Char ((wchar_t) 0xDE00, pending, cp);
        Assert::IsTrue (ready);
        Assert::AreEqual ((unsigned int) (char32_t) 0x1F600, (unsigned int) cp);
        Assert::AreEqual ((wchar_t) 0, pending);
    }


    TEST_METHOD (CoalesceUtf16_Stray_LowSurrogate_Dropped)
    {
        wchar_t   pending = 0;
        char32_t  cp      = 0;

        bool ready = RmlInputBridge::CoalesceUtf16Char ((wchar_t) 0xDE00, pending, cp);
        Assert::IsFalse (ready);
    }
};
