#include "Pch.h"

#include "ColorPickerOverlay.h"

#include "../../UnicodeSymbols.h"




static constexpr int      s_kDialogWidthDp        = 380;
static constexpr int      s_kDialogHeightDp       = 286;
static constexpr int      s_kPadDp                = 18;
static constexpr int      s_kRowHeightDp          = 28;
static constexpr int      s_kRowGapDp             = 12;
static constexpr int      s_kLabelWidthDp         = 64;
static constexpr int      s_kSliderWidthDp        = 200;
static constexpr int      s_kPreviewWidthDp       = 56;
static constexpr int      s_kHexWidthDp           = 110;
static constexpr int      s_kButtonWidthDp        = 96;
static constexpr int      s_kButtonGapDp          = 12;
static constexpr int      s_kCopyGapDp            = 8;
static constexpr int      s_kPreviewGapDp         = 14;
static constexpr int      s_kHexMaxLength         = 7;
static constexpr int      s_kHalfDivisor          = 2;
static constexpr int      s_kDialogPadCount       = 2;
static constexpr int      s_kSliderRowCount       = 3;
static constexpr int      s_kSliderGapCount       = 2;
static constexpr int      s_kFocusHue             = 0;
static constexpr int      s_kFocusSat             = 1;
static constexpr int      s_kFocusVal             = 2;
static constexpr int      s_kFocusHex             = 3;
static constexpr int      s_kFocusOk              = 4;
static constexpr int      s_kFocusCancel          = 5;
static constexpr int      s_kFocusCount           = 6;
static constexpr int      s_kFocusNone            = -1;
static constexpr int      s_kKeyDownMask          = 0x8000;
static constexpr wchar_t  s_kDeleteChar           = 0x7F;
static constexpr wchar_t  s_kFirstPrintableChar   = 0x20;

static constexpr float    s_kHueMax               = 360.0f;
static constexpr float    s_kPercentMax           = 100.0f;
static constexpr float    s_kTitleFontDip         = 15.0f;
static constexpr float    s_kBorderDip            = 1.0f;
static constexpr float    s_kCopyGlyphDip         = 14.0f;

static constexpr int64_t  s_kCopyFlashMs          = 1100;
static constexpr uint32_t s_kOpaqueAlphaMask      = 0xFF000000u;
static constexpr uint32_t s_kRgbMask              = 0x00FFFFFFu;
static constexpr UINT     s_kClipboardFormat      = CF_UNICODETEXT;
static constexpr UINT     s_kClipboardAllocFlags  = GMEM_MOVEABLE;
static constexpr wchar_t  s_kpszMdl2Family[]      = L"Segoe MDL2 Assets";




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
    m_originalArgb     = s_kOpaqueAlphaMask | (initialArgb & s_kRgbMask);
    m_argb             = m_originalArgb;
    m_focusIndex       = s_kFocusNone;
    m_prevFocusIndex   = s_kFocusNone;
    m_open             = true;
    m_copyHover        = false;
    m_copyFlashMs      = 0;
    m_hexReplaceOnChar = false;



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

