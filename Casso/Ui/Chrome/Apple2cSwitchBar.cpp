#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "Apple2cSwitchBar.h"




////////////////////////////////////////////////////////////////////////////////
//
//  MeasureLabel
//
//  Width of a silk-screen label in pixels. Uses the text renderer when one is
//  wired; before it exists (early layout) falls back to a fixed-pitch estimate,
//  matching the InputDeviceSelector contract.
//
////////////////////////////////////////////////////////////////////////////////

float Apple2cSwitchBar::MeasureLabel (const wchar_t * text, float fontPx) const
{
    float  tw = 0.0f;
    float  th = 0.0f;


    if (m_textRenderer != nullptr)
    {
        HRESULT  hrM = m_textRenderer->MeasureString (text, fontPx, kFontFamily, tw, th);
        if (FAILED (hrM)) { tw = 0.0f; }
    }

    if (tw <= 0.0f)
    {
        tw = (float) wcslen (text) * kFallbackCharPx * (float) m_dpi / 96.0f;
    }

    return tw;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  boundsDip is the whole band rect. The reset button and the two latching
//  switches anchor to the left edge; the disk-use and power indicators anchor
//  to the right edge, so the wide neutral case in the middle mirrors the real
//  //c top panel.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UINT   dpi     = scaler.Dpi();
    UINT   eDpi    = (dpi == 0) ? 96u : dpi;
    auto   px      = [eDpi] (int dp) { return MulDiv (dp, (int) eDpi, 96); };
    int    edge    = px (kEdgePadDp);
    int    resetW  = px (kResetWDp);
    int    resetH  = px (kResetHDp);
    int    gGap    = px (kGroupGapDp);
    int    keyW    = px (kKeyWDp);
    int    keyH    = px (kKeyHDp);
    int    lblGap  = px (kLabelGapDp);
    int    swGap   = px (kSwitchGapDp);
    int    ledW    = px (kLedWDp);
    int    ledH    = px (kLedHDp);
    int    indGap  = px (kIndGapDp);
    float  fontPx  = kFontDip * (float) eDpi / 96.0f;
    int    cy      = (boundsDip.top + boundsDip.bottom) / 2;


    m_dpi    = eDpi;
    m_bounds = boundsDip;

    // Left group: [reset] [80/40 key] "80/40" [keyboard key] "keyboard".
    int  x = boundsDip.left + edge;

    m_resetRect = RECT { x, cy - resetH / 2, x + resetW, cy + resetH / 2 };
    x += resetW + gGap;

    int  eightyLblW = (int) (MeasureLabel (kLabelEighty, fontPx) + 0.5f);
    m_eightyKey   = RECT { x, cy - keyH / 2, x + keyW, cy + keyH / 2 };
    m_eightyLabel = RECT { x + keyW + lblGap, boundsDip.top, x + keyW + lblGap + eightyLblW, boundsDip.bottom };
    m_eightyRect  = RECT { m_eightyKey.left, m_eightyKey.top, m_eightyLabel.right, m_eightyKey.bottom };
    x = m_eightyLabel.right + swGap;

    int  kbdLblW = (int) (MeasureLabel (kLabelKeyboard, fontPx) + 0.5f);
    m_kbdKey   = RECT { x, cy - keyH / 2, x + keyW, cy + keyH / 2 };
    m_kbdLabel = RECT { x + keyW + lblGap, boundsDip.top, x + keyW + lblGap + kbdLblW, boundsDip.bottom };
    m_kbdRect  = RECT { m_kbdKey.left, m_kbdKey.top, m_kbdLabel.right, m_kbdKey.bottom };

    // Right group: [disk-use LED] "disk use"  [power LED] "power", right-anchored.
    int  diskLblW  = (int) (MeasureLabel (kLabelDiskUse, fontPx) + 0.5f);
    int  powerLblW = (int) (MeasureLabel (kLabelPower,   fontPx) + 0.5f);
    int  rightW    = ledW + lblGap + diskLblW + indGap + ledW + lblGap + powerLblW;
    int  rx        = boundsDip.right - edge - rightW;

    m_diskLed   = RECT { rx, cy - ledH / 2, rx + ledW, cy + ledH / 2 };
    rx += ledW + lblGap;
    m_diskLabel = RECT { rx, boundsDip.top, rx + diskLblW, boundsDip.bottom };
    rx += diskLblW + indGap;

    m_powerLed   = RECT { rx, cy - ledH / 2, rx + ledW, cy + ledH / 2 };
    rx += ledW + lblGap;
    m_powerLabel = RECT { rx, boundsDip.top, rx + powerLblW, boundsDip.bottom };
}




