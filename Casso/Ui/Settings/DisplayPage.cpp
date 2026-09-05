#include "Pch.h"

#include "DisplayPage.h"

#include "../Chrome/CassoTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-local helpers
//
////////////////////////////////////////////////////////////////////////////////

// Match MachinePage's row spacing exactly so the two pages feel
// consistent when the user tabs between them.
static constexpr int    s_kRowHeightDp     = 28;
static constexpr int    s_kLabelWidthDp    = 140;
static constexpr int    s_kDropdownWidthDp = 220;
static constexpr int    s_kSliderWidthDp   = 280;
static constexpr int    s_kSectionGapDp    = 14;       // gap between adjacent rows
static constexpr int    s_kBigSectionGapDp = 22;       // gap between distinct "sections"
static constexpr int    s_kPagePadDp       = 16;





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DisplayPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
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

DisplayPage::DisplayPage(std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_monitorLabel);
    Adopt (m_textColorLabel);
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
    Adopt (m_textColor);
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
//  DisplayPage::LabelForSource
//
//  The badge text for the tier that supplied a row's value, or null for a
//  row the user has adjusted.
//
//  Only defaults are labeled. A row carrying neither badge is one the user
//  set, and saying so a third time would add a label to every row on the
//  page to convey what the absence of one already does.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DisplayPage::LabelForSource (CrtSource source)
{
    const wchar_t *  label = nullptr;



    switch (source)
    {
        case CrtSource::Preset:  label = L"(monitor default)";  break;
        case CrtSource::Theme:   label = L"(theme default)";    break;
        default:                                                break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::SetDefaultsHint
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
//  Places every control on the Display page and records each row's rect for
//  hit-testing and focus.
//
//  This is a linear top-to-bottom walk with a single running y, deliberately
//  rather than a form policy. The page mixes labeled rows, indented child
//  rows under their parent toggles, and controls that share a row, and
//  expressing that in a policy would be harder to follow than the walk itself.
//
//  Every control starts at the same x. That single column is what makes the
//  sliders read as one aligned group, and it is why the Restore button SHARES
//  the monitor row instead of getting its own -- a full-width button on its
//  own line would break the alignment the eye is following.
//
//  Row rects are stored as they are assigned, so hit-testing and the focus
//  ring both work from exactly the geometry that was painted rather than
//  recomputing it and risking drift.
//
//  Child rows indent by the same amount as DxuiTreeView, so a nested toggle
//  reads as nested against the rest of the UI.
//
//  Slider ranges, steps, and suffixes are set here beside the geometry, since
//  a slider's usable width and its step count are the same design decision --
//  a range with too many steps for its width is not adjustable.
//
//  The drag step is finer than the click step throughout, so keyboard and
//  click-stepping move in readable increments while a drag stays continuous.
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT  dpi          = scaler.GetDpi();
    int   pad          = scaler.ToPx (s_kPagePadDp);
    int   rowHeight    = scaler.ToPx (s_kRowHeightDp);
    int   labelWidth   = scaler.ToPx (s_kLabelWidthDp);
    int   dropWidth    = scaler.ToPx (s_kDropdownWidthDp);
    int   sliderWidth  = scaler.ToPx (s_kSliderWidthDp);
    int   togglePillW  = scaler.ToPx (70);            // wide enough for "Off" / "On" text
    int   sectionGap   = scaler.ToPx (s_kSectionGapDp);
    int   bigGap       = scaler.ToPx (s_kBigSectionGapDp);
    int   childIndent  = scaler.ToPx (18);            // matches DxuiTreeView indent
    int   x            = rect.left + pad;
    int   y            = rect.top  + pad;
    int   controlsX    = x + labelWidth;        // every control starts here



    m_scaler = scaler;



    // Monitor row + Restore defaults button (sharing this row so the
    // button doesn't break the "all sliders aligned" promise below).
    m_monitorLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_monitorLabel.SetText (L"Picture:");
    m_monitor.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_monitor.SetItems ({ L"Color", L"Green monochrome", L"Amber monochrome", L"White monochrome" });
    m_monitorRowRect = MakeRect (x, y, (controlsX + dropWidth) - x, rowHeight);

    {
        int  btnWidth  = scaler.ToPx (140);
        int  btnX      = controlsX + dropWidth + scaler.ToPx (16);
        m_restore.Layout   (MakeRect (btnX, y, btnWidth, rowHeight));
        m_restore.SetLabel (L"Restore defaults");
        m_restoreRowRect = MakeRect (btnX, y, btnWidth, rowHeight);
    }

    y += rowHeight + sectionGap;

    // Text color (Color monitor only): a dropdown plus a swatch showing
    // the resolved color. Disabled for the monochrome monitors, whose
    // text color is fixed by the phosphor.
    m_textColorLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_textColorLabel.SetText (L"Text color:");
    m_textColor.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_textColor.SetItems ({ L"White", L"Green", L"Amber", L"Custom" });
    m_textColorRowRect = MakeRect (x, y, (controlsX + dropWidth) - x, rowHeight);

    {
        int  swatchSize = rowHeight - scaler.ToPx (8);
        int  swatchX    = controlsX + dropWidth + scaler.ToPx (12);
        int  swatchY    = y + (rowHeight - swatchSize) / 2;

        m_textColorSwatchRect = MakeRect (swatchX, swatchY, swatchSize, swatchSize);
    }

    y += rowHeight + sectionGap;

    // Brightness / Contrast / Gamma -- consistent column alignment.
    m_brightnessLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_brightnessLabel.SetText (L"Brightness:");
    m_brightness.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_brightness.SetRange     (0.0f, 200.0f);
    m_brightness.SetStep      (10.0f);
    m_brightness.SetDragStep  (1.0f);
    m_brightness.SetSuffix    (L"%");
    m_brightness.SetShowTicks (true);
    m_brightnessRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_contrastLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_contrastLabel.SetText (L"Contrast:");
    m_contrast.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_contrast.SetRange     (0.0f, 200.0f);
    m_contrast.SetStep      (10.0f);
    m_contrast.SetDragStep  (1.0f);
    m_contrast.SetSuffix    (L"%");
    m_contrast.SetShowTicks (true);
    m_contrastRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_gammaLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_gammaLabel.SetText (L"Gamma:");
    m_gamma.SetRect           (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_gamma.SetRange          (0.5f, 2.5f);
    m_gamma.SetStep           (0.1f);
    m_gamma.SetDragStep       (0.01f);
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
    m_scanlinesInt.SetDragStep  (1.0f);
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
    // Halation measures about two emulated pixels of Gaussian sigma on every
    // tube size, so the whole useful span sits under 4 and half-pixel
    // resolution is what the setting wants. A ceiling of 10 in whole steps
    // spent most of its travel past anything anyone picks and could not
    // express 2.5 at all.
    m_bloomRadius.SetRect          (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_bloomRadius.SetRange         (0.5f, 4.0f);
    m_bloomRadius.SetStep          (0.5f);
    m_bloomRadius.SetDragStep      (0.1f);
    m_bloomRadius.SetDecimalPlaces (1);
    m_bloomRadius.SetSuffix        (L" px");
    m_bloomRadius.SetShowTicks     (true);
    m_bloomRadiusRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);
    y += rowHeight + sectionGap;

    m_bloomStrengthLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_bloomStrengthLabel.SetText (L"Strength:");
    m_bloomStrength.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_bloomStrength.SetRange     (10.0f, 100.0f);
    m_bloomStrength.SetStep      (10.0f);
    m_bloomStrength.SetDragStep  (1.0f);
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
    m_persistence.SetDragStep  (1.0f);
    m_persistence.SetSuffix    (L"%");
    m_persistence.SetShowTicks (true);
    m_persistenceRowRect = MakeRect (x, y, (controlsX + sliderWidth) - x, rowHeight);

    // Indicator column starts past the slider's right edge with a fixed
    // gap. All sliders are the same width so this lands at a single x
    // across every row; toggles use the same x even though their
    // controls don't extend that far.
    m_indicatorX = controlsX + sliderWidth + scaler.ToPx (28);

    m_monitorLabel.SetDpi        (dpi);
    m_textColorLabel.SetDpi      (dpi);
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
    m_textColor.SetDpi           (dpi);
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

void DisplayPage::Rebuild()
{
    SettingsPanelState * state = m_state;



    if (state == nullptr)
    {
        return;
    }

    m_monitor.SetSelected ((int) state->GetPrefs().colorMode);
    m_monitor.SetSelect ([this, state] (int idx)
    {
        state->SetColorMode ((SettingsColorMode) idx);
        if (m_onMonitor)
        {
            m_onMonitor (idx);
        }

        RefreshTextColorEnabled();
    });
    // Highlight changes (mouse hover + keyboard arrows while open) feed
    // the same live channel so the user sees the color treatment as
    // they browse the list, not just on commit.
    m_monitor.SetOnHighlightChange ([this] (int idx)
    {
        if (m_onMonitor)
        {
            m_onMonitor (idx);
        }
    });

    m_textColor.SetSelected ((int) m_textColorMode);
    m_textColor.SetSelect ([this] (int idx)
    {
        m_textColorMode = (ColorMonitorTextMode) idx;
        if (m_onTextColor)
        {
            m_onTextColor (idx);
        }

        if (m_onTextColorCommit)
        {
            m_onTextColorCommit (idx);
        }
    });
    m_textColor.SetOnHighlightChange ([this] (int idx)
    {
        if (m_onTextColor)
        {
            m_onTextColor (idx);
        }
    });
    RefreshTextColorEnabled();

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

    m_restore.SetOnClick ([this] { if (m_onRestore) { m_onRestore(); } });

    // Effect toggles
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

    // Effect parameter sliders
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
//  DisplayPage::GetFocusedControlRect
//
//  The screen rect a focused control occupies, used by the live-preview pass
//  to keep it sharp while the rest of the page is blurred.
//
//  Rects come from the stored row geometry rather than being recomputed, so
//  the sharp region lines up exactly with what Layout placed.
//
//  An OPEN monitor dropdown extends the rect to include its menu. Without
//  that, the menu -- which is what the user is actually reading while choosing
//  -- would fall outside the sharp region and be blurred at the moment it
//  matters most.
//
//  An unrecognized control id yields an empty rect, which the caller treats as
//  "nothing focused" rather than as an error.
//
////////////////////////////////////////////////////////////////////////////////

RECT DisplayPage::GetFocusedControlRect (int controlId) const
{
    RECT  rect      = {};
    RECT  menuRect  = {};
    int   rowHeight = m_scaler.ToPx (s_kRowHeightDp);



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
        menuRect        = m_monitor.GetRect();
        menuRect.top    = menuRect.bottom;
        menuRect.bottom = menuRect.top + (int) m_monitor.GetItems().size() * rowHeight;
        UnionRect (&rect, &rect, &menuRect);
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::SetTextColor
//
//  Seeds the Text-color dropdown + swatch state from the global prefs.
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::SetTextColor (ColorMonitorTextMode mode, uint32_t customArgb)
{
    m_textColorMode       = mode;
    m_textColorCustomArgb = customArgb;
    m_textColor.SetSelected ((int) mode);
    RefreshTextColorEnabled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::IsTextColorActive
//
//  True iff the active monitor is Color -- the only mode where a custom
//  //e text color has any visible effect.
//
////////////////////////////////////////////////////////////////////////////////

bool DisplayPage::IsTextColorActive() const
{
    return m_state != nullptr && m_state->GetPrefs().colorMode == SettingsColorMode::Color;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::RefreshTextColorEnabled
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::RefreshTextColorEnabled()
{
    m_textColor.SetEnabled (IsTextColorActive());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnMouse
//
//  Adds a swatch hit-test on top of the panel's child fan-out: clicking
//  the Text-color swatch while Custom is active re-opens the picker via
//  the commit callback.
//
////////////////////////////////////////////////////////////////////////////////

bool DisplayPage::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;
    int   x       = ev.positionDip.x;
    int   y       = ev.positionDip.y;



    if (ev.kind == DxuiMouseEventKind::Down &&
        IsTextColorActive() &&
        m_textColorMode == ColorMonitorTextMode::Custom &&
        x >= m_textColorSwatchRect.left && x < m_textColorSwatchRect.right &&
        y >= m_textColorSwatchRect.top  && y < m_textColorSwatchRect.bottom)
    {
        if (m_onTextColorCommit)
        {
            m_onTextColorCommit ((int) ColorMonitorTextMode::Custom);
        }

        handled = true;
    }

    if (!handled)
    {
        handled = DxuiPanel::OnMouse (ev);
    }

    return handled;
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
//  Draws every row, with per-row alpha for the focus fade and a "matches
//  default" indicator beside each control.
//
//  Focus emphasis is done with per-row GLOBAL ALPHA rather than by changing
//  colors. Both painter and text renderer are set together before each row and
//  the whole row fades as one, so no widget needs to know about the effect and
//  a newly added control participates automatically.
//
//  The alphas come from the fade animator, not from the live focus state, so
//  moving focus eases between rows instead of snapping.
//
//  PaintBackingIfFocused is an intentional empty stub, kept for grep-ability.
//  It used to paint a dark backing behind the focused row for the in-window
//  overlay preview; the owned-popup design replaced that with the compose
//  pass's blur-dim-sharpen pipeline, against which a hard-edged dark rectangle
//  is both redundant and visually wrong. It stays as a hook in case the effect
//  is ever wanted back as an accessibility opt-in.
//
//  The default indicator distinguishes a THEME default from a MONITOR default,
//  because a value can match one and not the other, and knowing which one is
//  what tells the user whether changing themes will move it.
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text,
                         const IDxuiTheme & theme)
{
    constexpr uint32_t         s_kFocusedBackingArgb = 0xFF202830;   // dark gray, near-opaque
    constexpr int              s_kIndicatorFontDp    = 12;
    constexpr int              s_kIndicatorWidthDp   = 140;
    constexpr const wchar_t  * s_kFont               = DxuiTheme::kBodyFace;



    int    focusedControlId = m_fadeFocusedId;
    float  focusedAlpha     = m_fadeFocusedAlpha;
    float  nonFocusedAlpha  = m_fadeNonFocusedAlpha;
    float  indicatorFontPx  = m_scaler.ToPxf (s_kIndicatorFontDp);
    float  indicatorWidthPx = m_scaler.ToPxf (s_kIndicatorWidthDp);


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

    auto  DrawIndicator = [&] (const RECT & rowRect, CrtField field)
    {
        HRESULT          hrLocal = S_OK;
        const wchar_t *  label   = LabelForSource (m_hint.source[(size_t) field]);



        // No label means the user set this row. The empty indicator column
        // is what says so.
        if (label == nullptr)
        {
            return;
        }

        hrLocal = text.DrawString (label,
                                   (float) m_indicatorX,
                                   (float) rowRect.top,
                                   indicatorWidthPx,
                                   (float) (rowRect.bottom - rowRect.top),
                                   theme.ForegroundMuted(),
                                   indicatorFontPx,
                                   s_kFont,
                                   DxuiTextRenderer::HAlign::Left,
                                   DxuiTextRenderer::VAlign::Center);
        IGNORE_RETURN_VALUE (hrLocal, S_OK);
    };




    SetAlphaForRow (kControlMonitor, m_monitorRowRect);
    PaintBackingIfFocused (kControlMonitor, m_monitorRowRect);
    m_monitorLabel.Paint    (painter, text, theme);
    m_monitor.SetTheme      (&theme);
    m_monitor.PaintBase     (painter, text);

    // Text-color row: label, dropdown base, and a swatch of the resolved
    // color. The dropdown menu paints last (with the monitor menu).
    SetAlphaForRow (-1, m_textColorRowRect);
    m_textColorLabel.Paint  (painter, text, theme);
    m_textColor.SetTheme    (&theme);
    m_textColor.PaintBase   (painter, text);
    {
        uint32_t  swatchArgb = ColorUtil::ResolveColorMonitorTextArgb (m_textColorMode, m_textColorCustomArgb);
        float     sl         = (float) m_textColorSwatchRect.left;
        float     st         = (float) m_textColorSwatchRect.top;
        float     sw         = (float) (m_textColorSwatchRect.right  - m_textColorSwatchRect.left);
        float     sh         = (float) (m_textColorSwatchRect.bottom - m_textColorSwatchRect.top);

        painter.FillRect (sl, st, sw, sh, swatchArgb);
    }

    SetAlphaForRow (kControlBrightness, m_brightnessRowRect);
    PaintBackingIfFocused (kControlBrightness, m_brightnessRowRect);
    m_brightnessLabel.Paint (painter, text, theme);
    m_brightness.Paint      (painter, text, theme);
    DrawIndicator (m_brightnessRowRect, CrtField::Brightness);

    SetAlphaForRow (kControlContrast, m_contrastRowRect);
    PaintBackingIfFocused (kControlContrast, m_contrastRowRect);
    m_contrastLabel.Paint   (painter, text, theme);
    m_contrast.Paint        (painter, text, theme);
    DrawIndicator (m_contrastRowRect, CrtField::Contrast);

    SetAlphaForRow (kControlGamma, m_gammaRowRect);
    PaintBackingIfFocused (kControlGamma, m_gammaRowRect);
    m_gammaLabel.Paint      (painter, text, theme);
    m_gamma.Paint           (painter, text, theme);
    DrawIndicator (m_gammaRowRect, CrtField::Gamma);

    // Scanlines section: label in the left column, toggle in the value column.
    SetAlphaForRow (-1, m_scanlinesEnRowRect);
    m_scanlinesLabel.Paint (painter, text, theme);
    m_scanlinesEn.Paint    (painter, text, theme);
    DrawIndicator (m_scanlinesEnRowRect, CrtField::ScanlinesEnabled);
    SetAlphaForRow (kControlScanlinesInt, m_scanlinesIntRowRect);
    PaintBackingIfFocused (kControlScanlinesInt, m_scanlinesIntRowRect);
    m_scanlinesIntLabel.Paint (painter, text, theme);
    m_scanlinesInt.Paint      (painter, text, theme);
    DrawIndicator (m_scanlinesIntRowRect, CrtField::ScanlinesIntensity);

    // Bloom section
    SetAlphaForRow (-1, m_bloomEnRowRect);
    m_bloomLabel.Paint (painter, text, theme);
    m_bloomEn.Paint    (painter, text, theme);
    DrawIndicator (m_bloomEnRowRect, CrtField::BloomEnabled);
    SetAlphaForRow (kControlBloomRadius, m_bloomRadiusRowRect);
    PaintBackingIfFocused (kControlBloomRadius, m_bloomRadiusRowRect);
    m_bloomRadiusLabel.Paint (painter, text, theme);
    m_bloomRadius.Paint      (painter, text, theme);
    DrawIndicator (m_bloomRadiusRowRect, CrtField::BloomRadius);
    SetAlphaForRow (kControlBloomStrength, m_bloomStrengthRowRect);
    PaintBackingIfFocused (kControlBloomStrength, m_bloomStrengthRowRect);
    m_bloomStrengthLabel.Paint (painter, text, theme);
    m_bloomStrength.Paint      (painter, text, theme);
    DrawIndicator (m_bloomStrengthRowRect, CrtField::BloomStrength);

    // Color-bleed section
    SetAlphaForRow (-1, m_colorBleedEnRowRect);
    m_colorBleedLabel.Paint (painter, text, theme);
    m_colorBleedEn.Paint    (painter, text, theme);
    DrawIndicator (m_colorBleedEnRowRect, CrtField::ColorBleedEnabled);
    SetAlphaForRow (kControlColorBleedW, m_colorBleedWRowRect);
    PaintBackingIfFocused (kControlColorBleedW, m_colorBleedWRowRect);
    m_colorBleedWLabel.Paint (painter, text, theme);
    m_colorBleedW.Paint      (painter, text, theme);
    DrawIndicator (m_colorBleedWRowRect, CrtField::ColorBleedWidth);

    SetAlphaForRow (kControlPersistence, m_persistenceRowRect);
    PaintBackingIfFocused (kControlPersistence, m_persistenceRowRect);
    m_persistenceLabel.Paint (painter, text, theme);
    m_persistence.Paint      (painter, text, theme);
    DrawIndicator (m_persistenceRowRect, CrtField::Persistence);

    SetAlphaForRow (-1, m_restoreRowRect);
    m_restore.Paint (painter, text, theme);

    // DxuiDropdown menu floats above the page; paint last so it overlays.
    SetAlphaForRow (kControlMonitor, m_monitorRowRect);
    m_monitor.PaintMenu     (painter, text);

    // Restore default so the rest of the panel paints opaque.
    painter.SetGlobalAlpha (1.0f);
    text.SetGlobalAlpha    (1.0f);
    m_textColor.PaintMenu   (painter, text);
}