void ColorPickerOverlay::Layout (const RECT & panelRect, const DxuiDpiScaler & scaler)
{
    int   dialogW   = scaler.Px (s_kDialogWidthDp);
    int   dialogH   = scaler.Px (s_kDialogHeightDp);
    int   pad       = scaler.Px (s_kPadDp);
    int   rowH      = scaler.Px (s_kRowHeightDp);
    int   rowGap    = scaler.Px (s_kRowGapDp);
    int   labelW    = scaler.Px (s_kLabelWidthDp);
    int   sliderW   = scaler.Px (s_kSliderWidthDp);
    int   hexW      = scaler.Px (s_kHexWidthDp);
    int   previewW  = scaler.Px (s_kPreviewWidthDp);
    int   btnW      = scaler.Px (s_kButtonWidthDp);
    int   btnGap    = scaler.Px (s_kButtonGapDp);
    int   copyGap   = scaler.Px (s_kCopyGapDp);
    int   left      = panelRect.left + (panelRect.right  - panelRect.left - dialogW) / s_kHalfDivisor;
    int   top       = panelRect.top  + (panelRect.bottom - panelRect.top  - dialogH) / s_kHalfDivisor;
    int   x         = left + pad;
    int   y         = top  + pad;
    int   controlsX = x + labelW;
    int   by        = top + dialogH - pad - rowH;
    int   bx        = left + dialogW - pad - btnW;
    UINT  dpi       = scaler.Dpi();



    m_scaler     = scaler;
    m_panelRect  = panelRect;
    m_dialogRect = MakeRect (left, top, dialogW, dialogH);

    m_title.SetRect (MakeRect (x, y, dialogW - pad * s_kDialogPadCount, rowH));
    m_title.SetText (L"Custom text color");
    m_title.SetFontSizeDip (s_kTitleFontDip);
    m_title.SetFontWeight (DWRITE_FONT_WEIGHT_SEMI_BOLD);
    y += rowH + rowGap;

    m_previewRect = MakeRect (controlsX + sliderW + scaler.Px (s_kPreviewGapDp), y, previewW, rowH * s_kSliderRowCount + rowGap * s_kSliderGapCount);

    m_hueLabel.SetRect (MakeRect (x, y, labelW, rowH));
    m_hueLabel.SetText (L"Hue");
    m_hue.SetRect      (MakeRect (controlsX, y, sliderW, rowH));
    m_hue.SetRange     (0.0f, s_kHueMax);
    m_hue.SetStep      (1.0f);
    m_hue.SetSuffix    (s_kpszDegree);
    m_hue.SetShowTicks (false);
    y += rowH + rowGap;

    m_satLabel.SetRect (MakeRect (x, y, labelW, rowH));
    m_satLabel.SetText (L"Sat");
    m_sat.SetRect      (MakeRect (controlsX, y, sliderW, rowH));
    m_sat.SetRange     (0.0f, s_kPercentMax);
    m_sat.SetStep      (1.0f);
    m_sat.SetSuffix    (L"%");
    m_sat.SetShowTicks (false);
    y += rowH + rowGap;

    m_valLabel.SetRect (MakeRect (x, y, labelW, rowH));
    m_valLabel.SetText (L"Val");
    m_val.SetRect      (MakeRect (controlsX, y, sliderW, rowH));
    m_val.SetRange     (0.0f, s_kPercentMax);
    m_val.SetStep      (1.0f);
    m_val.SetSuffix    (L"%");
    m_val.SetShowTicks (false);
    y += rowH + rowGap;

    m_hexLabel.SetRect (MakeRect (x, y, labelW, rowH));
    m_hexLabel.SetText (L"Hex");
    m_hex.SetRect      (MakeRect (controlsX, y, hexW, rowH));
    m_hex.SetMaxLength (s_kHexMaxLength);
    m_copyRect = MakeRect (controlsX + hexW + copyGap, y, rowH, rowH);

    m_cancel.Layout   (MakeRect (bx, by, btnW, rowH));
    m_cancel.SetLabel (L"Cancel");
    m_ok.Layout       (MakeRect (bx - btnGap - btnW, by, btnW, rowH));
    m_ok.SetLabel     (L"OK");
    m_ok.SetVariant   (DxuiButton::Variant::Primary);

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

void ColorPickerOverlay::SyncFromHsv()
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

void ColorPickerOverlay::SyncFromHex()
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
//  ColorPickerOverlay::Accept
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::Accept()
{
    m_open = false;

    if (m_onClose)
    {
        m_onClose (true, m_argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::Cancel
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::Cancel()
{
    m_open = false;

    if (m_onClose)
    {
        m_onClose (false, m_originalArgb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::MoveFocus
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::MoveFocus (int delta)
{
    m_focusIndex = ((m_focusIndex + delta) % s_kFocusCount + s_kFocusCount) % s_kFocusCount;
    ApplyFocus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::ApplyFocus
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::ApplyFocus()
{
    bool  hexGainingFocus = (m_focusIndex == s_kFocusHex) && (m_prevFocusIndex != s_kFocusHex);



    m_hue.SetFocused    (m_focusIndex == s_kFocusHue);
    m_sat.SetFocused    (m_focusIndex == s_kFocusSat);
    m_val.SetFocused    (m_focusIndex == s_kFocusVal);
    m_hex.SetFocused    (m_focusIndex == s_kFocusHex);
    m_ok.SetFocused     (m_focusIndex == s_kFocusOk);
    m_cancel.SetFocused (m_focusIndex == s_kFocusCancel);

    m_hexReplaceOnChar = hexGainingFocus;
    if (m_focusIndex != s_kFocusHex)
    {
        m_hexReplaceOnChar = false;
    }

    m_prevFocusIndex = m_focusIndex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::CopyTextToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::CopyTextToClipboard (const std::wstring & text)
{
    HRESULT  hr                   = S_OK;
    HGLOBAL  hGlobal              = nullptr;
    HANDLE   hClipboardData       = nullptr;
    void   * pBuffer              = nullptr;
    size_t   byteCount            = (text.size() + 1) * sizeof (wchar_t);
    BOOL     clipboardOpened      = FALSE;
    BOOL     clipboardEmptied     = FALSE;
    BOOL     clipboardClosed      = FALSE;
    BOOL     allocationSucceeded  = FALSE;
    BOOL     bufferLocked         = FALSE;
    BOOL     clipboardTransferred = FALSE;
    BOOL     bufferUnlocked       = FALSE;



    clipboardOpened = OpenClipboard (m_hwnd);
    CWR (clipboardOpened);

    clipboardEmptied = EmptyClipboard();
    CWR (clipboardEmptied);

    hGlobal = GlobalAlloc (s_kClipboardAllocFlags, byteCount);
    allocationSucceeded = (hGlobal != nullptr);
    CWR (allocationSucceeded);

    pBuffer = GlobalLock (hGlobal);
    bufferLocked = (pBuffer != nullptr);
    CWR (bufferLocked);

    CopyMemory (pBuffer, text.c_str(), byteCount);
    bufferUnlocked = GlobalUnlock (hGlobal);
    IGNORE_RETURN_VALUE (bufferUnlocked, TRUE);

    hClipboardData = SetClipboardData (s_kClipboardFormat, hGlobal);
    clipboardTransferred = (hClipboardData != nullptr);
    CWR (clipboardTransferred);

    hGlobal = nullptr;

Error:
    if (hGlobal != nullptr)
    {
        GlobalFree (hGlobal);
    }

    if (clipboardOpened)
    {
        clipboardClosed = CloseClipboard();
        IGNORE_RETURN_VALUE (clipboardClosed, TRUE);
    }

    return;
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
        m_focusIndex = s_kFocusHue;
        ApplyFocus();
    }
    else if (m_sat.OnLButtonDown (x, y))
    {
        m_focusIndex = s_kFocusSat;
        ApplyFocus();
    }
    else if (m_val.OnLButtonDown (x, y))
    {
        m_focusIndex = s_kFocusVal;
        ApplyFocus();
    }
    else if (m_hex.OnLButtonDown (x, y))
    {
        m_focusIndex       = s_kFocusHex;
        m_prevFocusIndex   = s_kFocusHex;
        m_hexReplaceOnChar = false;
        ApplyFocus();
    }
    else if (m_ok.HitTest (x, y))
    {
        m_focusIndex = s_kFocusOk;
        ApplyFocus();
        m_ok.SetMouse (x, y, true);
    }
    else if (m_cancel.HitTest (x, y))
    {
        m_focusIndex = s_kFocusCancel;
        ApplyFocus();
        m_cancel.SetMouse (x, y, true);
    }
    else if (CopyHit (x, y))
    {
        CopyTextToClipboard (m_hex.Text());
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
//  ColorPickerOverlay::OnMouseMove
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





////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::OnMouseHover
//
////////////////////////////////////////////////////////////////////////////////

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
//  key routes to the focused control. The picker is modal and swallows keys.
//
////////////////////////////////////////////////////////////////////////////////

bool ColorPickerOverlay::OnKey (WPARAM vk)
{
    bool  ctrlDown = (GetKeyState (VK_CONTROL) & s_kKeyDownMask) != 0;



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
        MoveFocus ((GetKeyState (VK_SHIFT) & s_kKeyDownMask) ? -1 : 1);
    }
    else
    {
        if (m_focusIndex == s_kFocusHex && m_hexReplaceOnChar &&
            (vk == VK_BACK || vk == VK_DELETE || (ctrlDown && vk == L'V')))
        {
            m_hex.SetText (L"");
            m_hexReplaceOnChar = false;
        }

        switch (m_focusIndex)
        {
            case s_kFocusHue:    (void) m_hue.OnKey (vk);    break;
            case s_kFocusSat:    (void) m_sat.OnKey (vk);    break;
            case s_kFocusVal:    (void) m_val.OnKey (vk);    break;
            case s_kFocusHex:    (void) m_hex.OnKey (vk);    break;
            case s_kFocusOk:     (void) m_ok.OnKey (vk);     break;
            case s_kFocusCancel: (void) m_cancel.OnKey (vk); break;
            default:             break;
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
    if (m_focusIndex == s_kFocusHex)
    {
        if (m_hexReplaceOnChar && ch >= s_kFirstPrintableChar && ch != s_kDeleteChar)
        {
            m_hex.SetText (L"");
            m_hexReplaceOnChar = false;
        }
        (void) m_hex.OnChar (ch);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay::Paint
//
//  Draws the themed dialog, then the controls and preview swatch.
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr       = S_OK;
    float    borderPx = m_scaler.Pxf (s_kBorderDip);
    float    dl       = (float) m_dialogRect.left;
    float    dt       = (float) m_dialogRect.top;
    float    dw       = (float) (m_dialogRect.right  - m_dialogRect.left);
    float    dh       = (float) (m_dialogRect.bottom - m_dialogRect.top);
    float    pl       = (float) m_previewRect.left;
    float    pt       = (float) m_previewRect.top;
    float    pw       = (float) (m_previewRect.right  - m_previewRect.left);
    float    ph       = (float) (m_previewRect.bottom - m_previewRect.top);



    BAIL_OUT_IF (!m_open, S_OK);

    painter.FillRect    (dl, dt, dw, dh, theme.BackgroundElevated());
    painter.OutlineRect (dl, dt, dw, dh, borderPx, theme.ButtonBorder());

    m_title.SetColorArgb    (theme.HeadingForeground());
    m_hueLabel.SetColorArgb (theme.Foreground());
    m_satLabel.SetColorArgb (theme.Foreground());
    m_valLabel.SetColorArgb (theme.Foreground());
    m_hexLabel.SetColorArgb (theme.Foreground());

    m_title.Paint    (painter, text);
    m_hueLabel.Paint (painter, text);
    m_satLabel.Paint (painter, text);
    m_valLabel.Paint (painter, text);
    m_hexLabel.Paint (painter, text);

    m_hue.Paint (painter, text, theme);
    m_sat.Paint (painter, text, theme);
    m_val.Paint (painter, text, theme);
    m_hex.SetTheme (&theme);
    m_hex.Paint (painter, text);

    PaintCopyIcon (painter, text, theme);

    painter.FillRect    (pl, pt, pw, ph, m_argb);
    painter.OutlineRect (pl, pt, pw, ph, borderPx, theme.ButtonBorder());

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
//  to a checkmark for a short window after a copy attempt.
//
////////////////////////////////////////////////////////////////////////////////

void ColorPickerOverlay::PaintCopyIcon (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme)
{
    HRESULT          hr        = S_OK;
    float            cl        = (float) m_copyRect.left;
    float            ct        = (float) m_copyRect.top;
    float            cw        = (float) (m_copyRect.right  - m_copyRect.left);
    float            ch        = (float) (m_copyRect.bottom - m_copyRect.top);
    float            glyphDip  = m_scaler.Pxf (s_kCopyGlyphDip);
    bool             flashing  = (m_copyFlashMs != 0) &&
                                 ((int64_t) GetTickCount64() - m_copyFlashMs < s_kCopyFlashMs);
    const wchar_t  * glyph     = flashing ? s_kpszMdl2Accept : s_kpszMdl2Copy;
    uint32_t         glyphArgb = theme.Foreground();



    if (flashing || m_copyHover)
    {
        glyphArgb = theme.Accent();
    }

    if (m_copyHover)
    {
        painter.FillRect (cl, ct, cw, ch, theme.HoverBackground());
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (glyph,
                                              cl,
                                              ct,
                                              cw,
                                              ch,
                                              glyphArgb,
                                              glyphDip,
                                              s_kpszMdl2Family,
                                              DxuiTextHAlign::Center,
                                              DxuiTextVAlign::Center,
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
    bool  hit = false;



    hit = x >= m_copyRect.left && x < m_copyRect.right &&
          y >= m_copyRect.top  && y < m_copyRect.bottom;

    return hit;
}



