#include "Pch.h"

#include "JoystickToggleButton.h"




static constexpr int      s_kBaseDpi      = 96;
static constexpr int      s_kPadXDp       = 12;
static constexpr int      s_kPadYDp       = 6;
static constexpr int      s_kLedGapDp     = 8;
static constexpr int      s_kLedCorePx    = 7;
static constexpr float    s_kFontDip      = 13.0f;
static constexpr float    s_kFallbackCharPx = 7.5f;
static constexpr wchar_t  s_kFontFamily[] = L"Segoe UI";

static constexpr wchar_t  s_kLabelJoystick[] = L"Joystick Mode";
static constexpr wchar_t  s_kLabelPaddle[]   = L"Paddle Mode (ESC to exit)";

// The LED glows a fixed realistic blue regardless of the active theme's
// drive LED color; a dark near-black core stands in for the unlit (Off)
// state. The LED is lit for both Joystick and Paddle and dark for Off.
static constexpr uint32_t s_kLedOnCoreArgb   = 0xFF3DA1FF;
static constexpr uint32_t s_kLedOnHaloArgb   = 0xA03DA1FF;
static constexpr uint32_t s_kLedOffCoreArgb  = 0xFF06121A;

static constexpr wchar_t  s_kTooltip[] =
    L"Click to cycle input mode: Off, Joystick (arrows + Z / X), then "
    L"Paddle (mouse).  In Paddle mode the mouse is captured and ~4 inches "
    L"of motion sweeps the paddle full range, holding position; the left "
    L"and right mouse buttons are the joystick buttons.  Press Esc to "
    L"release the mouse, or Ctrl+J to cycle out of Paddle mode.";