////////////////////////////////////////////////////////////////////////////////
//
//  PartAt / TooltipTextAt
//
////////////////////////////////////////////////////////////////////////////////

Apple2cSwitchBar::Part Apple2cSwitchBar::PartAt (int x, int y) const
{
    auto  inside = [] (const RECT & r, int px, int py)
    {
        return px >= r.left && px < r.right && py >= r.top && py < r.bottom;
    };

    if (inside (m_resetRect,  x, y)) { return Part::Reset; }
    if (inside (m_eightyRect, x, y)) { return Part::EightyForty; }
    if (inside (m_kbdRect,    x, y)) { return Part::Keyboard; }

    return Part::None;
}


const wchar_t * Apple2cSwitchBar::TooltipTextAt (int x, int y) const
{
    switch (PartAt (x, y))
    {
        case Part::Reset:       return kTipReset;
        case Part::EightyForty: return kTipEighty;
        case Part::Keyboard:    return kTipKeyboard;
        default:                return nullptr;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintLabel
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::PaintLabel (IDxuiTextRenderer & text, const RECT & r, const wchar_t * s, float fontPx)
{
    HRESULT  hr = text.DrawString (s,
                                   (float) r.left, (float) r.top,
                                   (float) (r.right - r.left) + 4.0f,
                                   (float) (r.bottom - r.top),
                                   kLabel, fontPx, kFontFamily,
                                   DxuiTextHAlign::Left,
                                   DxuiTextVAlign::CenterOnCapHeight,
                                   DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintResetButton
//
//  A raised cream cap. Dormant (no Ctrl) it paints with a muted label so the
//  user reads that it needs a modifier; armed (Ctrl held) the label darkens.
//  While pressed the bevel inverts and the cap sinks a pixel.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::PaintResetButton (IDxuiPainter & p, IDxuiTextRenderer & text, const RECT & r)
{
    float  x  = (float) r.left;
    float  y  = (float) r.top;
    float  w  = (float) (r.right - r.left);
    float  h  = (float) (r.bottom - r.top);
    bool   dn = (m_pressedPart == Part::Reset);


    if (dn)
    {
        p.FillGradientRect (x, y, w, h, kCapLo, kCapHi);
        p.FillRect         (x, y, w, 1.5f, 0x40000000);           // inner top shadow
    }
    else
    {
        p.FillGradientRect (x, y, w, h, kCapHi, kCapLo);
        p.FillRect         (x + 1.0f, y + 1.0f, w - 2.0f, 1.0f, kCapHi);   // top highlight
        p.FillRect         (x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f, kCapLo); // bottom shade
    }

    p.OutlineRect (x, y, w, h, 1.0f, kCapEdge);

    if (m_hovered && m_hoverPart == Part::Reset)
    {
        p.FillRect (x, y, w, h, kHoverWash);
    }

    HRESULT  hr = text.DrawString (kLabelReset,
                                   x, y + (dn ? 1.0f : 0.0f), w, h,
                                   m_resetArmed ? kCapText : kCapTextOff,
                                   kFontDip * (float) m_dpi / 96.0f, kFontFamily,
                                   DxuiTextHAlign::Center,
                                   DxuiTextVAlign::CenterOnCapHeight,
                                   DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintKey
//
//  A latching case key in its recessed well. Out = a proud, highlit cap sitting
//  at the top of the well with the dark slot showing below it. In = the cap sunk
//  to the bottom, its face darkened, with an inner shadow cast across the slot
//  above it — the unambiguous "clicked down and staying" state.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::PaintKey (IDxuiPainter & p, const RECT & keyRect, bool pressedIn, bool hovered)
{
    float  wx   = (float) keyRect.left;
    float  wy   = (float) keyRect.top;
    float  ww   = (float) (keyRect.right - keyRect.left);
    float  wh   = (float) (keyRect.bottom - keyRect.top);
    float  sink = wh * 0.28f;                    // travel between out and in


    // Recessed well the key rides in.
    p.FillRect    (wx, wy, ww, wh, kWell);
    p.OutlineRect (wx, wy, ww, wh, 1.0f, kWellRim);

    if (pressedIn)
    {
        // Sunk cap: lower, flat and dark, with a shadow cast over the slot above.
        p.FillRect         (wx + 1.0f, wy + 1.0f, ww - 2.0f, sink, kKeyShadow);
        p.FillGradientRect (wx + 1.0f, wy + sink, ww - 2.0f, wh - sink - 1.0f, kKeyFaceIn, kKeyLo);
        p.FillRect         (wx + 1.0f, wy + sink, ww - 2.0f, 1.0f, kKeyShadow);   // lip shadow
    }
    else
    {
        // Proud cap: raised toward the top, highlit, slot shows beneath it.
        p.FillGradientRect (wx + 1.0f, wy + 1.0f, ww - 2.0f, wh - sink - 1.0f, kKeyHi, kKeyLo);
        p.FillRect         (wx + 1.0f, wy + 1.0f, ww - 2.0f, 1.0f, kKeyHi);       // top highlight
        p.FillRect         (wx + 1.0f, wy + wh - sink - 1.0f, ww - 2.0f, 1.0f, kWellRim); // cap foot
        p.FillRect         (wx + 1.0f, wy + wh - sink, ww - 2.0f, sink - 1.0f, kWell);    // exposed slot
    }

    // Diagonal molding notch near the cap top, echoing the case switches.
    p.DrawLineApprox (wx + ww * 0.28f, wy + (pressedIn ? sink : 0.0f) + wh * 0.16f,
                      wx + ww * 0.72f, wy + (pressedIn ? sink : 0.0f) + wh * 0.30f,
                      1.0f, kKeyLo);

    if (hovered)
    {
        p.FillRect (wx, wy, ww, wh, kHoverWash);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintLed
//
//  A thin vertical indicator bar. Lit LEDs carry a soft green glow; idle LEDs
//  read as a dark recessed lamp.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::PaintLed (IDxuiPainter & p, const RECT & r, bool lit)
{
    float  x  = (float) r.left;
    float  y  = (float) r.top;
    float  w  = (float) (r.right - r.left);
    float  h  = (float) (r.bottom - r.top);
    float  cx = x + w * 0.5f;
    float  cy = y + h * 0.5f;


    if (lit)
    {
        // Glow halo: a few translucent discs on a quadratic falloff.
        constexpr int  kRings = 6;
        for (int ring = kRings; ring >= 1; ring--)
        {
            float     t = (float) ring / (float) (kRings + 1);
            uint32_t  a = (uint32_t) (110.0f * (1.0f - t) * (1.0f - t) + 0.5f);
            p.FillCircleApprox (cx, cy, (w * 0.5f) + (h * 0.55f) * t,
                                (a << 24) | (kLedGreenGlow & 0x00FFFFFF));
        }
    }

    p.OutlineRect (x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, 1.0f, kLedRim);
    p.FillRect    (x, y, w, h, lit ? kLedGreen : kLedOff);

    if (lit)
    {
        p.FillRect (x + 1.0f, y + 1.0f, w * 0.4f, h * 0.4f, 0x66FFFFFF);   // specular
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    (void) theme;

    if (m_bounds.right <= m_bounds.left)
    {
        return;                       // hidden / collapsed
    }

    float  x      = (float) m_bounds.left;
    float  y      = (float) m_bounds.top;
    float  w      = (float) (m_bounds.right - m_bounds.left);
    float  h      = (float) (m_bounds.bottom - m_bounds.top);
    float  fontPx = kFontDip * (float) m_dpi / 96.0f;


    // Case body: a subtle top-lit platinum panel with molded top/bottom edges.
    painter.FillGradientRect (x, y, w, h, kCaseHi, kCase);
    painter.FillRect         (x, y, w, 1.0f, kCaseHi);              // top catchlight
    painter.FillRect         (x, y + h - 1.0f, w, 1.0f, kCaseLo);   // bottom shade
    painter.FillRect         (x, y + h - 1.0f, w, 1.0f, kCaseEdge); // seam to the drive bar

    PaintResetButton (painter, text, m_resetRect);

    PaintKey (painter, m_eightyKey, m_eightyFortyIn,
              m_hovered && m_hoverPart == Part::EightyForty);
    PaintKey (painter, m_kbdKey, m_keyboardIn,
              m_hovered && m_hoverPart == Part::Keyboard);

    PaintLabel (text, m_eightyLabel, kLabelEighty,   fontPx);
    PaintLabel (text, m_kbdLabel,    kLabelKeyboard, fontPx);

    PaintLed   (painter, m_diskLed, m_diskActive);
    PaintLabel (text, m_diskLabel, kLabelDiskUse, fontPx);

    PaintLed   (painter, m_powerLed, m_powerOn);
    PaintLabel (text, m_powerLabel, kLabelPower, fontPx);
}
