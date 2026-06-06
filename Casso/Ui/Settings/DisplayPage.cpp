#include "Pch.h"

#include "DisplayPage.h"

#include "../Chrome/ChromeTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    // Match MachinePage's row spacing exactly so the two pages feel
    // consistent when the user tabs between them.
    constexpr int    s_kRowHeightDp     = 28;
    constexpr int    s_kLabelWidthDp    = 140;
    constexpr int    s_kDropdownWidthDp = 220;
    constexpr int    s_kSliderWidthDp   = 280;
    constexpr int    s_kSectionGapDp    = 14;       // gap between adjacent rows
    constexpr int    s_kBigSectionGapDp = 22;       // gap between distinct "sections"
    constexpr int    s_kPagePadDp       = 16;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::DisplayPage
//
//  Registers each member widget into the panel's child list via
//  Adopt so they participate in the IDxuiControl tree (Bounds,
//  Visible, focus, parent pointers). The widgets remain DisplayPage-
//  owned members; Adopt is non-owning. Layout positioning stays in
//  Layout() via legacy SetRect calls because the existing layout
//  code does things DxuiFormLayout cannot model (sub-row indents for
//  scanline / bloom / color-bleed children, a button sharing the
//  monitor row, indicator-column alignment past every slider).
//  SettingsPanel still drives input/paint through the bespoke shims
//  and the extended Paint() signature; collapsing the duality is
//  deferred to the SettingsPanel atomic conversion.
//
////////////////////////////////////////////////////////////////////////////////

