#include "Pch.h"

#include "RmlInputBridge.h"






////////////////////////////////////////////////////////////////////////////////
//
//  Static asserts: confirm our mirrored constants match RmlUi 6.2.
//
////////////////////////////////////////////////////////////////////////////////

static_assert (RmlInputBridge::Mod::Ctrl       == (int) Rml::Input::KM_CTRL,        "KM_CTRL drift");
static_assert (RmlInputBridge::Mod::Shift      == (int) Rml::Input::KM_SHIFT,       "KM_SHIFT drift");
static_assert (RmlInputBridge::Mod::Alt        == (int) Rml::Input::KM_ALT,         "KM_ALT drift");
static_assert (RmlInputBridge::Mod::Meta       == (int) Rml::Input::KM_META,        "KM_META drift");
static_assert (RmlInputBridge::Mod::CapsLock   == (int) Rml::Input::KM_CAPSLOCK,    "KM_CAPSLOCK drift");
static_assert (RmlInputBridge::Mod::NumLock    == (int) Rml::Input::KM_NUMLOCK,     "KM_NUMLOCK drift");
static_assert (RmlInputBridge::Mod::ScrollLock == (int) Rml::Input::KM_SCROLLLOCK,  "KM_SCROLLLOCK drift");





////////////////////////////////////////////////////////////////////////////////
//
//  RmlInputBridge::TranslateVirtualKey
//
////////////////////////////////////////////////////////////////////////////////

