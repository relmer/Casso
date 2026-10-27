#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayIndicator
//
//  "Replaying" and the host's time of the machine's position, over the top
//  of the emulated screen while the machine runs behind live, its opacity
//  pulsing so it is not mistaken for the guest's text. Not drawn live,
//  paused behind live or while rewinding, where the rewind bar and the
//  caption display the position, nor in a capture paint.
//
//  All on the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

class ReplayIndicator
{
public:
    static constexpr float    kFontDip     = 20.0f;
    static constexpr int      kInsetDip    = 12;
    static constexpr int      kHeightDip   = 36;
    static constexpr int64_t  kPulseMs     = 1600;     // one fade out and back
    static constexpr float    kMinOpacity  = 0.4f;

    static bool   IsShown    (bool isBehindLive, bool isPaused, bool isRewinding, bool isHidden);
    static float  GetOpacity (int64_t nowMs);

    void  Adopt     (DxuiPanel & root);

    //  Once a frame: drawn or not, with the text, centered across the screen
    //  just below topEdge, the lowest edge of the chrome docked over it.
    void  Sync      (bool isShown, const std::wstring & text, const RECT & screen, LONG topEdge, const DxuiDpiScaler & scaler, int64_t nowMs);
    void  Hide      ()       { m_text.SetVisible (false); }

    bool  IsVisible () const { return m_text.IsVisible(); }

private:
    DxuiShadowedText  m_text;
};
