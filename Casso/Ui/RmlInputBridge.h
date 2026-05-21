#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RmlInputBridge
//
//  Pure-logic translation layer between Win32 window messages and the
//  RmlUi input enums declared in `<RmlUi/Core/Input.h>`. The actual
//  dispatch to `Rml::Context::Process*` is left to the caller — this
//  module is deliberately decoupled from any live context so it can be
//  unit-tested without spinning up a real RmlUi instance.
//
//  Coverage ( acceptance):
//      VK_* -> Rml::Input::KeyIdentifier
//      modifier-bitmask synthesis (Ctrl/Shift/Alt + lock states)
//      mouse-button index for WM_LBUTTON/RBUTTON/MBUTTON/X1/X2
//      WM_MOUSEWHEEL delta normalization
//      WM_CHAR UTF-16 surrogate-pair coalescing
//
////////////////////////////////////////////////////////////////////////////////

class RmlInputBridge
{
public:
    // Sentinel for "this VK has no mapping in RmlUi".
    struct Key
    {
        static constexpr int Unknown = 0;
    };

    // Mirror of Rml::Input::KeyModifier bit values.
    struct Mod
    {
        static constexpr int Ctrl       = 1 << 0;
        static constexpr int Shift      = 1 << 1;
        static constexpr int Alt        = 1 << 2;
        static constexpr int Meta       = 1 << 3;
        static constexpr int CapsLock   = 1 << 4;
        static constexpr int NumLock    = 1 << 5;
        static constexpr int ScrollLock = 1 << 6;
    };

    struct MouseButton
    {
        static constexpr int None    = -1;
        static constexpr int Left    = 0;
        static constexpr int Right   = 1;
        static constexpr int Middle  = 2;
        static constexpr int X1      = 3;
        static constexpr int X2      = 4;
    };

    static int   TranslateVirtualKey        (unsigned int vk);
    static int   SynthesizeModifiers        (bool  fCtrl,
                                             bool  fShift,
                                             bool  fAlt,
                                             bool  fCapsLock,
                                             bool  fNumLock,
                                             bool  fScrollLock);
    static int   TranslateMouseButtonMessage (unsigned int   msg,
                                              unsigned short xbutton);
    static bool  IsMouseDownMessage         (unsigned int msg);
    static bool  IsMouseUpMessage           (unsigned int msg);
    static float NormalizeWheelDelta        (short rawDelta);
    static bool  CoalesceUtf16Char          (wchar_t    ch,
                                             wchar_t  & ioPendingHighSurrogate,
                                             char32_t & outCodePoint);
};