////////////////////////////////////////////////////////////////////////////////
//
//  LabelFor
//
//  Returns the static label string for an input mapping mode.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * JoystickToggleButton::LabelFor (InputMappingMode mode)
{
    switch (mode)
    {
        case InputMappingMode::Paddle:
            return s_kLabelPaddle;

        case InputMappingMode::Off:
        case InputMappingMode::Joystick:
        default:
            return s_kLabelJoystick;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Centers the button on (centerXPx, centerYPx), sizing the frame to the
//  CURRENT mode's label (measured live when a text renderer is available,
//  estimated otherwise) plus the LED and internal padding. The frame grows
//  for Paddle mode's longer "(ESC to exit)" label and shrinks back for Off
//  / Joystick, so the owner must relayout on a mode change. The LED is
//  positioned vertically centered against the left padding.
//
////////////////////////////////////////////////////////////////////////////////

void JoystickToggleButton::Layout (int centerXPx, int centerYPx, UINT dpi, DwriteTextRenderer * pText)
{
    const wchar_t * label    = LabelFor (m_mode);
    UINT   eDpi     = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;
    int    padX     = MulDiv (s_kPadXDp,   (int) eDpi, s_kBaseDpi);
    int    padY     = MulDiv (s_kPadYDp,   (int) eDpi, s_kBaseDpi);
    int    ledGap   = MulDiv (s_kLedGapDp, (int) eDpi, s_kBaseDpi);
    int    ledCore  = MulDiv (s_kLedCorePx, (int) eDpi, s_kBaseDpi);
    float  fontDip  = s_kFontDip * (float) eDpi / (float) s_kBaseDpi;
    float  textW    = 0.0f;
    float  textH    = 0.0f;
    int    contentW = 0;
    int    contentH = 0;
    int    width    = 0;
    int    height   = 0;
    int    left     = 0;
    int    top      = 0;
    int    ledY     = 0;



    m_dpi = eDpi;

    if (ledCore < s_kLedCorePx)
    {
        ledCore = s_kLedCorePx;
    }

    if (pText != nullptr)
    {
        HRESULT  hrM = pText->MeasureString (label, fontDip, s_kFontFamily, textW, textH);

        if (FAILED (hrM))
        {
            textW = 0.0f;
            textH = 0.0f;
        }
    }

    // Before the text renderer's draw target exists, MeasureString can't
    // run; fall back to a rough Segoe UI advance estimate so the button
    // still reserves a sane width on the very first layout.
    if (textW <= 0.0f)
    {
        textW = (float) lstrlenW (label) * s_kFallbackCharPx * (float) eDpi / (float) s_kBaseDpi;
    }

    if (textH <= 0.0f)
    {
        textH = fontDip;
    }

    contentW = ledCore + ledGap + (int) (textW + 0.5f);
    contentH = std::max (ledCore, (int) (textH + 0.5f));
    width    = padX * 2 + contentW;
    height   = padY * 2 + contentH;
    left     = centerXPx - width / 2;
    top      = centerYPx - height / 2;

    m_bounds.left   = left;
    m_bounds.top    = top;
    m_bounds.right  = left + width;
    m_bounds.bottom = top + height;

    ledY = top + (height - ledCore) / 2;
    m_led.Layout (left + padX, ledY, eDpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool JoystickToggleButton::HitTest (int x, int y) const
{
    return x >= m_bounds.left && x < m_bounds.right &&
           y >= m_bounds.top  && y < m_bounds.bottom;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Draws the frame only when hovered, focused, or pressed; a focus ring
//  is inset one step inside the border. The LED glows per mode (dark in
//  Off, blue in Joystick, amber in Paddle), then the active mode's label
//  is drawn cap-height centered to the right of it.
//
////////////////////////////////////////////////////////////////////////////////

void JoystickToggleButton::Paint (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme) const
{
    HRESULT             hr       = S_OK;
    bool                active   = m_hovered || m_focused || m_pressed;
    float               fontDip  = s_kFontDip * (float) m_dpi / (float) s_kBaseDpi;
    float               bl       = (float) m_bounds.left;
    float               bt       = (float) m_bounds.top;
    float               bw       = (float) (m_bounds.right  - m_bounds.left);
    float               bh       = (float) (m_bounds.bottom - m_bounds.top);
    uint32_t            coreArgb = (m_mode == InputMappingMode::Off) ? s_kLedOffCoreArgb : s_kLedOnCoreArgb;
    uint32_t            haloArgb = (m_mode == InputMappingMode::Off) ? 0                : s_kLedOnHaloArgb;
    LedIndicatorLayout  ledRect  = m_led.GetLayout();
    int                 ledGap   = MulDiv (s_kLedGapDp, (int) m_dpi, s_kBaseDpi);
    float               textX    = (float) (ledRect.coreRect.right + ledGap);



    if (bw <= 0.0f || bh <= 0.0f)
    {
        return;
    }

    if (active)
    {
        uint32_t  fill = m_pressed ? theme.buttonPressedArgb
                                   : (m_hovered ? theme.buttonHoverArgb : theme.buttonIdleArgb);

        painter.FillRect    (bl, bt, bw, bh, fill);
        painter.OutlineRect (bl, bt, bw, bh, 1.0f, theme.buttonBorderArgb);

        if (m_focused)
        {
            float  inset = (float) MulDiv (2, (int) m_dpi, s_kBaseDpi);

            painter.OutlineRect (bl + inset,
                                 bt + inset,
                                 bw - inset * 2.0f,
                                 bh - inset * 2.0f,
                                 1.0f,
                                 theme.linkArgb);
        }
    }

    m_led.Paint (painter, coreArgb, haloArgb);

    hr = text.DrawString (LabelFor (m_mode),
                          textX,
                          bt,
                          (float) m_bounds.right - textX,
                          bh,
                          theme.navItemTextArgb,
                          fontDip,
                          s_kFontFamily,
                          DwriteTextRenderer::HAlign::Left,
                          DwriteTextRenderer::VAlign::CenterOnCapHeight);
    IGNORE_RETURN_VALUE (hr, S_OK);
}




////////////////////////////////////////////////////////////////////////////////
//
//  TooltipText
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * JoystickToggleButton::TooltipText ()
{
    return s_kTooltip;
}
