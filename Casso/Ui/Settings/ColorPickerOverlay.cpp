#include "Pch.h"

#include "ColorPickerOverlay.h"

#include "../../UnicodeSymbols.h"




static constexpr int    s_kDialogWidthDp  = 380;
static constexpr int    s_kDialogHeightDp = 286;
static constexpr int    s_kPadDp          = 18;
static constexpr int    s_kRowHeightDp    = 28;
static constexpr int    s_kRowGapDp       = 12;
static constexpr int    s_kLabelWidthDp   = 64;
static constexpr int    s_kSliderWidthDp  = 200;
static constexpr int    s_kPreviewWidthDp = 56;
static constexpr int    s_kHexWidthDp     = 110;
static constexpr int    s_kButtonWidthDp  = 96;
static constexpr int    s_kButtonGapDp    = 12;
static constexpr int    s_kCopyGapDp      = 8;
static constexpr int    s_kFocusCount     = 6;

static constexpr float  s_kHueMax         = 360.0f;
static constexpr float  s_kPercentMax     = 100.0f;




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT ColorPickerOverlay::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };

    return rc;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::Open
//
//  Shows the picker seeded with `initialArgb`. The original is retained so
//  Cancel can restore it.
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::Open (uint32_t initialArgb)
{
    m_originalArgb = 0xFF000000u | (initialArgb & 0x00FFFFFFu);
    m_argb         = m_originalArgb;
    m_focusIndex   = -1;             // nothing focused at rest (no enlarged puck)
    m_prevFocusIndex = -1;
    m_open         = true;
    m_copyHover    = false;
    m_copyFlashMs  = 0;

    ColorUtil::ArgbToHsv (m_argb, m_h, m_s, m_v);

    m_hue.SetValue (m_h);
    m_sat.SetValue (m_s * s_kPercentMax);
    m_val.SetValue (m_v * s_kPercentMax);
    m_hex.SetText  (ColorUtil::FormatHexColor (m_argb));

    ApplyFocus();
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::Layout
//
//  Centers the dialog in the panel and positions every control. Recomputed
//  on each frame the panel lays out, so DPI / resize changes are absorbed.
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::Layout (const RECT & panelRect, const DpiScaler & scaler)
{
    int  dialogW = scaler.Px (s_kDialogWidthDp);
    int  dialogH = scaler.Px (s_kDialogHeightDp);
    int  pad     = scaler.Px (s_kPadDp);
    int  rowH    = scaler.Px (s_kRowHeightDp);
    int  rowGap  = scaler.Px (s_kRowGapDp);
    int  labelW  = scaler.Px (s_kLabelWidthDp);
    int  sliderW = scaler.Px (s_kSliderWidthDp);
    int  hexW    = scaler.Px (s_kHexWidthDp);
    int  previewW = scaler.Px (s_kPreviewWidthDp);
    int  btnW    = scaler.Px (s_kButtonWidthDp);
    int  btnGap  = scaler.Px (s_kButtonGapDp);
    int  left    = panelRect.left + (panelRect.right  - panelRect.left - dialogW) / 2;
    int  top     = panelRect.top  + (panelRect.bottom - panelRect.top  - dialogH) / 2;
    int  x       = left + pad;
    int  y       = top  + pad;
    int  controlsX = x + labelW;



    m_scaler     = scaler;
    m_panelRect  = panelRect;
    m_dialogRect = MakeRect (left, top, dialogW, dialogH);

    m_title.SetRect (MakeRect (x, y, dialogW - pad * 2, rowH));
    m_title.SetText (L"Custom text color");
    m_title.SetFontSizeDip (15.0f);
    m_title.SetFontWeight (DWRITE_FONT_WEIGHT_SEMI_BOLD);
    y += rowH + rowGap;

    // The preview swatch spans the three slider rows on the right.
    m_previewRect = MakeRect (controlsX + sliderW + scaler.Px (14), y, previewW, rowH * 3 + rowGap * 2);

    m_hueLabel.SetRect (MakeRect (x, y, labelW, rowH));
    m_hueLabel.SetText (L"Hue");
    m_hue.SetRect  (MakeRect (controlsX, y, sliderW, rowH));
    m_hue.SetRange (0.0f, s_kHueMax);
    m_hue.SetStep  (1.0f);
    m_hue.SetSuffix (s_kpszDegree);
    m_hue.SetShowTicks (false);
    y += rowH + rowGap;

    m_satLabel.SetRect (MakeRect (x, y, labelW, rowH));
    m_satLabel.SetText (L"Sat");
    m_sat.SetRect  (MakeRect (controlsX, y, sliderW, rowH));
    m_sat.SetRange (0.0f, s_kPercentMax);
    m_sat.SetStep  (1.0f);
    m_sat.SetSuffix (L"%");
    m_sat.SetShowTicks (false);
    y += rowH + rowGap;

    m_valLabel.SetRect (MakeRect (x, y, labelW, rowH));
    m_valLabel.SetText (L"Val");
    m_val.SetRect  (MakeRect (controlsX, y, sliderW, rowH));
    m_val.SetRange (0.0f, s_kPercentMax);
    m_val.SetStep  (1.0f);
    m_val.SetSuffix (L"%");
    m_val.SetShowTicks (false);
    y += rowH + rowGap;

    m_hexLabel.SetRect (MakeRect (x, y, labelW, rowH));
    m_hexLabel.SetText (L"Hex");
    m_hex.SetRect      (MakeRect (controlsX, y, hexW, rowH));
    m_hex.SetTheme     (m_theme);
    m_hex.SetMaxLength (7);

    // Copy-to-clipboard icon button, immediately right of the hex box.
    {
        int  copyGap = scaler.Px (s_kCopyGapDp);

        m_copyRect = MakeRect (controlsX + hexW + copyGap, y, rowH, rowH);
    }

    // Buttons pinned to the bottom-right of the dialog.
    {
        int  by = top + dialogH - pad - rowH;
        int  bx = left + dialogW - pad - btnW;

        m_cancel.Layout   (MakeRect (bx, by, btnW, rowH));
        m_cancel.SetLabel (L"Cancel");
        m_ok.Layout       (MakeRect (bx - btnGap - btnW, by, btnW, rowH));
        m_ok.SetLabel     (L"OK");
    }

    {
        UINT  dpi = scaler.Dpi();

        m_title.SetDpi    (dpi);
        m_hueLabel.SetDpi (dpi);
        m_satLabel.SetDpi (dpi);
        m_valLabel.SetDpi (dpi);
        m_hexLabel.SetDpi (dpi);
        m_hue.SetDpi      (dpi);
        m_sat.SetDpi      (dpi);
        m_val.SetDpi      (dpi);
        m_hex.SetDpi      (dpi);
        m_ok.SetDpi       (dpi);
        m_cancel.SetDpi   (dpi);
    }

    m_hue.SetOnChange ([this] (float v) { m_h = v;                 SyncFromHsv(); });
    m_sat.SetOnChange ([this] (float v) { m_s = v / s_kPercentMax; SyncFromHsv(); });
    m_val.SetOnChange ([this] (float v) { m_v = v / s_kPercentMax; SyncFromHsv(); });
    m_hex.SetOnChange ([this] (const std::wstring &) { SyncFromHex(); });
    m_ok.SetClick     ([this] { Accept(); });
    m_cancel.SetClick ([this] { Cancel(); });
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::SyncFromHsv
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::SyncFromHsv ()
{
    HRESULT  hr = S_OK;

    BAIL_OUT_IF (m_syncing, S_OK);

    m_syncing = true;
    m_argb    = ColorUtil::HsvToArgb (m_h, m_s, m_v);
    m_hex.SetText (ColorUtil::FormatHexColor (m_argb));
    m_syncing = false;

    if (m_onChange)
    {
        m_onChange (m_argb);
    }

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::SyncFromHex
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::SyncFromHex ()
{
    HRESULT   hr     = S_OK;
    uint32_t  parsed = 0;
    bool      ok     = false;

    BAIL_OUT_IF (m_syncing, S_OK);

    ok = ColorUtil::ParseHexColor (m_hex.Text(), parsed);
    BAIL_OUT_IF (!ok, S_OK);

    m_syncing = true;
    m_argb    = parsed;
    ColorUtil::ArgbToHsv (m_argb, m_h, m_s, m_v);
    m_hue.SetValue (m_h);
    m_sat.SetValue (m_s * s_kPercentMax);
    m_val.SetValue (m_v * s_kPercentMax);
    m_syncing = false;

    if (m_onChange)
    {
        m_onChange (m_argb);
    }

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::Accept / Cancel
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::Accept ()
{
    m_open = false;

    if (m_onClose)
    {
        m_onClose (true, m_argb);
    }
}




void ColorPickerOverlay::Cancel ()
{
    m_open = false;

    if (m_onClose)
    {
        m_onClose (false, m_originalArgb);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::MoveFocus / ApplyFocus
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::MoveFocus (int delta)
{
    m_focusIndex = ((m_focusIndex + delta) % s_kFocusCount + s_kFocusCount) % s_kFocusCount;
    ApplyFocus();
}




void ColorPickerOverlay::ApplyFocus ()
{
    bool  hexGainingFocus = (m_focusIndex == 3) && (m_prevFocusIndex != 3);

    m_hue.SetFocused    (m_focusIndex == 0);
    m_sat.SetFocused    (m_focusIndex == 1);
    m_val.SetFocused    (m_focusIndex == 2);
    m_hex.SetFocused    (m_focusIndex == 3);
    m_ok.SetFocused     (m_focusIndex == 4);
    m_cancel.SetFocused (m_focusIndex == 5);

    // Select the whole hex string when the field gains focus so the first
    // keystroke replaces it (it starts full at "#RRGGBB", so appending
    // would otherwise be a no-op).
    if (hexGainingFocus)
    {
        m_hex.SelectAll();
    }

    m_prevFocusIndex = m_focusIndex;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::OnLButtonDown (int x, int y)
{
    if (m_hue.OnLButtonDown (x, y))
    {
        m_focusIndex = 0;
        ApplyFocus();
    }
    else if (m_sat.OnLButtonDown (x, y))
    {
        m_focusIndex = 1;
        ApplyFocus();
    }
    else if (m_val.OnLButtonDown (x, y))
    {
        m_focusIndex = 2;
        ApplyFocus();
    }
    else if (m_hex.OnLButtonDown (x, y))
    {
        // A click on the hex field places its own caret (handled inside
        // TextInput); suppress the focus-gain select-all so the click point
        // is preserved (Tab-in still selects all).
        m_focusIndex     = 3;
        m_prevFocusIndex = 3;
        ApplyFocus();
    }
    else if (m_ok.HitTest (x, y))
    {
        m_focusIndex = 4;
        ApplyFocus();
        m_ok.SetMouse (x, y, true);
    }
    else if (m_cancel.HitTest (x, y))
    {
        m_focusIndex = 5;
        ApplyFocus();
        m_cancel.SetMouse (x, y, true);
    }
    else if (CopyHit (x, y))
    {
        m_hex.CopyAllToClipboard();
        m_copyFlashMs = (int64_t) GetTickCount64();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::OnLButtonUp (int x, int y)
{
    (void) m_hue.OnLButtonUp (x, y);
    (void) m_sat.OnLButtonUp (x, y);
    (void) m_val.OnLButtonUp (x, y);
    (void) m_hex.OnLButtonUp (x, y);

    if (m_ok.HitTest (x, y))
    {
        m_ok.Click();
    }
    else if (m_cancel.HitTest (x, y))
    {
        m_cancel.Click();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::OnMouseMove / OnMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::OnMouseMove (int x, int y)
{
    (void) m_hue.OnMouseMove (x, y);
    (void) m_sat.OnMouseMove (x, y);
    (void) m_val.OnMouseMove (x, y);
    m_hex.OnMouseMove (x, y);
    m_copyHover = CopyHit (x, y);
}




void ColorPickerOverlay::OnMouseHover (int x, int y)
{
    m_hue.SetMouseHover (x, y);
    m_sat.SetMouseHover (x, y);
    m_val.SetMouseHover (x, y);
    m_hex.SetMouseHover (x, y);
    m_ok.SetMouse       (x, y, false);
    m_cancel.SetMouse   (x, y, false);
    m_copyHover = CopyHit (x, y);
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::OnKey
//
//  Enter accepts, Esc cancels, Tab / Shift+Tab cycles focus; otherwise the
//  key routes to the focused control. Always returns true: while open the
//  picker is modal and swallows every key.
//
////////////////////////////////////////////////////////////////////////////////

bool ColorPickerOverlay::OnKey (WPARAM vk)
{
    if (vk == VK_ESCAPE)
    {
        Cancel();
    }
    else if (vk == VK_RETURN)
    {
        Accept();
    }
    else if (vk == VK_TAB)
    {
        MoveFocus ((GetKeyState (VK_SHIFT) & 0x8000) ? -1 : 1);
    }
    else
    {
        switch (m_focusIndex)
        {
            case 0:  (void) m_hue.OnKey (vk);    break;
            case 1:  (void) m_sat.OnKey (vk);    break;
            case 2:  (void) m_val.OnKey (vk);    break;
            case 3:  (void) m_hex.OnKey (vk);    break;
            case 4:  (void) m_ok.OnKey (vk);     break;
            case 5:  (void) m_cancel.OnKey (vk); break;
            default: break;
        }
    }

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::OnChar
//
////////////////////////////////////////////////////////////////////////////////

bool ColorPickerOverlay::OnChar (wchar_t ch)
{
    if (m_focusIndex == 3)
    {
        (void) m_hex.OnChar (ch);
    }

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::Paint
//
//  Dims the panel, draws the themed dialog, then the controls + preview.
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::Paint (DxUiPainter & painter, DwriteTextRenderer & text)
{
    HRESULT      hr      = S_OK;
    ChromeTheme  theme   = (m_theme != nullptr) ? *m_theme : ChromeTheme::Skeuomorphic();
    float        borderPx = m_scaler.Pxf (1.0f);
    float        dl      = (float) m_dialogRect.left;
    float        dt      = (float) m_dialogRect.top;
    float        dw      = (float) (m_dialogRect.right  - m_dialogRect.left);
    float        dh      = (float) (m_dialogRect.bottom - m_dialogRect.top);
    float        pl      = (float) m_previewRect.left;
    float        pt      = (float) m_previewRect.top;
    float        pw      = (float) (m_previewRect.right  - m_previewRect.left);
    float        ph      = (float) (m_previewRect.bottom - m_previewRect.top);



    BAIL_OUT_IF (!m_open, S_OK);

    // The backdrop is already blurred + dimmed by the settings compose
    // pass; draw only the opaque dialog body + border here.
    painter.FillRect    (dl, dt, dw, dh, theme.navStripArgb);
    painter.OutlineRect (dl, dt, dw, dh, borderPx, theme.buttonBorderArgb);

    m_title.SetColorArgb (theme.navItemTextArgb);
    m_hueLabel.SetColorArgb (theme.navItemTextArgb);
    m_satLabel.SetColorArgb (theme.navItemTextArgb);
    m_valLabel.SetColorArgb (theme.navItemTextArgb);
    m_hexLabel.SetColorArgb (theme.navItemTextArgb);

    m_title.Paint    (painter, text);
    m_hueLabel.Paint (painter, text);
    m_satLabel.Paint (painter, text);
    m_valLabel.Paint (painter, text);
    m_hexLabel.Paint (painter, text);

    m_hue.Paint (painter, text, theme);
    m_sat.Paint (painter, text, theme);
    m_val.Paint (painter, text, theme);
    m_hex.Paint (painter, text);

    PaintCopyIcon (painter, text, theme);

    // Preview swatch of the current color.
    painter.FillRect    (pl, pt, pw, ph, m_argb);
    painter.OutlineRect (pl, pt, pw, ph, borderPx, theme.buttonBorderArgb);

    m_ok.Paint     (painter, text, theme);
    m_cancel.Paint (painter, text, theme);

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::PaintCopyIcon
//
//  Draws the copy-to-clipboard glyph next to the hex box. The glyph swaps
//  to a checkmark for a short window after a successful copy so the click
//  has visible confirmation; the picker repaints continuously so the flash
//  clears on its own.
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::PaintCopyIcon (
    DxUiPainter         & painter,
    DwriteTextRenderer  & text,
    const ChromeTheme   & theme)
{
    constexpr wchar_t  s_kpszMdl2Family[] = L"Segoe MDL2 Assets";
    constexpr int64_t  s_kFlashMs         = 1100;

    HRESULT          hr        = S_OK;
    float            cl        = (float) m_copyRect.left;
    float            ct        = (float) m_copyRect.top;
    float            cw        = (float) (m_copyRect.right  - m_copyRect.left);
    float            ch        = (float) (m_copyRect.bottom - m_copyRect.top);
    float            glyphDip  = m_scaler.Pxf (14.0f);
    bool             flashing  = (m_copyFlashMs != 0) &&
                                 ((int64_t) GetTickCount64() - m_copyFlashMs < s_kFlashMs);
    const wchar_t  * glyph     = flashing ? s_kpszMdl2Accept : s_kpszMdl2Copy;
    uint32_t         glyphArgb = theme.navItemTextArgb;



    if (flashing || m_copyHover)
    {
        glyphArgb = theme.linkArgb;
    }

    if (m_copyHover)
    {
        painter.FillRect (cl, ct, cw, ch, theme.navHoverArgb);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (glyph,
                                              cl,
                                              ct,
                                              cw,
                                              ch,
                                              glyphArgb,
                                              glyphDip,
                                              s_kpszMdl2Family,
                                              DwriteTextRenderer::HAlign::Center,
                                              DwriteTextRenderer::VAlign::Center,
                                              DWRITE_FONT_WEIGHT_NORMAL,
                                              false));
}




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::CopyHit
//
////////////////////////////////////////////////////////////////////////////////

bool ColorPickerOverlay::CopyHit (int x, int y) const
{
    return x >= m_copyRect.left && x < m_copyRect.right &&
           y >= m_copyRect.top  && y < m_copyRect.bottom;
}