DisplayPage::DisplayPage()
{
    Adopt (m_monitorLabel);
    Adopt (m_brightnessLabel);
    Adopt (m_contrastLabel);
    Adopt (m_gammaLabel);
    Adopt (m_persistenceLabel);
    Adopt (m_scanlinesLabel);
    Adopt (m_bloomLabel);
    Adopt (m_colorBleedLabel);
    Adopt (m_scanlinesIntLabel);
    Adopt (m_bloomRadiusLabel);
    Adopt (m_bloomStrengthLabel);
    Adopt (m_colorBleedWLabel);

    Adopt (m_monitor);
    Adopt (m_brightness);
    Adopt (m_contrast);
    Adopt (m_gamma);
    Adopt (m_persistence);
    Adopt (m_scanlinesEn);
    Adopt (m_scanlinesInt);
    Adopt (m_bloomEn);
    Adopt (m_bloomRadius);
    Adopt (m_bloomStrength);
    Adopt (m_colorBleedEn);
    Adopt (m_colorBleedW);
    Adopt (m_restore);
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
    // Brightness/contrast: slider 0..200%, 100% = identity (shader 1.0).
    // Gamma: slider 0.5..2.5 directly (with 0.1 step). 1.0 is true
    // bypass -- the shader skip-band catches values within 1% of 1.0
    // and elides the gamma pass entirely.
    // Persistence: slider 0..100% maps to shader 0..1.0.
    // Bloom radius / color bleed: slider value is pixels directly.
    // Other sliders are 0..100% (a normalized 0..1 in the shader).
    m_brightness.SetValue       (snap.brightness         * 100.0f);
    m_contrast.SetValue         (snap.contrast           * 100.0f);
    m_gamma.SetValue            (snap.gamma);
    m_persistence.SetValue      (snap.persistence        * 100.0f);
    m_scanlinesEn.SetChecked    (snap.scanlinesEnabled);
    m_scanlinesInt.SetValue     (snap.scanlinesIntensity * 100.0f);
    m_bloomEn.SetChecked        (snap.bloomEnabled);
    m_bloomRadius.SetValue      (snap.bloomRadius);                  // px direct
    m_bloomStrength.SetValue    (snap.bloomStrength      * 100.0f);
    m_colorBleedEn.SetChecked   (snap.colorBleedEnabled);
    m_colorBleedW.SetValue      (snap.colorBleedWidth);              // px direct

    // Parameter sliders are enabled iff their toggle is on.
    m_scanlinesInt.SetEnabled  (snap.scanlinesEnabled);
    m_bloomRadius.SetEnabled   (snap.bloomEnabled);
    m_bloomStrength.SetEnabled (snap.bloomEnabled);
    m_colorBleedW.SetEnabled   (snap.colorBleedEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::SetDefaultsHint
//
//  SettingsPanel passes the resolved per-control defaults whenever the
//  active monitor changes (or Restore Defaults is hit) so Paint can
//  decorate each row with "(theme default)" or "(monitor default)"
//  when the current control value matches the default.
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::SetDefaultsHint (const DisplayDefaultsHint & hint)
{
    m_hint = hint;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi          = scaler.Dpi();
    int   pad          = scaler.Px (s_kPagePadDp);
    int   rowHeight    = scaler.Px (s_kRowHeightDp);
    int   labelWidth   = scaler.Px (s_kLabelWidthDp);
    int   dropWidth    = scaler.Px (s_kDropdownWidthDp);
    int   sliderWidth  = scaler.Px (s_kSliderWidthDp);
    int   togglePillW  = scaler.Px (70);            // wide enough for "Off" / "On" text
    int   sectionGap   = scaler.Px (s_kSectionGapDp);
    int   bigGap       = scaler.Px (s_kBigSectionGapDp);
    int   childIndent  = scaler.Px (18);            // matches DxuiTreeView indent
    int   x            = rect.left + pad;
    int   y            = rect.top  + pad;
    int   controlsX    = x + labelWidth;        // every control starts here

    m_scaler = scaler;



    // Monitor row + Restore defaults button (sharing this row so the
    // button doesn't break the "all sliders aligned" promise below).
    m_monitorLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_monitorLabel.SetText (L"Monitor:");
    m_monitor.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_monitor.SetItems ({ L"Color", L"Green monochrome", L"Amber monochrome", L"White monochrome" });
    m_monitorRowRect = MakeRect (x, y, (controlsX + dropWidth) - x, rowHeight);

    {
        int  btnWidth  = scaler.Px (140);
        int  btnX      = controlsX + dropWidth + scaler.Px (16);
        m_restore.Layout   (MakeRect (btnX, y, btnWidth, rowHeight));
        m_restore.SetLabel (L"Restore defaults");
        // Title-bar caption buttons (sysButton*) are invisible at idle
        // by design; this is a content-area button and needs an at-
        // rest fill so it reads as clickable before hover. Subtle
        // grey with a brighter hover.
        m_restore.SetColors (0xFF3A4252,    // idle
                              0xFF4A5364,    // hover
                              0xFF2A3140);   // pressed
        m_restoreRowRect = MakeRect (btnX, y, btnWidth, rowHeight);
    }
    y += rowHeight + sectionGap;

    // Brightness / Contrast / Gamma -- consistent column alignment.
    m_brightnessLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_brightnessLabel.SetText (L"Brightness:");
    m_brightness.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_brightness.SetRange     (0.0f, 200.0f);
    m_brightness.SetStep      (10.0f);
    m_brightness.SetSuffix    (L"%");
    m_brightness.SetShowTicks (true);
    m_brightnessRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_contrastLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_contrastLabel.SetText (L"Contrast:");
    m_contrast.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_contrast.SetRange     (0.0f, 200.0f);
    m_contrast.SetStep      (10.0f);
    m_contrast.SetSuffix    (L"%");
    m_contrast.SetShowTicks (true);
    m_contrastRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_gammaLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_gammaLabel.SetText (L"Gamma:");
    m_gamma.SetRect           (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_gamma.SetRange          (0.5f, 2.5f);
    m_gamma.SetStep           (0.1f);
    m_gamma.SetSuffix         (L"");
    m_gamma.SetShowValue      (true);     // dimensionless; opt in to readout
    m_gamma.SetDecimalPlaces  (1);
    m_gamma.SetShowTicks      (true);
    m_gammaRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + bigGap;

    // Scanlines section: enable on the toggle row, intensity slider on
    // the next row -- both at controlsX so the slider value text aligns
    // with every other slider above and below.
    m_scanlinesLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_scanlinesLabel.SetText (L"Scanlines:");
    m_scanlinesEn.SetRect    (MakeRect (controlsX, y, togglePillW, rowHeight));
    m_scanlinesEn.SetLabel   (L"");
    m_scanlinesEnRowRect = MakeRect (x, y, (controlsX + togglePillW) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_scanlinesIntLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_scanlinesIntLabel.SetText (L"Intensity:");
    m_scanlinesInt.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_scanlinesInt.SetRange     (10.0f, 100.0f);
    m_scanlinesInt.SetStep      (10.0f);
    m_scanlinesInt.SetSuffix    (L"%");
    m_scanlinesInt.SetShowTicks (true);
    m_scanlinesIntRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + bigGap;

    // Bloom section
    m_bloomLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_bloomLabel.SetText (L"Bloom:");
    m_bloomEn.SetRect    (MakeRect (controlsX, y, togglePillW, rowHeight));
    m_bloomEn.SetLabel   (L"");
    m_bloomEnRowRect = MakeRect (x, y, (controlsX + togglePillW) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_bloomRadiusLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_bloomRadiusLabel.SetText (L"Radius:");
    m_bloomRadius.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_bloomRadius.SetRange     (1.0f, 10.0f);
    m_bloomRadius.SetStep      (1.0f);
    m_bloomRadius.SetSuffix    (L" px");
    m_bloomRadius.SetShowTicks (true);
    m_bloomRadiusRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_bloomStrengthLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_bloomStrengthLabel.SetText (L"Strength:");
    m_bloomStrength.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_bloomStrength.SetRange     (10.0f, 100.0f);
    m_bloomStrength.SetStep      (10.0f);
    m_bloomStrength.SetSuffix    (L"%");
    m_bloomStrength.SetShowTicks (true);
    m_bloomStrengthRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + bigGap;

    // Color bleed section
    m_colorBleedLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_colorBleedLabel.SetText (L"Color bleed:");
    m_colorBleedEn.SetRect    (MakeRect (controlsX, y, togglePillW, rowHeight));
    m_colorBleedEn.SetLabel   (L"");
    m_colorBleedEnRowRect = MakeRect (x, y, (controlsX + togglePillW) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_colorBleedWLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_colorBleedWLabel.SetText (L"Width:");
    m_colorBleedW.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_colorBleedW.SetRange     (1.0f, 8.0f);
    m_colorBleedW.SetStep      (1.0f);
    m_colorBleedW.SetSuffix    (L" px");
    m_colorBleedW.SetShowTicks (true);
    m_colorBleedWRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + bigGap;

    // Persistence (single slider, no enable toggle -- 0% is "off")
    m_persistenceLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_persistenceLabel.SetText (L"Persistence:");
    m_persistence.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_persistence.SetRange     (0.0f, 99.0f);
    m_persistence.SetStep      (5.0f);
    m_persistence.SetSuffix    (L"%");
    m_persistence.SetShowTicks (true);
    m_persistenceRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);

    // Indicator column starts past the slider's right edge with a fixed
    // gap. All sliders are the same width so this lands at a single x
    // across every row; toggles use the same x even though their
    // controls don't extend that far.
    m_indicatorX = controlsX + sliderWidth + scaler.Px (28);

    m_monitorLabel.SetDpi        (dpi);
    m_brightnessLabel.SetDpi     (dpi);
    m_contrastLabel.SetDpi       (dpi);
    m_gammaLabel.SetDpi          (dpi);
    m_persistenceLabel.SetDpi    (dpi);
    m_scanlinesLabel.SetDpi      (dpi);
    m_bloomLabel.SetDpi          (dpi);
    m_colorBleedLabel.SetDpi     (dpi);
    m_scanlinesIntLabel.SetDpi   (dpi);
    m_bloomRadiusLabel.SetDpi    (dpi);
    m_bloomStrengthLabel.SetDpi  (dpi);
    m_colorBleedWLabel.SetDpi    (dpi);
    m_monitor.SetDpi             (dpi);
    m_brightness.SetDpi          (dpi);
    m_contrast.SetDpi            (dpi);
    m_gamma.SetDpi               (dpi);
    m_persistence.SetDpi         (dpi);
    m_scanlinesEn.SetDpi         (dpi);
    m_scanlinesInt.SetDpi        (dpi);
    m_bloomEn.SetDpi             (dpi);
    m_bloomRadius.SetDpi         (dpi);
    m_bloomStrength.SetDpi       (dpi);
    m_colorBleedEn.SetDpi        (dpi);
    m_colorBleedW.SetDpi         (dpi);
    m_restore.SetDpi             (dpi);

    // Mirror the page's footprint into the IDxuiControl tree so future
    // centralized walks see this page as a panel covering `rect`.
    // Adopted children already have their bounds written via the
    // SetRect calls above.
    DxuiPanel::SetBounds (rect);
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

    m_gamma.SetOnChange         ([this] (float v) { if (m_onGamma)       { m_onGamma       (v); } });
    m_gamma.SetOnDragStart      ([this] { if (m_onPreview) { m_onPreview (kControlGamma,       true,  false); } });
    m_gamma.SetOnDragEnd        ([this] { if (m_onPreview) { m_onPreview (kControlGamma,       false, false); } });
    m_gamma.SetOnKeyboardChange ([this] { if (m_onPreview) { m_onPreview (kControlGamma,       true,  true);  } });

    m_persistence.SetOnChange         ([this] (float v) { if (m_onPersistence) { m_onPersistence (v); } });
    m_persistence.SetOnDragStart      ([this] { if (m_onPreview) { m_onPreview (kControlPersistence, true,  false); } });
    m_persistence.SetOnDragEnd        ([this] { if (m_onPreview) { m_onPreview (kControlPersistence, false, false); } });
    m_persistence.SetOnKeyboardChange ([this] { if (m_onPreview) { m_onPreview (kControlPersistence, true,  true);  } });

    m_restore.SetClick ([this] { if (m_onRestore) { m_onRestore(); } });

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
//  Bespoke input + focus shims (OnLButtonDown / OnLButtonUp /
//  OnMouseMove / OnMouseHover / OnKey / CollectFocusables /
//  AnyDropdownOpen) used to live here. SettingsPanel now dispatches
//  via IDxuiControl::OnMouse / OnKey through DxuiPanel auto fan-out
//  and queries m_monitor.IsOpen() directly. The extended Paint
//  overload below is still bespoke pending DisplayPage paint collapse.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::FocusedControlRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DisplayPage::FocusedControlRect (int controlId) const
{
    RECT  rect      = {};
    RECT  menuRect  = {};
    int   rowHeight = m_scaler.Px (s_kRowHeightDp);



    switch (controlId)
    {
        case kControlBrightness:    rect = m_brightnessRowRect;    break;
        case kControlContrast:      rect = m_contrastRowRect;      break;
        case kControlMonitor:       rect = m_monitorRowRect;       break;
        case kControlScanlinesInt:  rect = m_scanlinesIntRowRect;  break;
        case kControlBloomRadius:   rect = m_bloomRadiusRowRect;   break;
        case kControlBloomStrength: rect = m_bloomStrengthRowRect; break;
        case kControlColorBleedW:   rect = m_colorBleedWRowRect;   break;
        case kControlGamma:         rect = m_gammaRowRect;         break;
        case kControlPersistence:   rect = m_persistenceRowRect;   break;
        default:                    rect = {};                     break;
    }

    if (controlId == kControlMonitor && m_monitor.IsOpen())
    {
        menuRect        = m_monitor.Rect();
        menuRect.top    = menuRect.bottom;
        menuRect.bottom = menuRect.top + (int) m_monitor.Items().size() * rowHeight;
        UnionRect (&rect, &rect, &menuRect);
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::SetFadeState
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::SetFadeState (int   focusedControlId,
                                float focusedAlpha,
                                float nonFocusedAlpha)
{
    m_fadeFocusedId       = focusedControlId;
    m_fadeFocusedAlpha    = focusedAlpha;
    m_fadeNonFocusedAlpha = nonFocusedAlpha;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text,
                         const IDxuiTheme & theme)
{
    constexpr uint32_t  s_kFocusedBackingArgb = 0xFF202830;   // dark grey, near-opaque
    constexpr uint32_t  s_kIndicatorArgb      = 0xFF7A8FA5;   // muted blue-grey
    constexpr int       s_kIndicatorFontDp    = 12;
    constexpr int       s_kIndicatorWidthDp   = 140;
    constexpr wchar_t   s_kFont[]             = L"Segoe UI";
    constexpr float     s_kFloatEpsilon       = 0.001f;

    int    focusedControlId = m_fadeFocusedId;
    float  focusedAlpha     = m_fadeFocusedAlpha;
    float  nonFocusedAlpha  = m_fadeNonFocusedAlpha;
    float  indicatorFontPx  = m_scaler.Pxf (s_kIndicatorFontDp);
    float  indicatorWidthPx = m_scaler.Pxf (s_kIndicatorWidthDp);


    UNREFERENCED_PARAMETER (theme);

    auto  SetAlphaForRow = [&] (int control, const RECT & rowRect)
    {
        float  a = (control == focusedControlId) ? focusedAlpha : nonFocusedAlpha;



        (void) rowRect;
        painter.SetGlobalAlpha (a);
        text.SetGlobalAlpha    (a);
    };

    // Legacy per-row dark backing rect that used to highlight the
    // focused control during in-window-overlay preview. The new
    // owned-popup design replaces that with the compose pass's
    // blur+dim+sharp-focus pipeline -- the row backing is now both
    // redundant and visually wrong (paints over the blurred backdrop
    // as a hard-edged dark rectangle). Kept as a stub for grep-ability
    // in case we want to bring it back as an opt-in for accessibility.
    auto  PaintBackingIfFocused = [] (int /*control*/, const RECT & /*rowRect*/)
    {
    };

    auto  DrawIndicator = [&] (const RECT & rowRect, bool matchesDefault, bool themeOwned)
    {
        HRESULT          hrLocal = S_OK;
        const wchar_t *  label   = themeOwned ? L"(theme default)" : L"(monitor default)";



        if (! matchesDefault)
        {
            return;
        }
        IGNORE_RETURN_VALUE (hrLocal,
                             text.DrawString (label,
                                              (float) m_indicatorX,
                                              (float) rowRect.top,
                                              indicatorWidthPx,
                                              (float) (rowRect.bottom - rowRect.top),
                                              s_kIndicatorArgb,
                                              indicatorFontPx,
                                              s_kFont,
                                              DxuiTextRenderer::HAlign::Left,
                                              DxuiTextRenderer::VAlign::Center));
    };

    auto  FloatMatches = [&] (float a, float b)
    {
        return std::fabs (a - b) < s_kFloatEpsilon;
    };



    SetAlphaForRow (kControlMonitor, m_monitorRowRect);
    PaintBackingIfFocused (kControlMonitor, m_monitorRowRect);
    m_monitorLabel.Paint    (painter, text);
    m_monitor.PaintBase     (painter, text);

    SetAlphaForRow (kControlBrightness, m_brightnessRowRect);
    PaintBackingIfFocused (kControlBrightness, m_brightnessRowRect);
    m_brightnessLabel.Paint (painter, text);
    m_brightness.Paint      (painter, text);
    DrawIndicator (m_brightnessRowRect,
                   FloatMatches (m_brightness.Value() / 100.0f, m_hint.values.brightness),
                   m_hint.brightnessFromTheme);

    SetAlphaForRow (kControlContrast, m_contrastRowRect);
    PaintBackingIfFocused (kControlContrast, m_contrastRowRect);
    m_contrastLabel.Paint   (painter, text);
    m_contrast.Paint        (painter, text);
    DrawIndicator (m_contrastRowRect,
                   FloatMatches (m_contrast.Value() / 100.0f, m_hint.values.contrast),
                   m_hint.contrastFromTheme);

    SetAlphaForRow (kControlGamma, m_gammaRowRect);
    PaintBackingIfFocused (kControlGamma, m_gammaRowRect);
    m_gammaLabel.Paint      (painter, text);
    m_gamma.Paint           (painter, text);
    DrawIndicator (m_gammaRowRect,
                   FloatMatches (m_gamma.Value(), m_hint.values.gamma),
                   false);  // gamma is never theme-owned

    // Scanlines section: label in the left column, toggle in the value column.
    SetAlphaForRow (-1, m_scanlinesEnRowRect);
    m_scanlinesLabel.Paint (painter, text);
    m_scanlinesEn.Paint    (painter, text);
    DrawIndicator (m_scanlinesEnRowRect,
                   m_scanlinesEn.Checked() == m_hint.values.scanlinesEnabled,
                   m_hint.scanlinesFromTheme);
    SetAlphaForRow (kControlScanlinesInt, m_scanlinesIntRowRect);
    PaintBackingIfFocused (kControlScanlinesInt, m_scanlinesIntRowRect);
    m_scanlinesIntLabel.Paint (painter, text);
    m_scanlinesInt.Paint      (painter, text);
    DrawIndicator (m_scanlinesIntRowRect,
                   FloatMatches (m_scanlinesInt.Value() / 100.0f, m_hint.values.scanlinesIntensity),
                   m_hint.scanlinesFromTheme);

    // Bloom section
    SetAlphaForRow (-1, m_bloomEnRowRect);
    m_bloomLabel.Paint (painter, text);
    m_bloomEn.Paint    (painter, text);
    DrawIndicator (m_bloomEnRowRect,
                   m_bloomEn.Checked() == m_hint.values.bloomEnabled,
                   m_hint.bloomFromTheme);
    SetAlphaForRow (kControlBloomRadius, m_bloomRadiusRowRect);
    PaintBackingIfFocused (kControlBloomRadius, m_bloomRadiusRowRect);
    m_bloomRadiusLabel.Paint (painter, text);
    m_bloomRadius.Paint      (painter, text);
    DrawIndicator (m_bloomRadiusRowRect,
                   FloatMatches (m_bloomRadius.Value(), m_hint.values.bloomRadius),
                   m_hint.bloomFromTheme);
    SetAlphaForRow (kControlBloomStrength, m_bloomStrengthRowRect);
    PaintBackingIfFocused (kControlBloomStrength, m_bloomStrengthRowRect);
    m_bloomStrengthLabel.Paint (painter, text);
    m_bloomStrength.Paint      (painter, text);
    DrawIndicator (m_bloomStrengthRowRect,
                   FloatMatches (m_bloomStrength.Value() / 100.0f, m_hint.values.bloomStrength),
                   m_hint.bloomFromTheme);

    // Color-bleed section
    SetAlphaForRow (-1, m_colorBleedEnRowRect);
    m_colorBleedLabel.Paint (painter, text);
    m_colorBleedEn.Paint    (painter, text);
    DrawIndicator (m_colorBleedEnRowRect,
                   m_colorBleedEn.Checked() == m_hint.values.colorBleedEnabled,
                   m_hint.colorBleedFromTheme);
    SetAlphaForRow (kControlColorBleedW, m_colorBleedWRowRect);
    PaintBackingIfFocused (kControlColorBleedW, m_colorBleedWRowRect);
    m_colorBleedWLabel.Paint (painter, text);
    m_colorBleedW.Paint      (painter, text);
    DrawIndicator (m_colorBleedWRowRect,
                   FloatMatches (m_colorBleedW.Value(), m_hint.values.colorBleedWidth),
                   m_hint.colorBleedFromTheme);

    SetAlphaForRow (kControlPersistence, m_persistenceRowRect);
    PaintBackingIfFocused (kControlPersistence, m_persistenceRowRect);
    m_persistenceLabel.Paint (painter, text);
    m_persistence.Paint      (painter, text);
    DrawIndicator (m_persistenceRowRect,
                   FloatMatches (m_persistence.Value() / 100.0f, m_hint.values.persistence),
                   false);  // persistence is never theme-owned

    SetAlphaForRow (-1, m_restoreRowRect);
    {
        static const ChromeTheme  s_kFallbackTheme = ChromeTheme::Skeuomorphic();
        // DxuiButton consults theme tokens for color when the caller hasn't
        // set explicit override colors. DisplayPage doesn't carry a
        // theme handle so we hand it the canonical fallback -- chrome
        // theming for the button face will land when the page picks
        // up the active theme pointer in a follow-up.
        m_restore.Paint (painter, text, s_kFallbackTheme);
    }

    // DxuiDropdown menu floats above the page; paint last so it overlays.
    SetAlphaForRow (kControlMonitor, m_monitorRowRect);
    m_monitor.PaintMenu     (painter, text);

    // Restore default so the rest of the panel paints opaque.
    painter.SetGlobalAlpha (1.0f);
    text.SetGlobalAlpha    (1.0f);
}
