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

void DisplayPage::SetInitialCrt (float brightness, float contrast)
{
    // Sliders are 0-100% (step 10) for a clean UX; shader values live
    // in [0.0, 2.0] where 1.0 is identity. Map 50% slider = 1.0 shader.
    m_brightness.SetValue (brightness * 50.0f);
    m_contrast.SetValue   (contrast   * 50.0f);
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
    int   x           = rect.left + pad;
    int   y           = rect.top  + pad;
    int   controlsX   = x + labelWidth;



    m_monitorLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_monitorLabel.SetText (L"Monitor:");
    m_monitor.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_monitor.SetItems ({ L"Color", L"Green monochrome", L"Amber monochrome", L"White monochrome" });
    y += rowHeight + sectionGap;

    m_brightnessLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_brightnessLabel.SetText (L"Brightness:");
    m_brightness.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_brightness.SetRange     (0.0f, 100.0f);
    m_brightness.SetStep      (10.0f);
    m_brightness.SetSuffix    (L"%");
    m_brightness.SetShowTicks (true);
    y += rowHeight + sectionGap;

    m_contrastLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_contrastLabel.SetText (L"Contrast:");
    m_contrast.SetRect      (MakeRect (controlsX, y, sliderWidth, rowHeight));
    m_contrast.SetRange     (0.0f, 100.0f);
    m_contrast.SetStep      (10.0f);
    m_contrast.SetSuffix    (L"%");
    m_contrast.SetShowTicks (true);

    m_monitorLabel.SetDpi    (dpi);
    m_brightnessLabel.SetDpi (dpi);
    m_contrastLabel.SetDpi   (dpi);
    m_monitor.SetDpi         (dpi);
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::OnLButtonDown (int x, int y)
{
    if (m_monitor.OnLButtonDown     (x, y)) { return; }
    if (m_brightness.OnLButtonDown  (x, y)) { return; }
    if (m_contrast.OnLButtonDown    (x, y)) { return; }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::OnLButtonUp (int x, int y)
{
    (void) m_monitor.OnLButtonUp    (x, y);
    (void) m_brightness.OnLButtonUp (x, y);
    (void) m_contrast.OnLButtonUp   (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::OnMouseMove (int x, int y)
{
    (void) m_brightness.OnMouseMove (x, y);
    (void) m_contrast.OnMouseMove   (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::OnMouseHover (int x, int y)
{
    m_monitor.SetMouseHover    (x, y);
    m_brightness.SetMouseHover (x, y);
    m_contrast.SetMouseHover   (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DisplayPage::OnKey (WPARAM vk)
{
    if (m_monitor.HandleKey   (vk)) { return true; }
    if (m_brightness.OnKey    (vk)) { return true; }
    if (m_contrast.OnKey      (vk)) { return true; }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisplayPage::CollectFocusables
//
////////////////////////////////////////////////////////////////////////////////

void DisplayPage::CollectFocusables (std::vector<std::function<void (bool)>> & out)
{
    out.push_back ([this] (bool f) { m_monitor.SetFocused    (f); });
    out.push_back ([this] (bool f) { m_brightness.SetFocused (f); });
    out.push_back ([this] (bool f) { m_contrast.SetFocused   (f); });
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
    auto  SetAlphaFor = [&] (int control)
    {
        float  a = (control == focusedControlId) ? focusedAlpha : nonFocusedAlpha;
        painter.SetGlobalAlpha (a);
        text.SetGlobalAlpha    (a);
    };



    SetAlphaFor (kControlMonitor);
    m_monitorLabel.Paint    (painter, text);
    m_monitor.PaintBase     (painter, text);

    SetAlphaFor (kControlBrightness);
    m_brightnessLabel.Paint (painter, text);
    m_brightness.Paint      (painter, text);

    SetAlphaFor (kControlContrast);
    m_contrastLabel.Paint   (painter, text);
    m_contrast.Paint        (painter, text);

    // Dropdown menu floats above the page; paint last so it overlays.
    SetAlphaFor (kControlMonitor);
    m_monitor.PaintMenu     (painter, text);

    // Restore default so the rest of the panel paints opaque.
    painter.SetGlobalAlpha (1.0f);
    text.SetGlobalAlpha    (1.0f);
}