int RmlInputBridge::TranslateVirtualKey (unsigned int vk)
{
    if (vk >= 'A' && vk <= 'Z')
    {
        return (int) Rml::Input::KI_A + (int) (vk - 'A');
    }

    if (vk >= '0' && vk <= '9')
    {
        return (int) Rml::Input::KI_0 + (int) (vk - '0');
    }

    switch (vk)
    {
        case VK_SPACE:    return (int) Rml::Input::KI_SPACE;
        case VK_BACK:     return (int) Rml::Input::KI_BACK;
        case VK_TAB:      return (int) Rml::Input::KI_TAB;
        case VK_RETURN:   return (int) Rml::Input::KI_RETURN;
        case VK_ESCAPE:   return (int) Rml::Input::KI_ESCAPE;
        case VK_DELETE:   return (int) Rml::Input::KI_DELETE;
        case VK_INSERT:   return (int) Rml::Input::KI_INSERT;

        case VK_LEFT:     return (int) Rml::Input::KI_LEFT;
        case VK_RIGHT:    return (int) Rml::Input::KI_RIGHT;
        case VK_UP:       return (int) Rml::Input::KI_UP;
        case VK_DOWN:     return (int) Rml::Input::KI_DOWN;
        case VK_HOME:     return (int) Rml::Input::KI_HOME;
        case VK_END:      return (int) Rml::Input::KI_END;
        case VK_PRIOR:    return (int) Rml::Input::KI_PRIOR;
        case VK_NEXT:     return (int) Rml::Input::KI_NEXT;

        case VK_SHIFT:    return (int) Rml::Input::KI_LSHIFT;
        case VK_CONTROL:  return (int) Rml::Input::KI_LCONTROL;
        case VK_MENU:     return (int) Rml::Input::KI_LMENU;
        case VK_LSHIFT:   return (int) Rml::Input::KI_LSHIFT;
        case VK_LCONTROL: return (int) Rml::Input::KI_LCONTROL;
        case VK_LMENU:    return (int) Rml::Input::KI_LMENU;

        case VK_F1:       return (int) Rml::Input::KI_F1;
        case VK_F2:       return (int) Rml::Input::KI_F1 + 1;
        case VK_F3:       return (int) Rml::Input::KI_F1 + 2;
        case VK_F4:       return (int) Rml::Input::KI_F1 + 3;
        case VK_F5:       return (int) Rml::Input::KI_F1 + 4;
        case VK_F6:       return (int) Rml::Input::KI_F1 + 5;
        case VK_F7:       return (int) Rml::Input::KI_F1 + 6;
        case VK_F8:       return (int) Rml::Input::KI_F1 + 7;
        case VK_F9:       return (int) Rml::Input::KI_F1 + 8;
        case VK_F10:      return (int) Rml::Input::KI_F10;
        case VK_F11:      return (int) Rml::Input::KI_F11;
        case VK_F12:      return (int) Rml::Input::KI_F12;

        default:
            return Key::Unknown;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RmlInputBridge::SynthesizeModifiers
//
////////////////////////////////////////////////////////////////////////////////

int RmlInputBridge::SynthesizeModifiers (
    bool fCtrl,
    bool fShift,
    bool fAlt,
    bool fCapsLock,
    bool fNumLock,
    bool fScrollLock)
{
    int  mods = 0;



    if (fCtrl)        mods |= Mod::Ctrl;
    if (fShift)       mods |= Mod::Shift;
    if (fAlt)         mods |= Mod::Alt;
    if (fCapsLock)    mods |= Mod::CapsLock;
    if (fNumLock)     mods |= Mod::NumLock;
    if (fScrollLock)  mods |= Mod::ScrollLock;

    return mods;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RmlInputBridge::TranslateMouseButtonMessage
//
////////////////////////////////////////////////////////////////////////////////

int RmlInputBridge::TranslateMouseButtonMessage (
    unsigned int     msg,
    unsigned short   xbutton)
{
    switch (msg)
    {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
            return MouseButton::Left;
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
            return MouseButton::Right;
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
            return MouseButton::Middle;
        case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
            return (xbutton == XBUTTON1) ? MouseButton::X1
                 : (xbutton == XBUTTON2) ? MouseButton::X2
                 :                         MouseButton::None;
        default:
            return MouseButton::None;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RmlInputBridge::IsMouseDownMessage / IsMouseUpMessage
//
////////////////////////////////////////////////////////////////////////////////

bool RmlInputBridge::IsMouseDownMessage (unsigned int msg)
{
    return msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK
        || msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK
        || msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK
        || msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMouseUpMessage
//
////////////////////////////////////////////////////////////////////////////////

bool RmlInputBridge::IsMouseUpMessage (unsigned int msg)
{
    return msg == WM_LBUTTONUP
        || msg == WM_RBUTTONUP
        || msg == WM_MBUTTONUP
        || msg == WM_XBUTTONUP;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RmlInputBridge::NormalizeWheelDelta
//
//  Win32 ships wheel deltas as `(short)HIWORD(wparam)` in 120-unit
//  increments (`WHEEL_DELTA`). Convert to "lines scrolled" — the unit
//  Rml expects for `Context::ProcessMouseWheel`. Sign is inverted vs
//  Win32: positive in Rml means scrolling down toward the user.
//
////////////////////////////////////////////////////////////////////////////////

float RmlInputBridge::NormalizeWheelDelta (short rawDelta)
{
    constexpr float  kWheelDelta = 120.0f;
    return -((float) rawDelta) / kWheelDelta;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RmlInputBridge::CoalesceUtf16Char
//
////////////////////////////////////////////////////////////////////////////////

bool RmlInputBridge::CoalesceUtf16Char (
    wchar_t      ch,
    wchar_t    & ioPendingHighSurrogate,
    char32_t   & outCodePoint)
{
    constexpr wchar_t  kHighStart  = 0xD800;
    constexpr wchar_t  kHighEnd    = 0xDBFF;
    constexpr wchar_t  kLowStart   = 0xDC00;
    constexpr wchar_t  kLowEnd     = 0xDFFF;



    if (ch >= kHighStart && ch <= kHighEnd)
    {
        ioPendingHighSurrogate = ch;
        return false;
    }

    if (ch >= kLowStart && ch <= kLowEnd)
    {
        if (ioPendingHighSurrogate == 0)
        {
            return false;
        }
        outCodePoint = 0x10000
                     + (((char32_t) (ioPendingHighSurrogate - kHighStart)) << 10)
                     +  ((char32_t) (ch - kLowStart));
        ioPendingHighSurrogate = 0;
        return true;
    }

    ioPendingHighSurrogate = 0;
    outCodePoint = (char32_t) (unsigned int) ch;
    return true;
}
