#pragma once

#include "Pch.h"

#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HudBanner
//
//  A line of text over the picture, legible against whatever is behind it.
//
//  The problem a plain label has here is that it is drawn over live emulator
//  output: light text vanishes into a bright frame, dark text into a dark one,
//  and a solid backing plate reads as a dialog someone forgot to dismiss. The
//  answer is a FEATHERED SHADOW -- nested rectangles, each a step narrower
//  than the last and each carrying the same small alpha, so their overlap
//  accumulates into a smooth ramp from nothing at the edge to nearly opaque
//  over the ink.
//
//  What that gives is a dark cloud with no edge to notice: it reads as the
//  background darkening under the words rather than as a panel behind them,
//  and the text stays legible over anything the machine is showing.
//
////////////////////////////////////////////////////////////////////////////////

class HudBanner : public IDxuiControl
{
public:
    HudBanner  () = default;
    ~HudBanner () override = default;

    // Visibility and bounds are the base control's -- SetVisible / Visible
    // and SetBounds / Bounds come from IDxuiControl, and a second copy here
    // would shadow the ones the panel tree actually consults.
    void  SetText        (const std::wstring & text) { m_text = text; }
    void  SetFontSizeDip (float sizeDip)             { m_fontSizeDip = sizeDip; }
    void  SetDpi         (UINT dpi)                  { m_dpi = dpi; }

    // The banner spans `boundsDip` and centers its line inside it. The
    // shadow needs room to fade in: a rect fitted to the ink alone clips the
    // outermost rings against its own edges.
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    void  Paint  (IDxuiPainter      & painter,
                  IDxuiTextRenderer & text,
                  const IDxuiTheme  & theme) override;

    // How far the shadow reaches past the text, how many layers build that
    // reach, and how dark it gets over the ink. Twenty-eight is wide enough
    // that the fade has somewhere to happen; twenty layers is smooth at any
    // DPI, since the step scales with the reach rather than the pixel.
    static constexpr float  kFeatherDp     = 28.0f;
    static constexpr int    kFeatherLayers = 20;
    static constexpr float  kMaxOpacity    = 0.82f;
    static constexpr float  kFontDip       = 13.0f;

private:
    std::wstring  m_text;
    float         m_fontSizeDip = kFontDip;
    UINT          m_dpi         = 96;
};
