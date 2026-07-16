#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "Apple2cSwitchBar.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Slanted-fill helpers (file-local)
//
//  Every piece of a switch cap shares one shear field so it reads as a single
//  leaning parallelogram: a point at absolute height y is pushed right by
//  (refBottom - y) * tan, i.e. no shift at the reference bottom, growing toward
//  the top. ShearFill draws one solid parallelogram strip; ShearGrad stacks
//  strips with an interpolated colour for a top-lit gradient on the slant.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    uint32_t LerpArgb (uint32_t a, uint32_t b, float t)
    {
        auto  chan = [] (uint32_t c, int shift) { return (int) ((c >> shift) & 0xFFu); };
        auto  mix  = [&] (int shift)
        {
            int  v = chan (a, shift) + (int) ((chan (b, shift) - chan (a, shift)) * t + 0.5f);
            return (uint32_t) (v < 0 ? 0 : (v > 255 ? 255 : v));
        };

        return (mix (24) << 24) | (mix (16) << 16) | (mix (8) << 8) | mix (0);
    }

    void ShearFill (IDxuiPainter & p, float xL, float yTop, float w, float h,
                    float tan, float refBottom, uint32_t argb)
    {
        float  st = (refBottom - yTop)       * tan;   // top-edge shift
        float  sb = (refBottom - (yTop + h)) * tan;   // bottom-edge shift

        p.FillConvexQuad (xL + st,     yTop,
                          xL + w + st, yTop,
                          xL + w + sb, yTop + h,
                          xL + sb,     yTop + h,
                          argb);
    }

    void ShearGrad (IDxuiPainter & p, float xL, float yTop, float w, float h,
                    float tan, float refBottom, uint32_t top, uint32_t bot, int strips)
    {
        for (int i = 0; i < strips; i++)
        {
            float  t0 = (float) i       / (float) strips;
            float  t1 = (float) (i + 1) / (float) strips;

            ShearFill (p, xL, yTop + h * t0, w, h * (t1 - t0),
                       tan, refBottom, LerpArgb (top, bot, (t0 + t1) * 0.5f));
        }
    }

    // Horizontal companion to ShearGrad: interpolates left -> right across the
    // width by stacking full-height vertical columns (each its own thin slanted
    // sliver), so a left/right-directional shadow follows the same lean.
    void ShearGradH (IDxuiPainter & p, float xL, float yTop, float w, float h,
                     float tan, float refBottom, uint32_t left, uint32_t right, int cols)
    {
        for (int i = 0; i < cols; i++)
        {
            float  t0 = (float) i       / (float) cols;
            float  t1 = (float) (i + 1) / (float) cols;

            ShearFill (p, xL + w * t0, yTop, w * (t1 - t0), h,
                       tan, refBottom, LerpArgb (left, right, (t0 + t1) * 0.5f));
        }
    }
}




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

    // Slanted caps lean right, so their bounding box is wider than the cap body
    // by the top-edge overhang. Budget it into each hit rect so the parallelogram
    // top-right corner never rides over the next element or its label.
    int    slantKey   = (int) (keyH   * kSlantTan + 0.5f);
    int    slantReset = (int) (resetH * kSlantTan + 0.5f);


    m_dpi    = eDpi;
    m_bounds = boundsDip;

    // Left group: [reset] [80/40 key] "80/40" [keyboard key] "keyboard".
    int  x = boundsDip.left + edge;

    m_resetRect = RECT { x, cy - resetH / 2, x + resetW + slantReset, cy + resetH / 2 };
    x += resetW + slantReset + gGap;

    int  eightyLblW = (int) (MeasureLabel (kLabelEighty, fontPx) + 0.5f);
    m_eightyKey   = RECT { x, cy - keyH / 2, x + keyW + slantKey, cy + keyH / 2 };
    m_eightyLabel = RECT { m_eightyKey.right + lblGap, boundsDip.top, m_eightyKey.right + lblGap + eightyLblW, boundsDip.bottom };
    m_eightyRect  = RECT { m_eightyKey.left, m_eightyKey.top, m_eightyLabel.right, m_eightyKey.bottom };
    x = m_eightyLabel.right + swGap;

    int  kbdLblW = (int) (MeasureLabel (kLabelKeyboard, fontPx) + 0.5f);
    m_kbdKey   = RECT { x, cy - keyH / 2, x + keyW + slantKey, cy + keyH / 2 };
    m_kbdLabel = RECT { m_kbdKey.right + lblGap, boundsDip.top, m_kbdKey.right + lblGap + kbdLblW, boundsDip.bottom };
    m_kbdRect  = RECT { m_kbdKey.left, m_kbdKey.top, m_kbdLabel.right, m_kbdKey.bottom };

    // Right group: [disk-use LED] "disk use"  [power LED] "power", right-anchored.
    // The LEDs lean the same way as the switches, so budget their overhang too.
    int  slantLed  = (int) (ledH * kSlantTan + 0.5f);
    int  ledBoxW   = ledW + slantLed;
    int  diskLblW  = (int) (MeasureLabel (kLabelDiskUse, fontPx) + 0.5f);
    int  powerLblW = (int) (MeasureLabel (kLabelPower,   fontPx) + 0.5f);
    int  rightW    = ledBoxW + lblGap + diskLblW + indGap + ledBoxW + lblGap + powerLblW;
    int  rx        = boundsDip.right - edge - rightW;

    m_diskLed   = RECT { rx, cy - ledH / 2, rx + ledBoxW, cy + ledH / 2 };
    rx = m_diskLed.right + lblGap;
    m_diskLabel = RECT { rx, boundsDip.top, rx + diskLblW, boundsDip.bottom };
    rx += diskLblW + indGap;

    m_powerLed   = RECT { rx, cy - ledH / 2, rx + ledBoxW, cy + ledH / 2 };
    rx = m_powerLed.right + lblGap;
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
//  A slanted cream cap that reads raised, then darkens and takes the sunk
//  top-left shadow while pressed (momentary, unlike the latching switches).
//  Dormant (no Ctrl) it paints with a muted label so the user reads that it
//  needs a modifier; armed (Ctrl held) the label darkens. A 1px label nudge
//  gives the press a tactile beat.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::PaintResetButton (IDxuiPainter & p, IDxuiTextRenderer & text, const RECT & r)
{
    bool  dn = (m_pressedPart == Part::Reset);

    PaintSlantCap (p, r, dn, m_hovered && m_hoverPart == Part::Reset,
                   dn ? kCapLo : kCapHi, dn ? kKeyLoIn : kCapLo);

    HRESULT  hr = text.DrawString (kLabelReset,
                                   (float) r.left,
                                   (float) r.top + (dn ? 1.0f : 0.0f),
                                   (float) (r.right - r.left),
                                   (float) (r.bottom - r.top),
                                   m_resetArmed ? kCapText : kCapTextOff,
                                   kFontDip * (float) m_dpi / 96.0f, kFontFamily,
                                   DxuiTextHAlign::Center,
                                   DxuiTextVAlign::CenterOnCapHeight,
                                   DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintSlantCap
//
//  The shared skeuomorphic cap: a right-leaning parallelogram lit from the
//  top-left. The cap fills its whole slot (no gap); depth is carried purely by
//  directional shading. Out (proud) it is highlit along the top and left edges
//  and shadowed toward the bottom-right, so it reads as standing above the
//  surface. In (pressed) it is darkened and shadowed from the top and left --
//  the two overlap in the top-left corner and, on a thin switch, swallow most
//  of the cap -- with only a faint catch at the bottom-right, so it reads as
//  sunk below the surface. Shadows and highlights are gradients that fade to
//  clear, never flat blocks.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::PaintSlantCap (IDxuiPainter & p, const RECT & r, bool pressedIn,
                                      bool hovered, uint32_t faceHi, uint32_t faceLo)
{
    constexpr int  kN = 10;

    float  x     = (float) r.left;
    float  y     = (float) r.top;
    float  w     = (float) (r.right - r.left);
    float  h     = (float) (r.bottom - r.top);
    float  tan   = kSlantTan;
    float  refB  = y + h;
    float  dx    = h * tan;                  // top-edge overhang
    float  bodyW = w - dx;                   // cap body width (bbox minus overhang)
    float  cx    = x + 1.0f;
    float  cy    = y + 1.0f;
    float  cw    = bodyW - 2.0f;             // cap face, inside a thin rim
    float  ch    = h - 2.0f;


    // Thin molded rim, then the cap face fills the whole footprint.
    ShearFill (p, x, y, bodyW, h, tan, refB, kSocketRim);
    ShearGrad (p, cx, cy, cw, ch, tan, refB, faceHi, faceLo, kN);

    if (!pressedIn)
    {
        // Raised: highlit top + left, shadowed bottom + right.
        ShearFill  (p, cx, cy, cw, 1.2f, tan, refB, kLitEdge);                               // top highlight
        ShearGradH (p, cx, cy, cw * 0.5f, ch, tan, refB, kLitEdge, kShadowNil, kN);          // left highlight
        ShearGrad  (p, cx, cy + ch * 0.45f, cw, ch * 0.55f, tan, refB, kShadowNil, kShadeProud, kN); // bottom shadow
        ShearGradH (p, cx + cw * 0.5f, cy, cw * 0.5f, ch, tan, refB, kShadowNil, kShadeProud, kN);    // right shadow
    }
    else
    {
        // Sunk: shadowed top + left (dominant), a faint catch bottom-right.
        ShearGrad  (p, cx, cy, cw, ch * 0.62f, tan, refB, kShadePushed, kShadowNil, kN);     // top shadow
        ShearGradH (p, cx, cy, cw * 0.62f, ch, tan, refB, kShadePushed, kShadowNil, kN);     // left shadow
        ShearFill  (p, cx, cy + ch - 1.2f, cw, 1.2f, tan, refB, kLitFoot);                   // bottom-right catch
    }

    if (hovered)
    {
        ShearFill (p, x, y, bodyW, h, tan, refB, kHoverWash);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintKey
//
//  A latching case switch: the shared slanted cap, out (proud) when the switch
//  is released and in (depressed + darkened) while it stays clicked down.
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::PaintKey (IDxuiPainter & p, const RECT & keyRect, bool pressedIn, bool hovered)
{
    PaintSlantCap (p, keyRect, pressedIn, hovered,
                   pressedIn ? kKeyFaceIn : kKeyHi,
                   pressedIn ? kKeyLoIn   : kKeyLo);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintLed
//
//  A thin indicator lamp, slanted to match the case switches. Lit LEDs carry a
//  soft green glow with a specular catch; idle LEDs read as a dark recessed
//  lamp. The glow halo stays radial (a diffuse bloom has no edge to slant).
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cSwitchBar::PaintLed (IDxuiPainter & p, const RECT & r, bool lit)
{
    float  x     = (float) r.left;
    float  y     = (float) r.top;
    float  w     = (float) (r.right - r.left);
    float  h     = (float) (r.bottom - r.top);
    float  tan   = kSlantTan;
    float  refB  = y + h;
    float  dx    = h * tan;
    float  bodyW = w - dx;                   // lamp body width (bbox minus overhang)
    float  cx    = x + w * 0.5f;             // body centre at mid-height
    float  cy    = y + h * 0.5f;


    if (lit)
    {
        // Glow halo: a few translucent discs on a quadratic falloff.
        constexpr int  kRings = 6;
        for (int ring = kRings; ring >= 1; ring--)
        {
            float     t = (float) ring / (float) (kRings + 1);
            uint32_t  a = (uint32_t) (110.0f * (1.0f - t) * (1.0f - t) + 0.5f);
            p.FillCircleApprox (cx, cy, (bodyW * 0.5f) + (h * 0.55f) * t,
                                (a << 24) | (kLedGreenGlow & 0x00FFFFFF));
        }
    }

    ShearFill (p, x,        y,        bodyW,        h,        tan, refB, kLedRim);
    ShearFill (p, x + 1.0f, y + 1.0f, bodyW - 2.0f, h - 2.0f, tan, refB, lit ? kLedGreen : kLedOff);

    if (lit)
    {
        ShearFill (p, x + 1.5f, y + 1.5f, bodyW * 0.4f, h * 0.4f, tan, refB, 0x66FFFFFF);   // specular
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
