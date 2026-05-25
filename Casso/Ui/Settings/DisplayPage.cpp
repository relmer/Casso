#include "Pch.h"

#include "DisplayPage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kRowHeightDp     = 28;
    constexpr int    s_kLabelWidthDp    = 140;
    constexpr int    s_kDropdownWidthDp = 220;
    constexpr int    s_kSliderWidthDp   = 260;
    constexpr int    s_kSectionGapDp    = 14;
    constexpr int    s_kPagePadDp       = 16;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::SetInitialCrt
//
//  Called by the panel on Show with the baseline brightness/contrast
//  pulled from GlobalUserPrefs so the sliders open at the right spot.
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::SetInitialCrt (const GlobalUserPrefsCrtSnapshot & snap)
{
    // Brightness/contrast sliders: 0-100% where 50% = identity (shader = 1.0).
    // Map other shader values to slider percentages similarly so the UX is
    // consistent across rows.
    m_brightness.SetValue       (snap.brightness         * 50.0f);
    m_contrast.SetValue         (snap.contrast           * 50.0f);
    m_scanlinesEn.SetChecked    (snap.scanlinesEnabled);
    m_scanlinesInt.SetValue     (snap.scanlinesIntensity * 100.0f); // 0..1 -> 0..100
    m_bloomEn.SetChecked        (snap.bloomEnabled);
    m_bloomRadius.SetValue      (snap.bloomRadius        * 25.0f);  // 0..4 -> 0..100
    m_bloomStrength.SetValue    (snap.bloomStrength      * 100.0f); // 0..1 -> 0..100
    m_colorBleedEn.SetChecked   (snap.colorBleedEnabled);
    m_colorBleedW.SetValue      (snap.colorBleedWidth    * 25.0f);  // 0..4 -> 0..100

    // Parameter sliders are enabled iff their toggle is on.
    m_scanlinesInt.SetEnabled  (snap.scanlinesEnabled);
    m_bloomRadius.SetEnabled   (snap.bloomEnabled);
    m_bloomStrength.SetEnabled (snap.bloomEnabled);
    m_colorBleedW.SetEnabled   (snap.colorBleedEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::Layout (const RECT & rect, const DpiScaler & scaler)
{
    UINT  dpi         = scaler.Dpi();
    int   pad         = scaler.Px (s_kPagePadDp);
    int   rowHeight   = scaler.Px (s_kRowHeightDp);
    int   labelWidth  = scaler.Px (s_kLabelWidthDp);
    int   dropWidth   = scaler.Px (s_kDropdownWidthDp);
    int   sliderWidth = scaler.Px (s_kSliderWidthDp);
    int   sectionGap  = scaler.Px (s_kSectionGapDp);
    int   tightGap    = scaler.Px (4);
    int   x           = rect.left + pad;
    int   y           = rect.top  + pad;
    int   controlsX   = x + labelWidth;
    int   subLabelW   = scaler.Px (110);              // narrower for sub-parameters
    int   subCtrlX    = x + scaler.Px (24) + subLabelW;



    m_monitorLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_monitorLabel.SetText (L"Monitor:");
    m_monitor.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_monitor.SetItems ({ L"Color", L"Green monochrome", L"Amber monochrome", L"White monochrome" });
    m_monitorRowRect = MakeRect (x, y, (controlsX + dropWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_brightnessLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_brightnessLabel.SetText (L"Brightness:");
    m_brightness.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_brightness.SetRange     (0.0f, 100.0f);
    m_brightness.SetStep      (10.0f);
    m_brightness.SetSuffix    (L"%");
    m_brightness.SetShowTicks (true);
    m_brightnessRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_contrastLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_contrastLabel.SetText (L"Contrast:");
    m_contrast.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_contrast.SetRange     (0.0f, 100.0f);
    m_contrast.SetStep      (10.0f);
    m_contrast.SetSuffix    (L"%");
    m_contrast.SetShowTicks (true);
    m_contrastRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    // --- Scanlines section ---
    m_scanlinesEn.SetRect  (MakeRect (x, y, labelWidth + dropWidth, rowHeight));
    m_scanlinesEn.SetLabel (L"Scanlines");
    m_scanlinesEnRowRect = MakeRect (x, y, labelWidth + dropWidth, rowHeight);
    y += rowHeight + tightGap;

    m_scanlinesIntLabel.SetRect (MakeRect (x + scaler.Px (24), y, subLabelW, rowHeight));
    m_scanlinesIntLabel.SetText (L"Intensity:");
    m_scanlinesInt.SetRect      (MakeRect (subCtrlX, y, sliderWidth, rowHeight));
    m_scanlinesInt.SetRange     (0.0f, 100.0f);
    m_scanlinesInt.SetStep      (10.0f);
    m_scanlinesInt.SetSuffix    (L"%");
    m_scanlinesInt.SetShowTicks (true);
    m_scanlinesIntRowRect = MakeRect (x, y, (subCtrlX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    // --- Bloom section ---
    m_bloomEn.SetRect  (MakeRect (x, y, labelWidth + dropWidth, rowHeight));
    m_bloomEn.SetLabel (L"Bloom");
    m_bloomEnRowRect = MakeRect (x, y, labelWidth + dropWidth, rowHeight);
    y += rowHeight + tightGap;

    m_bloomRadiusLabel.SetRect (MakeRect (x + scaler.Px (24), y, subLabelW, rowHeight));
    m_bloomRadiusLabel.SetText (L"Radius:");
    m_bloomRadius.SetRect      (MakeRect (subCtrlX, y, sliderWidth, rowHeight));
    m_bloomRadius.SetRange     (0.0f, 100.0f);
    m_bloomRadius.SetStep      (10.0f);
    m_bloomRadius.SetSuffix    (L"%");
    m_bloomRadius.SetShowTicks (true);
    m_bloomRadiusRowRect = MakeRect (x, y, (subCtrlX + sliderWidth) - x, rowHeight);
    y += rowHeight + tightGap;

    m_bloomStrengthLabel.SetRect (MakeRect (x + scaler.Px (24), y, subLabelW, rowHeight));
    m_bloomStrengthLabel.SetText (L"Strength:");
    m_bloomStrength.SetRect      (MakeRect (subCtrlX, y, sliderWidth, rowHeight));
    m_bloomStrength.SetRange     (0.0f, 100.0f);
    m_bloomStrength.SetStep      (10.0f);
    m_bloomStrength.SetSuffix    (L"%");
    m_bloomStrength.SetShowTicks (true);
    m_bloomStrengthRowRect = MakeRect (x, y, (subCtrlX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    // --- Color bleed section ---
    m_colorBleedEn.SetRect  (MakeRect (x, y, labelWidth + dropWidth, rowHeight));
    m_colorBleedEn.SetLabel (L"Color bleed");
    m_colorBleedEnRowRect = MakeRect (x, y, labelWidth + dropWidth, rowHeight);
    y += rowHeight + tightGap;

    m_colorBleedWLabel.SetRect (MakeRect (x + scaler.Px (24), y, subLabelW, rowHeight));
    m_colorBleedWLabel.SetText (L"Width:");
    m_colorBleedW.SetRect      (MakeRect (subCtrlX, y, sliderWidth, rowHeight));
    m_colorBleedW.SetRange     (0.0f, 100.0f);
    m_colorBleedW.SetStep      (10.0f);
    m_colorBleedW.SetSuffix    (L"%");
    m_colorBleedW.SetShowTicks (true);
    m_colorBleedWRowRect = MakeRect (x, y, (subCtrlX + sliderWidth) - x, rowHeight);

    m_monitorLabel.SetDpi        (dpi);
    m_brightnessLabel.SetDpi     (dpi);
    m_contrastLabel.SetDpi       (dpi);
    m_scanlinesIntLabel.SetDpi   (dpi);
    m_bloomRadiusLabel.SetDpi    (dpi);
    m_bloomStrengthLabel.SetDpi  (dpi);
    m_colorBleedWLabel.SetDpi    (dpi);
    m_monitor.SetDpi             (dpi);
    m_brightness.SetDpi          (dpi);
    m_contrast.SetDpi            (dpi);
    m_scanlinesEn.SetDpi         (dpi);
    m_scanlinesInt.SetDpi        (dpi);
    m_bloomEn.SetDpi             (dpi);
    m_bloomRadius.SetDpi         (dpi);
    m_bloomStrength.SetDpi       (dpi);
    m_colorBleedEn.SetDpi        (dpi);
    m_colorBleedW.SetDpi         (dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::Rebuild
//
//  Re-sync widget visible state to the underlying SettingsPanelState
//  and wire each widget's OnChange callback back into state or the
//  panel's pending CRT staging.
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::Rebuild ()
{
    SettingsPanelState * state = m_state;


    if (state == nullptr)
    {
        return;
    }

    m_monitor.SetSelected ((int) state->Prefs().colorMode);
    m_monitor.SetSelect ([this, state] (int idx)
    {
        state->SetColorMode ((SettingsColorMode) idx);
        if (m_onMonitor)
        {
            m_onMonitor (idx);
        }
    });
    // Highlight changes (mouse hover + keyboard arrows while open) feed
    // the same live channel so the user sees the colour treatment as
    // they browse the list, not just on commit.
    m_monitor.SetOnHighlightChange ([this] (int idx)
    {
        if (m_onMonitor)
        {
            m_onMonitor (idx);
        }
    });

    m_brightness.SetOnChange ([this] (float v)
    {
        if (m_onBrightness)
        {
            m_onBrightness (v);
        }
    });
    m_contrast.SetOnChange ([this] (float v)
    {
        if (m_onContrast)
        {
            m_onContrast (v);
        }
    });

    m_brightness.SetOnDragStart      ([this] { if (m_onPreview) { m_onPreview (kControlBrightness, true,  false); } });
    m_brightness.SetOnDragEnd        ([this] { if (m_onPreview) { m_onPreview (kControlBrightness, false, false); } });
    m_brightness.SetOnKeyboardChange ([this] { if (m_onPreview) { m_onPreview (kControlBrightness, true,  true);  } });

    m_contrast.SetOnDragStart        ([this] { if (m_onPreview) { m_onPreview (kControlContrast,   true,  false); } });
    m_contrast.SetOnDragEnd          ([this] { if (m_onPreview) { m_onPreview (kControlContrast,   false, false); } });
    m_contrast.SetOnKeyboardChange   ([this] { if (m_onPreview) { m_onPreview (kControlContrast,   true,  true);  } });

    // --- Effect toggles ---------------------------------------------------
    m_scanlinesEn.SetOnChange ([this] (bool on)
    {
        m_scanlinesInt.SetEnabled (on);
        if (m_onScanlinesEn) { m_onScanlinesEn (on); }
    });
    m_bloomEn.SetOnChange ([this] (bool on)
    {
        m_bloomRadius.SetEnabled   (on);
        m_bloomStrength.SetEnabled (on);
        if (m_onBloomEn) { m_onBloomEn (on); }
    });
    m_colorBleedEn.SetOnChange ([this] (bool on)
    {
        m_colorBleedW.SetEnabled (on);
        if (m_onColorBleedEn) { m_onColorBleedEn (on); }
    });

    // --- Effect parameter sliders ----------------------------------------
    m_scanlinesInt.SetOnChange  ([this] (float v) { if (m_onScanlinesInt)  { m_onScanlinesInt  (v); } });
    m_bloomRadius.SetOnChange   ([this] (float v) { if (m_onBloomRadius)   { m_onBloomRadius   (v); } });
    m_bloomStrength.SetOnChange ([this] (float v) { if (m_onBloomStrength) { m_onBloomStrength (v); } });
    m_colorBleedW.SetOnChange   ([this] (float v) { if (m_onColorBleedW)   { m_onColorBleedW   (v); } });

    m_scanlinesInt.SetOnDragStart      ([this] { if (m_onPreview) { m_onPreview (kControlScanlinesInt,   true,  false); } });
    m_scanlinesInt.SetOnDragEnd        ([this] { if (m_onPreview) { m_onPreview (kControlScanlinesInt,   false, false); } });
    m_scanlinesInt.SetOnKeyboardChange ([this] { if (m_onPreview) { m_onPreview (kControlScanlinesInt,   true,  true);  } });

    m_bloomRadius.SetOnDragStart       ([this] { if (m_onPreview) { m_onPreview (kControlBloomRadius,    true,  false); } });
    m_bloomRadius.SetOnDragEnd         ([this] { if (m_onPreview) { m_onPreview (kControlBloomRadius,    false, false); } });
    m_bloomRadius.SetOnKeyboardChange  ([this] { if (m_onPreview) { m_onPreview (kControlBloomRadius,    true,  true);  } });

    m_bloomStrength.SetOnDragStart     ([this] { if (m_onPreview) { m_onPreview (kControlBloomStrength,  true,  false); } });
    m_bloomStrength.SetOnDragEnd       ([this] { if (m_onPreview) { m_onPreview (kControlBloomStrength,  false, false); } });
    m_bloomStrength.SetOnKeyboardChange ([this] { if (m_onPreview) { m_onPreview (kControlBloomStrength, true,  true);  } });

    m_colorBleedW.SetOnDragStart       ([this] { if (m_onPreview) { m_onPreview (kControlColorBleedW,    true,  false); } });
    m_colorBleedW.SetOnDragEnd         ([this] { if (m_onPreview) { m_onPreview (kControlColorBleedW,    false, false); } });
    m_colorBleedW.SetOnKeyboardChange  ([this] { if (m_onPreview) { m_onPreview (kControlColorBleedW,    true,  true);  } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::OnLButtonDown (int x, int y)
{
    if (m_monitor.OnLButtonDown        (x, y)) { return; }
    if (m_brightness.OnLButtonDown     (x, y)) { return; }
    if (m_contrast.OnLButtonDown       (x, y)) { return; }
    if (m_scanlinesEn.OnLButtonDown    (x, y)) { return; }
    if (m_scanlinesInt.OnLButtonDown   (x, y)) { return; }
    if (m_bloomEn.OnLButtonDown        (x, y)) { return; }
    if (m_bloomRadius.OnLButtonDown    (x, y)) { return; }
    if (m_bloomStrength.OnLButtonDown  (x, y)) { return; }
    if (m_colorBleedEn.OnLButtonDown   (x, y)) { return; }
    if (m_colorBleedW.OnLButtonDown    (x, y)) { return; }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::OnLButtonUp (int x, int y)
{
    (void) m_monitor.OnLButtonUp         (x, y);
    (void) m_brightness.OnLButtonUp      (x, y);
    (void) m_contrast.OnLButtonUp        (x, y);
    (void) m_scanlinesEn.OnLButtonUp     (x, y);
    (void) m_scanlinesInt.OnLButtonUp    (x, y);
    (void) m_bloomEn.OnLButtonUp         (x, y);
    (void) m_bloomRadius.OnLButtonUp     (x, y);
    (void) m_bloomStrength.OnLButtonUp   (x, y);
    (void) m_colorBleedEn.OnLButtonUp    (x, y);
    (void) m_colorBleedW.OnLButtonUp     (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::OnMouseMove (int x, int y)
{
    (void) m_brightness.OnMouseMove     (x, y);
    (void) m_contrast.OnMouseMove       (x, y);
    (void) m_scanlinesInt.OnMouseMove   (x, y);
    (void) m_bloomRadius.OnMouseMove    (x, y);
    (void) m_bloomStrength.OnMouseMove  (x, y);
    (void) m_colorBleedW.OnMouseMove    (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::OnMouseHover (int x, int y)
{
    m_monitor.SetMouseHover         (x, y);
    m_brightness.SetMouseHover      (x, y);
    m_contrast.SetMouseHover        (x, y);
    m_scanlinesEn.SetMouseHover     (x, y);
    m_scanlinesInt.SetMouseHover    (x, y);
    m_bloomEn.SetMouseHover         (x, y);
    m_bloomRadius.SetMouseHover     (x, y);
    m_bloomStrength.SetMouseHover   (x, y);
    m_colorBleedEn.SetMouseHover    (x, y);
    m_colorBleedW.SetMouseHover     (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DisplayPage::OnKey (WPARAM vk)
{
    if (m_monitor.HandleKey        (vk)) { return true; }
    if (m_brightness.OnKey         (vk)) { return true; }
    if (m_contrast.OnKey           (vk)) { return true; }
    if (m_scanlinesEn.OnKey        (vk)) { return true; }
    if (m_scanlinesInt.OnKey       (vk)) { return true; }
    if (m_bloomEn.OnKey            (vk)) { return true; }
    if (m_bloomRadius.OnKey        (vk)) { return true; }
    if (m_bloomStrength.OnKey      (vk)) { return true; }
    if (m_colorBleedEn.OnKey       (vk)) { return true; }
    if (m_colorBleedW.OnKey        (vk)) { return true; }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::CollectFocusables
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::CollectFocusables (std::vector<std::function<void (bool)>> & out)
{
    out.push_back ([this] (bool f) { m_monitor.SetFocused         (f); });
    out.push_back ([this] (bool f) { m_brightness.SetFocused      (f); });
    out.push_back ([this] (bool f) { m_contrast.SetFocused        (f); });
    out.push_back ([this] (bool f) { m_scanlinesEn.SetFocused     (f); });
    out.push_back ([this] (bool f) { m_scanlinesInt.SetFocused    (f); });
    out.push_back ([this] (bool f) { m_bloomEn.SetFocused         (f); });
    out.push_back ([this] (bool f) { m_bloomRadius.SetFocused     (f); });
    out.push_back ([this] (bool f) { m_bloomStrength.SetFocused   (f); });
    out.push_back ([this] (bool f) { m_colorBleedEn.SetFocused    (f); });
    out.push_back ([this] (bool f) { m_colorBleedW.SetFocused     (f); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::Paint (DxUiPainter & painter, DwriteTextRenderer & text,
                         int focusedControlId,
                         float nonFocusedAlpha,
                         float focusedAlpha) const
{
    constexpr uint32_t  s_kFocusedBackingArgb = 0xFF202830;   // dark grey, near-opaque

    auto  SetAlphaFor = [&] (int control)
    {
        float  a = (control == focusedControlId) ? focusedAlpha : nonFocusedAlpha;
        painter.SetGlobalAlpha (a);
        text.SetGlobalAlpha    (a);
    };

    auto  PaintBackingIfFocused = [&] (int control, const RECT & rowRect)
    {
        if (control == focusedControlId)
        {
            painter.FillRect ((float) rowRect.left,  (float) rowRect.top,
                              (float) (rowRect.right - rowRect.left),
                              (float) (rowRect.bottom - rowRect.top),
                              s_kFocusedBackingArgb);
        }
    };



    SetAlphaFor (kControlMonitor);
    PaintBackingIfFocused (kControlMonitor, m_monitorRowRect);
    m_monitorLabel.Paint    (painter, text);
    m_monitor.PaintBase     (painter, text);

    SetAlphaFor (kControlBrightness);
    PaintBackingIfFocused (kControlBrightness, m_brightnessRowRect);
    m_brightnessLabel.Paint (painter, text);
    m_brightness.Paint      (painter, text);

    SetAlphaFor (kControlContrast);
    PaintBackingIfFocused (kControlContrast, m_contrastRowRect);
    m_contrastLabel.Paint   (painter, text);
    m_contrast.Paint        (painter, text);

    // Scanlines section
    SetAlphaFor (-1);
    m_scanlinesEn.Paint (painter, text);
    SetAlphaFor (kControlScanlinesInt);
    PaintBackingIfFocused (kControlScanlinesInt, m_scanlinesIntRowRect);
    m_scanlinesIntLabel.Paint (painter, text);
    m_scanlinesInt.Paint      (painter, text);

    // Bloom section
    SetAlphaFor (-1);
    m_bloomEn.Paint (painter, text);
    SetAlphaFor (kControlBloomRadius);
    PaintBackingIfFocused (kControlBloomRadius, m_bloomRadiusRowRect);
    m_bloomRadiusLabel.Paint (painter, text);
    m_bloomRadius.Paint      (painter, text);
    SetAlphaFor (kControlBloomStrength);
    PaintBackingIfFocused (kControlBloomStrength, m_bloomStrengthRowRect);
    m_bloomStrengthLabel.Paint (painter, text);
    m_bloomStrength.Paint      (painter, text);

    // Color-bleed section
    SetAlphaFor (-1);
    m_colorBleedEn.Paint (painter, text);
    SetAlphaFor (kControlColorBleedW);
    PaintBackingIfFocused (kControlColorBleedW, m_colorBleedWRowRect);
    m_colorBleedWLabel.Paint (painter, text);
    m_colorBleedW.Paint      (painter, text);

    // Dropdown menu floats above the page; paint last so it overlays.
    SetAlphaFor (kControlMonitor);
    m_monitor.PaintMenu     (painter, text);

    // Restore default so the rest of the panel paints opaque.
    painter.SetGlobalAlpha (1.0f);
    text.SetGlobalAlpha    (1.0f);
}
